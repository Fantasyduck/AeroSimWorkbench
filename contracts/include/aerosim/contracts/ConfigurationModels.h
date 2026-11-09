#pragma once

#include "aerosim/contracts/MeshModels.h"
#include "aerosim/contracts/References.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace aerosim {

// Parameter categories mirror the parameter workspace pages so a State's
// configuration can be grouped, validated and snapshotted per category.
enum class ParameterCategory {
    GoverningEquations,
    BoundaryConditions,
    MeshMotion,
    SolutionMethod,
    ReferenceValues,
    OutputControl,
    Initialization
};

enum class ParameterValueKind {
    Unspecified,
    Boolean,
    Integer,
    Real,
    Text,
    Enumeration
};

// Inclusive numeric applicability window. hasMinimum/hasMaximum avoid a C++17
// optional and keep the type copyable for queued Qt messages.
struct ParameterRange {
    bool hasMinimum = false;
    double minimum = 0.0;
    bool hasMaximum = false;
    double maximum = 0.0;

    bool isValid() const;
    bool contains(double value) const;

    static ParameterRange between(double minimum, double maximum);
    static ParameterRange atLeast(double minimum);
    static ParameterRange atMost(double maximum);
};

// One typed parameter value. QVariantMap stays a window-local UI draft; persisted
// configuration uses explicit kinds, units and validity instead.
struct ParameterValue {
    ParameterValueKind kind = ParameterValueKind::Unspecified;
    bool booleanValue = false;
    qint64 integerValue = 0;
    double realValue = 0.0;
    QString textValue;
    QString unit;

    bool isValid() const;
    bool isNumeric() const;

    static ParameterValue boolean(bool value);
    static ParameterValue integer(qint64 value, const QString &unit = QString());
    static ParameterValue real(double value, const QString &unit = QString());
    static ParameterValue text(const QString &value);
    static ParameterValue enumeration(const QString &token);
};

struct ParameterEntry {
    QString key;
    ParameterValue value;
    ParameterRange range;
    QString applicabilityRule;   // empty means "always applicable"
    bool required = false;

    bool isValid() const;
    bool isApplicable() const;
    bool valueWithinRange() const;

    static ParameterEntry requiredReal(const QString &key, double value,
                                       const QString &unit);
    static ParameterEntry optionalReal(const QString &key, double value,
                                       const QString &unit);
    static ParameterEntry requiredBoolean(const QString &key, bool value);
};

struct ParameterCategoryConfig {
    ParameterCategory category = ParameterCategory::GoverningEquations;
    QVector<ParameterEntry> entries;

    bool isValid() const;
    bool hasUniqueKeys() const;
    const ParameterEntry *find(const QString &key) const;
};

// Extensible boundary for solver-specific configuration that is not modelled
// field by field yet. The opaque schema token keeps unknown algorithms
// representable without inventing parameters that have not been confirmed.
struct SolverConfigPayload {
    QString solverId;
    QString schemaId;
    quint64 schemaVersion = 0;
    QVector<ParameterEntry> entries;

    bool isValid() const;
    bool hasUniqueKeys() const;
};

// An immutable parameter set identified by snapshotId + revision. It is a typed
// value list rather than a QVariantMap so units and applicability survive.
struct ParameterSnapshot {
    ParameterSnapshotRef ref;
    StateRef state;
    QVector<ParameterCategoryConfig> categories;
    QVector<SolverConfigPayload> solverConfigs;
    quint64 schemaVersion = 0;
    ContentFingerprint fingerprint;

    bool isIdentified() const;
    bool isPinned() const;
    bool scopeMirrorsState() const;
    bool isValid() const;
    const ParameterCategoryConfig *category(ParameterCategory category) const;
    const ParameterEntry *entry(ParameterCategory category, const QString &key) const;
};

// A State is the versioned engineering case. It pins the resources a run needs;
// a State without a computation mesh or parameter snapshot may be edited but is
// not computable.
struct StateConfig {
    StateRef ref;
    QString name;
    QString solverId;
    QString backendId;
    MeshRefV2 computationMesh;
    ParameterSnapshotRef parameterSnapshot;
    quint64 schemaVersion = 0;

    bool isValid() const;
    bool isComputable() const;
};

struct BatchPlanItem {
    QString itemId;
    StateRef state;
    quint32 order = 0;
    QString displayLabel;

    bool isValid() const;
};

// One append-only revision of a logical plan. Editing a plan creates a new
// revision; a revision already pinned by a BatchRun is never rewritten.
struct BatchPlan {
    BatchPlanRef ref;
    QString name;
    QVector<BatchPlanItem> items;
    quint64 schemaVersion = 0;
    QDateTime createdAt;

    bool isValid() const;
    bool hasUniqueItemIds() const;
    bool hasUniqueOrders() const;
    bool hasUniqueStates() const;
    const BatchPlanItem *item(const QString &itemId) const;
};

// Revision history of one planId. appendRevision() refuses identity changes and
// non-increasing revisions, so history is never overwritten in place.
struct BatchPlanHistory {
    QString projectId;
    QString planId;
    QVector<BatchPlan> revisions;

    bool isValid() const;
    const BatchPlan *latest() const;
    const BatchPlan *findRevision(quint64 revision) const;
    quint64 nextRevision() const;
    bool appendRevision(const BatchPlan &plan);
};

// Immutable execution snapshot taken when a batch run starts. It pins the exact
// plan revision and item order (plus the resulting fingerprint on `resource`)
// so later plan edits cannot change a historical run.
struct BatchPlanExecutionSnapshot {
    ResourceRefV2 resource;      // must be typed ResourceType::BatchPlanSnapshot
    BatchPlanRef sourcePlan;
    QVector<BatchPlanItem> items;
    QString executionPolicy;
    quint64 schemaVersion = 0;

    bool isValid() const;
    bool matches(const BatchPlan &plan) const;
};

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::ParameterRange)
Q_DECLARE_METATYPE(aerosim::ParameterValue)
Q_DECLARE_METATYPE(aerosim::ParameterEntry)
Q_DECLARE_METATYPE(aerosim::ParameterSnapshot)
Q_DECLARE_METATYPE(aerosim::StateConfig)
Q_DECLARE_METATYPE(aerosim::BatchPlanItem)
Q_DECLARE_METATYPE(aerosim::BatchPlan)
Q_DECLARE_METATYPE(aerosim::BatchPlanExecutionSnapshot)
