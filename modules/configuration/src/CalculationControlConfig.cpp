#include "aerosim/configuration/CalculationControlConfig.h"

namespace aerosim {
namespace {
struct Field {
    const char *key;
    const char *path;
    ParameterValue initial;
    double minimum;
    double maximum;
};
const QVector<Field> &fields()
{
    static const QVector<Field> values = {
        {"firstOrderIterations", "RegionBlkMG/BasicRegion/_reg3/FOIS", ParameterValue::integer(2000), 0, 1e9},
        {"maximumIterations", "RegionBlkMG/BasicRegion/_reg3/NITSCG", ParameterValue::integer(30000), 1, 1e9},
        {"convergenceResidual", "RegionBlkMG/BasicRegion/_reg3/dABS_RES", ParameterValue::real(-10), -1e9, 1e9},
        {"residualMonitorThreshold", "RegionBlkMG/BasicRegion/_reg3/rms_limit_upper", ParameterValue::real(100), 0, 1e9},
        {"useGpu", "RegionBlkMG/BasicRegion/_reg3/ISGPU", ParameterValue::boolean(false), 0, 1},
        {"processCount", "Geom_Type_Dat/_nProcess", ParameterValue::integer(4), 1, 65536},
        {"gpuCount", "RegionBlkMG/BasicRegion/_reg3/GPUNUM", ParameterValue::integer(1), 1, 65536}
    };
    return values;
}
const ParameterEntry *find(const SolverConfigPayload &config, const QString &key)
{
    for (const auto &entry : config.entries) if (entry.key == key) return &entry;
    return nullptr;
}
bool fail(QString *error, const QString &path)
{
    if (error) *error = QString::fromUtf16(u"计算控制字段缺失、重复、类型错误或超出当前支持范围：") + path;
    return false;
}
QDomElement elementAt(QDomElement root, const QString &path, QString *error, bool *ok)
{
    for (const auto &tag : path.split('/')) {
        auto child = root.firstChildElement(tag);
        if (!child.isNull() && !child.nextSiblingElement(tag).isNull()) {
            *ok = false;
            fail(error, path);
            return {};
        }
        root = child;
        if (root.isNull()) return {};
    }
    return root;
}
}

SolverConfigPayload defaultCalculationControl()
{
    SolverConfigPayload config;
    config.solverId = "xcfd";
    config.schemaId = "xcfd.calculation-control";
    config.schemaVersion = 1;
    for (const auto &field : fields()) {
        ParameterEntry entry;
        entry.key = field.key;
        entry.value = field.initial;
        entry.required = true;
        entry.range = ParameterRange::between(field.minimum, field.maximum);
        config.entries.append(entry);
    }
    return config;
}

bool validateCalculationControl(const SolverConfigPayload &config, QString *error)
{
    if (!config.isValid() || config.solverId != "xcfd"
        || config.schemaId != "xcfd.calculation-control" || config.schemaVersion != 1)
        return fail(error, "xcfd.calculation-control");
    for (const auto &field : fields()) {
        const auto *entry = find(config, field.key);
        if (!entry || entry->value.kind != field.initial.kind || !entry->value.isValid())
            return fail(error, field.path);
        if (field.initial.isNumeric()) {
            const double value = entry->value.kind == ParameterValueKind::Real
                ? entry->value.realValue : double(entry->value.integerValue);
            if (value < field.minimum || value > field.maximum) return fail(error, field.path);
        }
    }
    return true;
}

bool readCalculationControl(const QDomDocument &xml, SolverConfigPayload *config,
                            QStringList *warnings, QString *error)
{
    auto candidate = defaultCalculationControl();
    int i = 0;
    for (const auto &field : fields()) {
        bool ok = true;
        const auto element = elementAt(xml.documentElement(), field.path, error, &ok);
        if (!ok) return false;
        if (element.isNull()) {
            warnings->append(QString::fromUtf16(u"缺少计算控制字段，使用默认值：") + field.path);
            ++i;
            continue;
        }
        if (!element.firstChildElement().isNull()) return fail(error, field.path);
        const auto text = element.text().trimmed();
        ParameterValue value;
        if (field.initial.kind == ParameterValueKind::Real) {
            value = ParameterValue::real(text.toDouble(&ok));
        } else {
            const auto number = text.toLongLong(&ok);
            if (field.initial.kind == ParameterValueKind::Boolean) {
                if (number != 0 && number != 1) ok = false;
                value = ParameterValue::boolean(number == 1);
            } else value = ParameterValue::integer(number);
        }
        if (!ok) return fail(error, field.path);
        candidate.entries[i++].value = value;
    }
    if (!validateCalculationControl(candidate, error)) return false;
    *config = candidate;
    return true;
}

void writeCalculationControl(QDomDocument &xml, const SolverConfigPayload &config)
{
    for (const auto &field : fields()) {
        auto element = xml.documentElement();
        for (const auto &tag : QString(field.path).split('/')) {
            auto child = element.firstChildElement(tag);
            if (child.isNull()) {
                child = xml.createElement(tag);
                element.appendChild(child);
            }
            element = child;
        }
        for (auto node = element.firstChild(); !node.isNull();) {
            auto next = node.nextSibling();
            if (node.isText() || node.isCDATASection()) element.removeChild(node);
            node = next;
        }
        const auto &value = find(config, field.key)->value;
        QString text;
        if (value.kind == ParameterValueKind::Real) text = QString::number(value.realValue, 'g', 17);
        else if (value.kind == ParameterValueKind::Boolean) text = value.booleanValue ? "1" : "0";
        else text = QString::number(value.integerValue);
        element.appendChild(xml.createTextNode(text));
    }
}
}
