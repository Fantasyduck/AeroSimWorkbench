#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace aerosim {

enum class ResourceType {
    Unknown,
    ProjectFile,
    ParameterSnapshot,
    TaskInputManifest,
    BatchPlanSnapshot,
    Mesh,
    Result,
    Log,
    Monitor,
    Checkpoint,
    Export
};

enum class LocationKind { ProjectRelative, ExternalAbsolute, Uri };

enum class FingerprintStatus { NotComputed, Computed, Verified };

struct ResourceLocation {
    LocationKind kind = LocationKind::ProjectRelative;
    QString value;

    static ResourceLocation projectRelative(const QString &path);
    static ResourceLocation externalAbsolute(const QString &path);
    static ResourceLocation uri(const QString &value);

    bool isValid() const;
};

struct ContentFingerprint {
    FingerprintStatus status = FingerprintStatus::NotComputed;
    QString algorithm;
    QString digest;
    quint64 sizeBytes = 0;
    QDateTime modifiedAt;

    bool isValid() const;
    bool isComputed() const { return status != FingerprintStatus::NotComputed; }
    bool isVerified() const { return status == FingerprintStatus::Verified; }
};

struct ResourceRefV2 {
    QString projectId;
    QString resourceId;
    quint64 revision = 0;
    ResourceType type = ResourceType::Unknown;
    ResourceLocation location;
    ContentFingerprint fingerprint;

    bool isValid() const;
    bool hasVerifiedContent() const
    {
        return isValid() && fingerprint.isVerified();
    }
    bool sameIdentity(const ResourceRefV2 &other) const
    {
        return projectId == other.projectId && resourceId == other.resourceId;
    }
    bool sameVersion(const ResourceRefV2 &other) const
    {
        return sameIdentity(other) && revision == other.revision;
    }
};

} // namespace aerosim

Q_DECLARE_METATYPE(aerosim::ResourceLocation)
Q_DECLARE_METATYPE(aerosim::ContentFingerprint)
Q_DECLARE_METATYPE(aerosim::ResourceRefV2)
