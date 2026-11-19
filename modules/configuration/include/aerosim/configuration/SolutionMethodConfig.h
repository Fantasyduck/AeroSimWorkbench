#pragma once
#include "aerosim/contracts/ConfigurationModels.h"
#include <QDomDocument>
#include <QStringList>
namespace aerosim {
ParameterCategoryConfig defaultSolutionMethod();
bool validateSolutionMethod(const ParameterCategoryConfig &config, QString *error);
bool readSolutionMethod(const QDomDocument &xml, ParameterCategoryConfig *config, QStringList *warnings, QString *error);
void writeSolutionMethod(QDomDocument &xml, const ParameterCategoryConfig &config);
}
