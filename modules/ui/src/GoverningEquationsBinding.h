#pragma once

#include "aerosim/configuration/XcfProjectDocument.h"
#include <QVariantMap>

namespace aerosim {
// UI boundary only: the service/file adapter works with typed contracts.
inline QVariantMap governingEquationsDraft(const ParameterCategoryConfig &config)
{
    QVariantMap draft;
    for (const auto &entry : config.entries) {
        const auto &value = entry.value;
        switch (value.kind) {
        case ParameterValueKind::Boolean: draft[entry.key] = value.booleanValue; break;
        case ParameterValueKind::Integer: draft[entry.key] = value.integerValue; break;
        case ParameterValueKind::Real: draft[entry.key] = value.realValue; break;
        default: draft[entry.key] = value.textValue; break;
        }
    }
    draft["viscousFlowRegime"] = draft.value("flowRegime") == "laminar" ? "laminar" : "turbulent";
    return draft;
}
inline ParameterCategoryConfig governingEquationsConfig(const QVariantMap &draft)
{
    auto config = XcfProjectDocument::defaultGoverningEquations();
    for (auto &entry : config.entries) {
        if (!draft.contains(entry.key)) continue;
        const auto value = draft.value(entry.key);
        switch (entry.value.kind) {
        case ParameterValueKind::Boolean: entry.value = ParameterValue::boolean(value.toBool()); break;
        case ParameterValueKind::Integer: entry.value = ParameterValue::integer(value.toLongLong()); break;
        case ParameterValueKind::Real: entry.value = ParameterValue::real(value.toDouble()); break;
        default: entry.value = ParameterValue::enumeration(value.toString()); break;
        }
    }
    return config;
}
}
