#pragma once

#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace aerosim {

struct ProjectRef {
    QString projectId;
    QString filePath;
    quint64 revision = 0;
};

// IDs and paths are copied across modules. They do not own the referenced data.
struct ResourceRef {
    QString resourceId;
    QString projectId;
    QString kind;
    QString filePath;
    quint64 revision = 0;
};

struct ScopeRef {
    QString projectId;
    QString caseId;
    QString objectId;
};

struct ParameterSnapshotRef {
    QString snapshotId;
    ScopeRef scope;
    quint64 revision = 0;
};

struct MeshRef {
    ResourceRef resource;
    QString meshVersion;
};

struct ResultRef {
    ResourceRef resource;
    QString taskId;
    QString meshVersion;
    QString caseId;
};

// V2 references use opaque stable IDs. A revision of zero means unspecified
// legacy data and is not valid for a version-pinned calculation.
struct StateRef {
    QString projectId;
    QString stateId;
    quint64 revision = 0;

    bool isValid() const
    {
        return !projectId.isEmpty() && !stateId.isEmpty() && revision > 0;
    }
    bool sameIdentity(const StateRef &other) const
    {
        return projectId == other.projectId && stateId == other.stateId;
    }
    bool sameVersion(const StateRef &other) const
    {
        return sameIdentity(other) && revision == other.revision;
    }
};

struct BatchPlanRef {
    QString projectId;
    QString planId;
    quint64 revision = 0;

    bool isValid() const
    {
        return !projectId.isEmpty() && !planId.isEmpty() && revision > 0;
    }
    bool sameIdentity(const BatchPlanRef &other) const
    {
        return projectId == other.projectId && planId == other.planId;
    }
    bool sameVersion(const BatchPlanRef &other) const
    {
        return sameIdentity(other) && revision == other.revision;
    }
};

struct BatchRunRef {
    QString projectId;
    QString batchRunId;
    BatchPlanRef sourcePlan;

    bool isValid() const
    {
        return !projectId.isEmpty() && !batchRunId.isEmpty()
            && sourcePlan.isValid() && sourcePlan.projectId == projectId;
    }
    bool sameIdentity(const BatchRunRef &other) const
    {
        return projectId == other.projectId && batchRunId == other.batchRunId;
    }
    bool sameVersion(const BatchRunRef &other) const
    {
        // A BatchRun is immutable after creation. sourcePlan is provenance and
        // intentionally does not participate in BatchRun identity.
        return sameIdentity(other);
    }
};

struct TaskRef {
    QString projectId;
    QString taskId;
    QString batchRunId;

    bool isValid() const
    {
        return !projectId.isEmpty() && !taskId.isEmpty();
    }
    bool sameIdentity(const TaskRef &other) const
    {
        // batchRunId is relationship metadata, not part of Task identity.
        return projectId == other.projectId && taskId == other.taskId;
    }
    bool sameVersion(const TaskRef &other) const
    {
        // Task input is immutable; execution retries are represented by
        // AttemptRef rather than a Task revision.
        return sameIdentity(other);
    }
};

struct AttemptRef {
    TaskRef task;
    QString attemptId;

    bool isValid() const
    {
        return task.isValid() && !attemptId.isEmpty();
    }
    bool sameIdentity(const AttemptRef &other) const
    {
        return task.sameIdentity(other.task) && attemptId == other.attemptId;
    }
    bool sameVersion(const AttemptRef &other) const
    {
        return sameIdentity(other);
    }
};

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::StateRef)
Q_DECLARE_METATYPE(aerosim::BatchPlanRef)
Q_DECLARE_METATYPE(aerosim::BatchRunRef)
Q_DECLARE_METATYPE(aerosim::TaskRef)
Q_DECLARE_METATYPE(aerosim::AttemptRef)
