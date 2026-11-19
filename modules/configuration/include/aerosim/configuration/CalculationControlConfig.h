#pragma once

#include "aerosim/contracts/ConfigurationModels.h"
#include <QDomDocument>
#include <QStringList>

namespace aerosim {
// Calculation control has no dedicated ParameterCategory in the public
// contracts. Use their existing solver-specific typed payload instead.
SolverConfigPayload defaultCalculationControl();
bool validateCalculationControl(const SolverConfigPayload &config, QString *error);
bool readCalculationControl(const QDomDocument &xml, SolverConfigPayload *config,
                            QStringList *warnings, QString *error);
void writeCalculationControl(QDomDocument &xml, const SolverConfigPayload &config);
}
