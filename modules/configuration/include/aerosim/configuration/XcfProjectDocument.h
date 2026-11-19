#pragma once

#include "aerosim/contracts/ConfigurationModels.h"
#include "aerosim/configuration/CalculationControlConfig.h"
#include "aerosim/configuration/OutputControlConfig.h"
#include "aerosim/configuration/SolutionMethodConfig.h"
#include <QDomDocument>
#include <QStringList>

namespace aerosim {

// Editable category, not an immutable/versioned ParameterSnapshot. Retains the
// source XML so sections outside this first adapter's scope survive saving.
class XcfProjectDocument {
public:
    XcfProjectDocument();
    static ParameterCategoryConfig defaultGoverningEquations();
    const ParameterCategoryConfig &solutionMethod() const { return solution_; }
    bool setSolutionMethod(const ParameterCategoryConfig &config, QString *error = nullptr);
    const ParameterCategoryConfig &outputControl() const { return output_; }
    bool setOutputControl(const ParameterCategoryConfig &config, QString *error = nullptr);
    const SolverConfigPayload &calculationControl() const { return calculation_; }
    bool setCalculationControl(const SolverConfigPayload &config, QString *error = nullptr);
    const ParameterCategoryConfig &governingEquations() const { return equations_; }
    bool setGoverningEquations(const ParameterCategoryConfig &config, QString *error = nullptr);
    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr);
    QString filePath() const { return filePath_; }
    bool isModified() const { return modified_; }
    QStringList warnings() const { return warnings_; }

private:
    QDomDocument xml_;
    ParameterCategoryConfig equations_;
    SolverConfigPayload calculation_;
    ParameterCategoryConfig output_;
    ParameterCategoryConfig solution_;
    QString filePath_;
    QStringList warnings_;
    bool modified_ = false;
    // An unsupported inactive turbulence model is retained until the user
    // explicitly chooses a supported model or enables turbulent flow.
    bool preserveInactiveTurbulence_ = false;
};
}
