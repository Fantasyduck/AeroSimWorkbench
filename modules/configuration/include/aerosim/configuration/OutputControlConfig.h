#pragma once
#include "aerosim/contracts/ConfigurationModels.h"
#include <QDomDocument>
#include <QStringList>
namespace aerosim {
QStringList outputFlowVariableKeys();
QStringList outputWallVariableKeys();
ParameterCategoryConfig defaultOutputControl();
bool validateOutputControl(const ParameterCategoryConfig &config, QString *error);
bool readOutputControl(const QDomDocument &xml, ParameterCategoryConfig *config,
                       QStringList *warnings, QString *error);
void writeOutputControl(QDomDocument &xml, const ParameterCategoryConfig &config);
}
