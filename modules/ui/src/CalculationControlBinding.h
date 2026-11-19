#pragma once
#include "aerosim/configuration/CalculationControlConfig.h"
#include <QVariantMap>

namespace aerosim {
inline QVariantMap calculationControlDraft(const SolverConfigPayload &config)
{
    QVariantMap draft;
    for (const auto &entry : config.entries) {
        const auto &value = entry.value;
        if (value.kind == ParameterValueKind::Boolean) draft[entry.key] = value.booleanValue;
        else if (value.kind == ParameterValueKind::Integer) draft[entry.key] = value.integerValue;
        else draft[entry.key] = value.realValue;
    }
    return draft;
}
inline SolverConfigPayload calculationControlConfig(const QVariantMap &draft)
{
    auto config = defaultCalculationControl();
    for (auto &entry : config.entries) {
        if (!draft.contains(entry.key)) continue;
        const auto value = draft.value(entry.key);
        if (entry.value.kind == ParameterValueKind::Boolean) entry.value = ParameterValue::boolean(value.toBool());
        else if (entry.value.kind == ParameterValueKind::Integer) entry.value = ParameterValue::integer(value.toLongLong());
        else entry.value = ParameterValue::real(value.toDouble());
    }
    return config;
}
}
