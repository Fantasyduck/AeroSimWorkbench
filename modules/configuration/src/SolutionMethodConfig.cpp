#include "aerosim/configuration/SolutionMethodConfig.h"
#include <cmath>
namespace aerosim {
namespace {
struct Field { const char *key; const char *tag; QStringList tokens; };
const QVector<Field> fields = {
    {"spatialScheme", "SPADIS", {"roe", "vanleer", "ausm", "ausmpw-plus", "hllc"}},
    {"limiter", "FLOWLIM", {"none", "vanleer", "vanalbada", "hvanleer", "hvanalbada"}},
    {"timeScheme", "TIMEDIS", {"bdf1", "bdf2", "optimized-bdf2", "bdf3", "modified-bdf4", "ssprk3"}},
    {"entropyFix", "adptv_entropy_fix", {}}, {"strongStability", "linear_projection", {}},
    {"cflPointCount", "CFL_VAL", {}}, {"timeStep", "time_step", {}}, {"subIterations", "subiterations", {}}
};
bool fail(QString *error, const QString &key) {
    if (error) *error = QString::fromUtf16(u"求解方法字段重复、编码错误或超出范围：") + key;
    return false;
}
void put(ParameterCategoryConfig &config, const QString &key, const ParameterValue &value) {
    for (auto &entry : config.entries) if (entry.key == key) { entry.value = value; return; }
    ParameterEntry entry; entry.key = key; entry.value = value; config.entries.append(entry);
}
QString pointKey(int i, const QString &field) { return "cflPoints/" + QString::number(i) + "/" + field; }
QDomElement scalar(QDomElement parent, const QString &tag, bool *ok, QString *error) {
    const auto element = parent.firstChildElement(tag);
    if (!element.nextSiblingElement(tag).isNull() || !element.firstChildElement().isNull()) {
        *ok = false; fail(error, tag);
    }
    return element;
}
void write(QDomDocument &xml, QDomElement parent, const QString &tag, const QString &value) {
    auto element = parent.firstChildElement(tag);
    if (element.isNull()) { element = xml.createElement(tag); parent.appendChild(element); }
    for (auto node = element.firstChild(); !node.isNull();) {
        const auto next = node.nextSibling();
        if (node.isText() || node.isCDATASection()) element.removeChild(node);
        node = next;
    }
    element.appendChild(xml.createTextNode(value));
}
}
ParameterCategoryConfig defaultSolutionMethod() {
    ParameterCategoryConfig c; c.category = ParameterCategory::SolutionMethod;
    put(c, "spatialScheme", ParameterValue::enumeration("hllc"));
    put(c, "limiter", ParameterValue::enumeration("vanleer"));
    put(c, "timeScheme", ParameterValue::enumeration("bdf1"));
    put(c, "entropyFix", ParameterValue::boolean(false));
    put(c, "strongStability", ParameterValue::boolean(false));
    put(c, "timeStep", ParameterValue::real(0.001, "s"));
    put(c, "subIterations", ParameterValue::integer(10));
    put(c, "cflPointCount", ParameterValue::integer(2));
    for (int i = 0; i < 2; ++i) {
        put(c, pointKey(i, "iteration"), ParameterValue::integer(i == 0 ? 1 : 2000));
        put(c, pointKey(i, "cfl"), ParameterValue::real(i == 0 ? 0.0 : 5.0));
    }
    return c;
}
bool validateSolutionMethod(const ParameterCategoryConfig &c, QString *error) {
    if (c.category != ParameterCategory::SolutionMethod || !c.isValid()) return fail(error, "SolutionMethod");
    const auto defaults = defaultSolutionMethod();
    for (const auto &field : fields) {
        const auto *e = c.find(field.key);
        if (!e || e->value.kind != defaults.find(field.key)->value.kind) return fail(error, field.key);
        if (!field.tokens.isEmpty() && !field.tokens.contains(e->value.textValue)) return fail(error, field.key);
    }
    const auto count = c.find("cflPointCount")->value.integerValue;
    const auto sub = c.find("subIterations")->value.integerValue;
    const auto step = c.find("timeStep")->value.realValue;
    if (count < 1 || count > 1000 || sub < 1 || sub > 1000000000 || !std::isfinite(step)
        || step < 0.00000001 || step > 1e12 || c.entries.size() != 8 + 2 * count) return fail(error, "range/count");
    for (int i = 0; i < count; ++i) {
        const auto *iteration = c.find(pointKey(i, "iteration")), *cfl = c.find(pointKey(i, "cfl"));
        if (!iteration || !cfl || iteration->value.kind != ParameterValueKind::Integer
            || cfl->value.kind != ParameterValueKind::Real || iteration->value.integerValue < 1
            || iteration->value.integerValue > 1000000000 || !std::isfinite(cfl->value.realValue)
            || cfl->value.realValue < 0 || cfl->value.realValue > 1e12) return fail(error, pointKey(i, "iteration/cfl"));
    }
    return true;
}
bool readSolutionMethod(const QDomDocument &xml, ParameterCategoryConfig *config, QStringList *warnings, QString *error) {
    auto c = defaultSolutionMethod();
    const auto root = xml.documentElement();
    const auto parent = root.firstChildElement("SolutionMethodData");
    if (!parent.nextSiblingElement("SolutionMethodData").isNull()) return fail(error, "SolutionMethodData");
    for (const auto &field : fields) {
        bool ok = true; const auto e = scalar(parent, field.tag, &ok, error);
        if (!ok) return false;
        if (e.isNull()) { warnings->append(QString::fromUtf16(u"缺少求解方法字段，使用默认值：") + field.tag); continue; }
        const auto text = e.text().trimmed(); const auto kind = c.find(field.key)->value.kind;
        ParameterValue value;
        if (!field.tokens.isEmpty()) {
            const int index = text.toInt(&ok);
            if (!ok || index < 0 || index >= field.tokens.size()) return fail(error, field.tag);
            value = ParameterValue::enumeration(field.tokens[index]);
        } else if (kind == ParameterValueKind::Boolean) {
            if (text != "0" && text != "1") return fail(error, field.tag);
            value = ParameterValue::boolean(text == "1");
        } else if (kind == ParameterValueKind::Real) value = ParameterValue::real(text.toDouble(&ok), "s");
        else value = ParameterValue::integer(text.toLongLong(&ok));
        if (!ok) return fail(error, field.tag);
        put(c, field.key, value);
    }
    bool ok = true;
    const auto iterations = scalar(parent, "lstLn1", &ok, error), values = scalar(parent, "lstLn2", &ok, error);
    if (!ok) return false;
    const auto count = c.find("cflPointCount")->value.integerValue;
    if (count < 1 || count > 1000) return fail(error, "CFL_VAL");
    if (iterations.isNull() != values.isNull()) return fail(error, "lstLn1/lstLn2");
    if (iterations.isNull()) {
        warnings->append(QString::fromUtf16(u"缺少 CFL 表格，使用默认值。"));
        for (int i = 2; i < count; ++i) {
            put(c, pointKey(i, "iteration"), ParameterValue::integer(2000 + i - 1));
            put(c, pointKey(i, "cfl"), ParameterValue::real(0));
        }
    } else {
        const auto rows = iterations.text().split(',', QString::KeepEmptyParts), cols = values.text().split(',', QString::KeepEmptyParts);
        if (rows.size() != count || cols.size() != count) return fail(error, "CFL_VAL/lstLn1/lstLn2");
        for (int i = 0; i < count; ++i) {
            const auto n = rows[i].trimmed().toLongLong(&ok);
            if (!ok) return fail(error, "lstLn1");
            put(c, pointKey(i, "iteration"), ParameterValue::integer(n));
            const auto value = cols[i].trimmed().toDouble(&ok);
            if (!ok) return fail(error, "lstLn2");
            put(c, pointKey(i, "cfl"), ParameterValue::real(value));
        }
    }
    for (int i = c.entries.size() - 1; i >= 0; --i)
        if (c.entries[i].key.startsWith("cflPoints/") && c.entries[i].key.section('/', 1, 1).toInt() >= count) c.entries.removeAt(i);
    if (!validateSolutionMethod(c, error)) return false;
    *config = c; return true;
}
void writeSolutionMethod(QDomDocument &xml, const ParameterCategoryConfig &c) {
    auto parent = xml.documentElement().firstChildElement("SolutionMethodData");
    if (parent.isNull()) { parent = xml.createElement("SolutionMethodData"); xml.documentElement().appendChild(parent); }
    for (const auto &field : fields) {
        const auto &v = c.find(field.key)->value; QString text;
        if (!field.tokens.isEmpty()) text = QString::number(field.tokens.indexOf(v.textValue));
        else if (v.kind == ParameterValueKind::Boolean) text = v.booleanValue ? "1" : "0";
        else if (v.kind == ParameterValueKind::Real) text = QString::number(v.realValue, 'g', 17);
        else text = QString::number(v.integerValue);
        write(xml, parent, field.tag, text);
    }
    QStringList iterations, values;
    for (int i = 0; i < c.find("cflPointCount")->value.integerValue; ++i) {
        iterations.append(QString::number(c.find(pointKey(i, "iteration"))->value.integerValue));
        values.append(QString::number(c.find(pointKey(i, "cfl"))->value.realValue, 'g', 17));
    }
    write(xml, parent, "lstLn1", iterations.join(',')); write(xml, parent, "lstLn2", values.join(','));
}
}
