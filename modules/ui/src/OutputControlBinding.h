#pragma once
#include "aerosim/configuration/OutputControlConfig.h"
#include <QVariantMap>
namespace aerosim {
inline QVariantMap outputControlDraft(const ParameterCategoryConfig &config)
{
    QVariantMap draft;
    QStringList flow, wall, history, names;
    for (const auto &entry : config.entries) {
        if (entry.value.kind == ParameterValueKind::Integer) draft[entry.key] = entry.value.integerValue;
        else if (entry.key.startsWith("flowVariables/") && entry.value.booleanValue) flow.append(entry.key.mid(14));
        else if (entry.key.startsWith("wallVariables/") && entry.value.booleanValue) wall.append(entry.key.mid(14));
        else if (entry.key.startsWith("boundaryHistory/")) {
            names.append(entry.key.mid(16));
            if (entry.value.booleanValue) history.append(entry.key.mid(16));
        }
    }
    draft["flowVariables"] = flow; draft["wallVariables"] = wall;
    draft["boundaryHistory"] = history; draft["boundaryNames"] = names;
    return draft;
}
inline ParameterCategoryConfig outputControlConfig(const QVariantMap &draft)
{
    auto config = defaultOutputControl();
    for (auto &entry : config.entries) {
        if (entry.value.kind == ParameterValueKind::Integer) {
            if (draft.contains(entry.key)) entry.value = ParameterValue::integer(draft.value(entry.key).toLongLong());
        } else {
            const auto group = entry.key.section('/', 0, 0);
            if (draft.contains(group)) entry.value = ParameterValue::boolean(draft.value(group).toStringList().contains(entry.key.section('/', 1)));
        }
    }
    auto names = draft.value("boundaryNames").toStringList();
    const auto selected = draft.value("boundaryHistory").toStringList();
    for (const auto &name : selected) if (!names.contains(name)) names.append(name);
    for (const auto &name : names) {
        ParameterEntry entry; entry.key = "boundaryHistory/" + name;
        entry.value = ParameterValue::boolean(selected.contains(name)); config.entries.append(entry);
    }
    return config;
}
}
