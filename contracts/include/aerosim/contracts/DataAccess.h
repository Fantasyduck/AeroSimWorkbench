#pragma once

#include "aerosim/contracts/References.h"
#include "aerosim/contracts/WorkflowModels.h"

#include <QString>

namespace aerosim {

// Read-only boundary contracts. D, C and G own the concrete implementations.
class IProjectAccess {
public:
    virtual ~IProjectAccess() = default;
    virtual ProjectRef currentProject() const = 0;
    virtual ResourceRef resource(const QString &resourceId) const = 0;
};

class IParameterAccess {
public:
    virtual ~IParameterAccess() = default;
    virtual ValidationResult validateParameters(const ScopeRef &scope) const = 0;
    virtual ParameterSnapshotRef currentSnapshot(const ScopeRef &scope) const = 0;
};

class IMeshAccess {
public:
    virtual ~IMeshAccess() = default;
    virtual ValidationResult validateMesh(const ScopeRef &scope) const = 0;
    virtual MeshRef mesh(const ScopeRef &scope) const = 0;
};

class IResultAccess {
public:
    virtual ~IResultAccess() = default;
    virtual ValidationResult validateResult(const ResultRef &result) const = 0;
};

} // namespace aerosim
