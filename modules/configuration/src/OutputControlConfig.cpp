#include "aerosim/configuration/OutputControlConfig.h"

namespace aerosim {
namespace {
const QString boundaryPrefix = "boundaryHistory/";
void add(ParameterCategoryConfig &config, const QString &key, const ParameterValue &value)
{
    ParameterEntry entry; entry.key = key; entry.value = value;
    config.entries.append(entry);
}
bool fail(QString *error, const QString &path)
{
    if (error) *error = QString::fromUtf16(u"输出控制字段重复、编码错误或超出当前范围：") + path;
    return false;
}
QDomElement locate(QDomElement root, const QString &path, bool *ok, QString *error)
{
    for (const auto &tag : path.split('/')) {
        auto child = root.firstChildElement(tag);
        if (!child.isNull() && !child.nextSiblingElement(tag).isNull()) {
            *ok = false; fail(error, path); return {};
        }
        root = child;
        if (root.isNull()) break;
    }
    if (!root.firstChildElement().isNull()) { *ok = false; fail(error, path); }
    return root;
}
void write(QDomDocument &xml, const QString &path, const QString &value)
{
    auto element = xml.documentElement();
    for (const auto &tag : path.split('/')) {
        auto child = element.firstChildElement(tag);
        if (child.isNull()) { child = xml.createElement(tag); element.appendChild(child); }
        element = child;
    }
    for (auto node = element.firstChild(); !node.isNull();) {
        const auto next = node.nextSibling();
        if (node.isText() || node.isCDATASection()) element.removeChild(node);
        node = next;
    }
    element.appendChild(xml.createTextNode(value));
}
const QStringList intervalKeys = {"restartInterval", "wallInterval", "flowInterval"};
const QStringList intervalTags = {"CONTINUE_INTERVAL", "WALL_INTERVAL", "FLOW_INTERVAL"};
}
QStringList outputFlowVariableKeys()
{
    return {"pressure","density","velocity-u","velocity-v","velocity-w","temperature","mach","node-status","component-id","initialization-status","vorticity-magnitude","vorticity-x","vorticity-y","vorticity-z","total-temperature","total-pressure","q-criterion"};
}
QStringList outputWallVariableKeys()
{
    return {"pressure","density","velocity-u","velocity-v","velocity-w","temperature","mach","node-status","component-id","initialization-status","heat-flux","friction-x","friction-y","friction-z","friction-magnitude","total-temperature","total-pressure"};
}
ParameterCategoryConfig defaultOutputControl()
{
    ParameterCategoryConfig config; config.category = ParameterCategory::OutputControl;
    for (int i = 0; i < 3; ++i) add(config, intervalKeys[i], ParameterValue::integer(i == 0 ? 100 : -1));
    const auto flow = outputFlowVariableKeys();
    for (int i = 0; i < flow.size(); ++i) add(config, "flowVariables/" + flow[i], ParameterValue::boolean(i < 5));
    for (const auto &key : outputWallVariableKeys()) add(config, "wallVariables/" + key, ParameterValue::boolean(key == "mach"));
    return config;
}
bool validateOutputControl(const ParameterCategoryConfig &config, QString *error)
{
    if (config.category != ParameterCategory::OutputControl || !config.isValid()) return fail(error, "OutputControl");
    const auto defaults = defaultOutputControl();
    for (const auto &expected : defaults.entries) {
        const auto *entry = config.find(expected.key);
        if (!entry || entry->value.kind != expected.value.kind) return fail(error, expected.key);
    }
    for (const auto &entry : config.entries) {
        if (intervalKeys.contains(entry.key)) {
            const auto value = entry.value.integerValue;
            if (value < (entry.key == "restartInterval" ? 0 : -1000000000) || value > 1000000000) return fail(error, entry.key);
        } else if (entry.key.startsWith(boundaryPrefix)) {
            const auto name = entry.key.mid(boundaryPrefix.size());
            if (entry.value.kind != ParameterValueKind::Boolean || name.trimmed().isEmpty() || name.contains(';')) return fail(error, entry.key);
        } else if (!defaults.find(entry.key)) return fail(error, entry.key);
    }
    return true;
}
bool readOutputControl(const QDomDocument &xml, ParameterCategoryConfig *config, QStringList *warnings, QString *error)
{
    auto candidate = defaultOutputControl();
    for (int i = 0; i < 3; ++i) {
        const auto path = "RefenceParametersData/" + intervalTags[i];
        bool ok = true; const auto element = locate(xml.documentElement(), path, &ok, error);
        if (!ok) return false;
        if (element.isNull()) { warnings->append(QString::fromUtf16(u"缺少输出控制字段，使用默认值：") + path); continue; }
        const auto value = element.text().trimmed().toLongLong(&ok);
        if (!ok) return fail(error, path);
        candidate.entries[i].value = ParameterValue::integer(value);
    }
    for (int group = 0; group < 2; ++group) {
        const auto keys = group == 0 ? outputFlowVariableKeys() : outputWallVariableKeys();
        const QString prefix = group == 0 ? "flowVariables/" : "wallVariables/";
        const QString path = group == 0 ? "OutputSettingData/VOLUME" : "OutputSettingData/BOUNDARY";
        bool ok = true; const auto element = locate(xml.documentElement(), path, &ok, error);
        if (!ok) return false;
        if (element.isNull()) { warnings->append(QString::fromUtf16(u"缺少输出控制字段，使用默认值：") + path); continue; }
        const auto flags = element.text().split(';', QString::KeepEmptyParts);
        if (flags.size() != 17) return fail(error, path);
        for (int i = 0; i < 17; ++i) {
            const auto flag = flags[i].trimmed();
            if (flag != "0" && flag != "1") return fail(error, path);
            for (auto &entry : candidate.entries) if (entry.key == prefix + keys[i]) entry.value = ParameterValue::boolean(flag == "1");
        }
    }
    bool ok = true; const auto history = locate(xml.documentElement(), "OutputSettingData/OUTPUTBC", &ok, error);
    if (!ok) return false;
    QStringList selected;
    if (!history.isNull() && !history.text().trimmed().isEmpty()) {
        for (const auto &part : history.text().split(';', QString::KeepEmptyParts)) {
            const auto name = part.trimmed();
            if (name.isEmpty() || selected.contains(name)) return fail(error, "OutputSettingData/OUTPUTBC");
            selected.append(name);
        }
    }
    QStringList names = selected;
    const auto manager = xml.documentElement().firstChildElement("BCDataManager");
    for (auto bc = manager.firstChildElement("BC_DATA"); !bc.isNull(); bc = bc.nextSiblingElement("BC_DATA")) {
        const auto name = bc.firstChildElement("_ssName").text().trimmed();
        if (!name.isEmpty() && !names.contains(name)) names.append(name);
    }
    for (const auto &name : names) add(candidate, boundaryPrefix + name, ParameterValue::boolean(selected.contains(name)));
    if (!validateOutputControl(candidate, error)) return false;
    *config = candidate; return true;
}
void writeOutputControl(QDomDocument &xml, const ParameterCategoryConfig &config)
{
    for (int i = 0; i < 3; ++i) write(xml, "RefenceParametersData/" + intervalTags[i], QString::number(config.find(intervalKeys[i])->value.integerValue));
    for (int group = 0; group < 2; ++group) {
        const auto keys = group == 0 ? outputFlowVariableKeys() : outputWallVariableKeys();
        const QString prefix = group == 0 ? "flowVariables/" : "wallVariables/";
        QStringList flags;
        for (const auto &key : keys) flags.append(config.find(prefix + key)->value.booleanValue ? "1" : "0");
        write(xml, group == 0 ? "OutputSettingData/VOLUME" : "OutputSettingData/BOUNDARY", flags.join(';'));
    }
    QStringList selected;
    for (const auto &entry : config.entries) if (entry.key.startsWith(boundaryPrefix) && entry.value.booleanValue) selected.append(entry.key.mid(boundaryPrefix.size()));
    write(xml, "OutputSettingData/OUTPUTBC", selected.join(';'));
}
}
