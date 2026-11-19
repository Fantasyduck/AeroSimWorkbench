#include "aerosim/configuration/XcfProjectDocument.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <cmath>

namespace aerosim {
namespace {
template <size_t N> QString localized(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}
const QStringList models = {"sa", "kw-menter", "des", "ddes", "wilcox-kw1998", "wilcox-kw2006"};
const QStringList collisions = {"vss", "hs", "vhs"};
struct Field { const char *tag; const char *key; QStringList tokens; };
const QVector<Field> fields = {
    {"ISSTEADY", "calculationType", {"steady", "unsteady"}},
    {"EQUATION_TYPE", "equation", {"navier-stokes", "euler"}},
    {"TURB_MODEL", "flowRegime", {"laminar", "turbulent", "inviscid"}},
    {"Considering_rarefied_gas", "rarefiedGas", {}},
    {"ZimoxingTYPE", "collisionSubmodel", {"standard-bgk", "es-bgk"}},
    {"NusenShu", "knudsenNumber", {}},
    {"LailiuLizi", "freestreamParticleCount", {}},
    {"WanggeLizi", "gridParticleCount", {}},
    {"PengZhuang", "collisionModel", collisions}
};
bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}
void put(ParameterCategoryConfig &config, const QString &key, const ParameterValue &value)
{
    for (auto &entry : config.entries) {
        if (entry.key == key) { entry.value = value; return; }
    }
    ParameterEntry entry;
    entry.key = key;
    entry.value = value;
    config.entries.append(entry);
}
QString token(const ParameterCategoryConfig &config, const QString &key)
{
    const auto *entry = config.find(key);
    return entry ? entry->value.textValue : QString();
}
QDomElement ensure(QDomDocument &doc, QDomElement parent, const QString &tag)
{
    auto element = parent.firstChildElement(tag);
    if (element.isNull()) {
        element = doc.createElement(tag);
        parent.appendChild(element);
    }
    return element;
}
void write(QDomDocument &doc, QDomElement parent, const QString &tag, const QString &value)
{
    auto element = ensure(doc, parent, tag);
    // Retain comments/attributes rather than replacing the whole element.
    for (auto node = element.firstChild(); !node.isNull();) {
        const auto next = node.nextSibling();
        if (node.isText() || node.isCDATASection()) element.removeChild(node);
        node = next;
    }
    element.appendChild(doc.createTextNode(value));
}
bool validate(const ParameterCategoryConfig &config, QString *error)
{
    if (config.category != ParameterCategory::GoverningEquations || !config.isValid())
        return fail(error, localized(u"\u65e0\u6548\u7684\u63a7\u5236\u65b9\u7a0b\u53c2\u6570\u7ed3\u6784\u3002"));
    const auto defaults = XcfProjectDocument::defaultGoverningEquations();
    for (const auto &expected : defaults.entries) {
        const auto *entry = config.find(expected.key);
        if (!entry || entry->value.kind != expected.value.kind)
            return fail(error, localized(u"\u53c2\u6570\u7f3a\u5931\u6216\u7c7b\u578b\u9519\u8bef\uff1a") + expected.key);
    }
    for (const auto &field : fields) {
        if (!field.tokens.isEmpty() && !field.tokens.contains(token(config, field.key)))
            return fail(error, localized(u"\u4e0d\u652f\u6301\u7684\u53c2\u6570\u9009\u9879\uff1a") + field.key);
    }
    if (!models.contains(token(config, "turbulenceModel")) || token(config, "collisionEquation") != "bgk")
        return fail(error, localized(u"\u4e0d\u652f\u6301\u7684\u6e4d\u6d41\u6a21\u578b\u6216\u78b0\u649e\u65b9\u7a0b\u3002"));
    const bool euler = token(config, "equation") == "euler";
    if (euler != (token(config, "flowRegime") == "inviscid"))
        return fail(error, localized(u"\u6c42\u89e3\u65b9\u7a0b\u4e0e\u6d41\u6001\u4e0d\u4e00\u81f4\u3002"));
    const double kn = config.find("knudsenNumber")->value.realValue;
    if (!std::isfinite(kn) || kn < 0.000001 || kn > 1e9)
        return fail(error, localized(u"Kn \u8d85\u51fa\u5f53\u524d\u754c\u9762\u652f\u6301\u8303\u56f4 [0.000001, 1e9]\u3002"));
    for (const auto &key : {"freestreamParticleCount", "gridParticleCount"}) {
        const auto count = config.find(key)->value.integerValue;
        if (count < 1 || count > 1000000000)
            return fail(error, localized(u"\u7c92\u5b50\u53c2\u8003\u6570\u8d85\u51fa\u5f53\u524d\u754c\u9762\u652f\u6301\u8303\u56f4\uff1a") + key);
    }
    return true;
}
bool uniqueChildren(QDomElement parent, const QString &tag, QString *error)
{
    const auto first = parent.firstChildElement(tag);
    return first.isNull() || first.nextSiblingElement(tag).isNull()
        || fail(error, localized(u"XCF \u5b58\u5728\u91cd\u590d\u8282\u70b9\uff1a") + parent.tagName() + "/" + tag);
}
}

ParameterCategoryConfig XcfProjectDocument::defaultGoverningEquations()
{
    ParameterCategoryConfig config;
    put(config, "calculationType", ParameterValue::enumeration("steady"));
    put(config, "equation", ParameterValue::enumeration("navier-stokes"));
    put(config, "flowRegime", ParameterValue::enumeration("turbulent"));
    put(config, "turbulenceModel", ParameterValue::enumeration("sa"));
    put(config, "rarefiedGas", ParameterValue::boolean(true));
    put(config, "collisionEquation", ParameterValue::enumeration("bgk"));
    put(config, "collisionSubmodel", ParameterValue::enumeration("standard-bgk"));
    put(config, "knudsenNumber", ParameterValue::real(1.0));
    put(config, "freestreamParticleCount", ParameterValue::integer(100));
    put(config, "gridParticleCount", ParameterValue::integer(2));
    put(config, "collisionModel", ParameterValue::enumeration("vss"));
    return config;
}

XcfProjectDocument::XcfProjectDocument() : equations_(defaultGoverningEquations()), calculation_(defaultCalculationControl()), output_(defaultOutputControl()), solution_(defaultSolutionMethod())
{
    xml_ = QDomDocument(QDomImplementation().createDocumentType("XCFD_ROOT", QString(), QString()));
    xml_.appendChild(xml_.createElement("DATA_ROOT"));
}

bool XcfProjectDocument::setGoverningEquations(const ParameterCategoryConfig &config, QString *error)
{
    if (error) error->clear();
    if (!validate(config, error)) return false;
    if (token(config, "turbulenceModel") != token(equations_, "turbulenceModel")
        || token(config, "flowRegime") == "turbulent") preserveInactiveTurbulence_ = false;
    for (const auto &entry : config.entries) {
        const auto *previous = equations_.find(entry.key);
        if (!previous || entry.value.kind != previous->value.kind
            || entry.value.booleanValue != previous->value.booleanValue
            || entry.value.integerValue != previous->value.integerValue
            || entry.value.realValue != previous->value.realValue
            || entry.value.textValue != previous->value.textValue) modified_ = true;
    }
    equations_ = config;
    return true;
}

bool XcfProjectDocument::setCalculationControl(const SolverConfigPayload &config, QString *error)
{
    if (error) error->clear();
    if (!validateCalculationControl(config, error)) return false;
    for (int i = 0; i < config.entries.size(); ++i) {
        const auto &entry = config.entries[i];
        const ParameterEntry *previous = nullptr;
        for (const auto &item : calculation_.entries) if (item.key == entry.key) previous = &item;
        if (!previous || entry.value.booleanValue != previous->value.booleanValue
            || entry.value.integerValue != previous->value.integerValue
            || entry.value.realValue != previous->value.realValue) modified_ = true;
    }
    calculation_ = config;
    return true;
}

bool XcfProjectDocument::setOutputControl(const ParameterCategoryConfig &config, QString *error)
{
    if (error) error->clear();
    if (!validateOutputControl(config, error)) return false;
    for (const auto &entry : config.entries) {
        const auto *previous = output_.find(entry.key);
        if (!previous || entry.value.integerValue != previous->value.integerValue
            || entry.value.booleanValue != previous->value.booleanValue) modified_ = true;
    }
    for (const auto &entry : output_.entries) if (!config.find(entry.key)) modified_ = true;
    output_ = config; return true;
}

bool XcfProjectDocument::setSolutionMethod(const ParameterCategoryConfig &config, QString *error)
{
    if (error) error->clear();
    if (!validateSolutionMethod(config, error)) return false;
    if (config.entries.size() != solution_.entries.size()) modified_ = true;
    for (const auto &entry : config.entries) {
        const auto *previous = solution_.find(entry.key);
        if (!previous || entry.value.kind != previous->value.kind
            || entry.value.integerValue != previous->value.integerValue
            || entry.value.realValue != previous->value.realValue
            || entry.value.booleanValue != previous->value.booleanValue
            || entry.value.textValue != previous->value.textValue) modified_ = true;
    }
    solution_ = config; return true;
}

bool XcfProjectDocument::load(const QString &path, QString *error)
{
    if (error) error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    XcfProjectDocument candidate;
    QString parseError;
    int line = 0, column = 0;
    if (!candidate.xml_.setContent(&file, false, &parseError, &line, &column))
        return fail(error, localized(u"XML \u89e3\u6790\u5931\u8d25\uff08%1:%2\uff09\uff1a%3").arg(line).arg(column).arg(parseError));
    const auto root = candidate.xml_.documentElement();
    if (root.tagName() != "DATA_ROOT") return fail(error, localized(u"\u4e0d\u662f\u652f\u6301\u7684 XCF \u5de5\u7a0b\uff1a\u7f3a\u5c11 DATA_ROOT\u3002"));
    if (!uniqueChildren(root, "EquationSettingData", error) || !uniqueChildren(root, "TurbModelData", error)) return false;
    const auto equation = root.firstChildElement("EquationSettingData");
    if (equation.isNull()) return fail(error, localized(u"XCF \u7f3a\u5c11 EquationSettingData\u3002"));
    for (const auto &field : fields) {
        if (!uniqueChildren(equation, field.tag, error)) return false;
        const auto element = equation.firstChildElement(field.tag);
        if (element.isNull()) {
            candidate.warnings_ << localized(u"\u7f3a\u5c11 %1/%2\uff0c\u91c7\u7528\u5de5\u4f5c\u53f0\u9ed8\u8ba4\u503c\u3002").arg(equation.tagName(), field.tag);
            continue;
        }
        if (!element.firstChildElement().isNull()) return fail(error, localized(u"\u53c2\u6570\u5e94\u4e3a\u6807\u91cf\uff1a") + field.tag);
        bool ok = false;
        const QString text = element.text().trimmed();
        ParameterValue value;
        if (!field.tokens.isEmpty()) {
            const int index = text.toInt(&ok);
            if (!ok || index < 0 || index >= field.tokens.size())
                return fail(error, localized(u"\u4e0d\u652f\u6301\u7684 XCF \u7f16\u7801\uff1a%1/%2=%3").arg(equation.tagName(), field.tag, text));
            value = ParameterValue::enumeration(field.tokens[index]);
        } else if (QString(field.key) == "rarefiedGas") {
            const int flag = text.toInt(&ok);
            if (!ok || (flag != 0 && flag != 1)) return fail(error, localized(u"\u65e0\u6548\u7684\u7a00\u8584\u6c14\u4f53\u5f00\u5173\uff1a") + text);
            value = ParameterValue::boolean(flag == 1);
        } else if (QString(field.key) == "knudsenNumber") {
            const double number = text.toDouble(&ok);
            if (!ok) return fail(error, localized(u"\u65e0\u6548\u7684 Kn\uff1a") + text);
            value = ParameterValue::real(number);
        } else {
            const qint64 number = text.toLongLong(&ok);
            if (!ok) return fail(error, localized(u"\u65e0\u6548\u7684\u7c92\u5b50\u6570\uff1a") + text);
            value = ParameterValue::integer(number);
        }
        put(candidate.equations_, field.key, value);
    }
    const auto turbulence = root.firstChildElement("TurbModelData");
    if (!uniqueChildren(turbulence, "TURB_MODEL", error)) return false;
    const auto model = turbulence.firstChildElement("TURB_MODEL");
    if (!model.isNull()) {
        bool ok = false;
        const int index = model.text().trimmed().toInt(&ok);
        if (ok && index >= 0 && index < models.size()) {
            put(candidate.equations_, "turbulenceModel", ParameterValue::enumeration(models[index]));
        } else if (token(candidate.equations_, "flowRegime") != "turbulent") {
            candidate.preserveInactiveTurbulence_ = true;
            candidate.warnings_ << localized(u"\u4fdd\u7559\u672a\u542f\u7528\u7684\u672a\u77e5\u6e4d\u6d41\u6a21\u578b\u7f16\u7801 %1\uff1b\u754c\u9762\u6682\u663e\u793a S-A\uff0c\u542f\u7528\u6e4d\u6d41\u540e\u5c06\u91c7\u7528\u754c\u9762\u9009\u62e9\u3002").arg(model.text());
        } else return fail(error, localized(u"\u4e0d\u652f\u6301\u7684 TurbModelData/TURB_MODEL\uff1a") + model.text());
    } else candidate.warnings_ << localized(u"\u7f3a\u5c11 TurbModelData/TURB_MODEL\uff0c\u91c7\u7528 S-A\u3002");
    if (!validate(candidate.equations_, error)) return false;
    if (!readCalculationControl(candidate.xml_, &candidate.calculation_, &candidate.warnings_, error)) return false;
    if (!readOutputControl(candidate.xml_, &candidate.output_, &candidate.warnings_, error)) return false;
    if (!readSolutionMethod(candidate.xml_, &candidate.solution_, &candidate.warnings_, error)) return false;
    candidate.filePath_ = QFileInfo(path).absoluteFilePath();
    *this = candidate;
    return true;
}

bool XcfProjectDocument::save(const QString &path, QString *error)
{
    if (error) error->clear();
    if (!validate(equations_, error)) return false;
    if (!validateCalculationControl(calculation_, error)) return false;
    if (!validateOutputControl(output_, error)) return false;
    if (!validateSolutionMethod(solution_, error)) return false;
    // QDomDocument copies are shared: clone before changing the retained source.
    auto output = xml_.cloneNode(true).toDocument();
    const auto root = output.documentElement();
    auto equation = ensure(output, root, "EquationSettingData");
    for (const auto &field : fields) {
        const auto &value = equations_.find(field.key)->value;
        QString text;
        if (!field.tokens.isEmpty()) text = QString::number(field.tokens.indexOf(value.textValue));
        else if (value.kind == ParameterValueKind::Boolean) text = value.booleanValue ? "1" : "0";
        else if (value.kind == ParameterValueKind::Real) text = QString::number(value.realValue, 'g', 17);
        else text = QString::number(value.integerValue);
        write(output, equation, field.tag, text);
    }
    if (!preserveInactiveTurbulence_)
        write(output, ensure(output, root, "TurbModelData"), "TURB_MODEL",
              QString::number(models.indexOf(token(equations_, "turbulenceModel"))));
    writeCalculationControl(output, calculation_);
    writeOutputControl(output, output_);
    writeSolutionMethod(output, solution_);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    const auto bytes = output.toByteArray(1);
    if (file.write(bytes) != bytes.size()) return fail(error, file.errorString());
    if (!file.commit()) return fail(error, file.errorString());
    xml_ = output;
    filePath_ = QFileInfo(path).absoluteFilePath();
    modified_ = false;
    return true;
}
}
