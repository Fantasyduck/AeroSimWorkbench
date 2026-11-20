#pragma once
#include "aerosim/configuration/SolutionMethodConfig.h"
#include <QVariantMap>
namespace aerosim {
inline QVariantMap solutionMethodDraft(const ParameterCategoryConfig &config) {
    QVariantMap draft;
    for (const auto &entry : config.entries) {
        if (entry.key.startsWith("cflPoints/")) continue;
        const auto &v = entry.value;
        if (v.kind == ParameterValueKind::Boolean) draft[entry.key] = v.booleanValue;
        else if (v.kind == ParameterValueKind::Integer) draft[entry.key] = v.integerValue;
        else if (v.kind == ParameterValueKind::Real) draft[entry.key] = v.realValue;
        else draft[entry.key] = v.textValue;
    }
    QVariantList points;
    for (int i = 0; i < draft.value("cflPointCount").toInt(); ++i) {
        const auto prefix = "cflPoints/" + QString::number(i) + "/";
        points.append(QVariantMap{{"iteration", config.find(prefix + "iteration")->value.integerValue},
                                 {"cfl", config.find(prefix + "cfl")->value.realValue}});
    }
    draft["cflPoints"] = points; return draft;
}
inline ParameterCategoryConfig solutionMethodConfig(const QVariantMap &draft) {
    auto config = defaultSolutionMethod();
    for (int i = config.entries.size() - 1; i >= 0; --i) {
        auto &entry = config.entries[i];
        if (entry.key.startsWith("cflPoints/")) { config.entries.removeAt(i); continue; }
        if (!draft.contains(entry.key)) continue;
        const auto v = draft.value(entry.key);
        if (entry.value.kind == ParameterValueKind::Boolean) entry.value = ParameterValue::boolean(v.toBool());
        else if (entry.value.kind == ParameterValueKind::Integer) entry.value = ParameterValue::integer(v.toLongLong());
        else if (entry.value.kind == ParameterValueKind::Real) entry.value = ParameterValue::real(v.toDouble(), "s");
        else entry.value = ParameterValue::enumeration(v.toString());
    }
    const auto points = draft.value("cflPoints").toList();
    const auto count = draft.value("cflPointCount", 2).toInt();
    for (int i = 0; i < qMin(count, points.size()); ++i) {
        const auto point = points[i].toMap();
        ParameterEntry entry; entry.key = "cflPoints/" + QString::number(i) + "/iteration";
        entry.value = ParameterValue::integer(point.value("iteration").toLongLong()); config.entries.append(entry);
        entry.key = "cflPoints/" + QString::number(i) + "/cfl";
        entry.value = ParameterValue::real(point.value("cfl").toDouble()); config.entries.append(entry);
    }
    return config;
}
}
