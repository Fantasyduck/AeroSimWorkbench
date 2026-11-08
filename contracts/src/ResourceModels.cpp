#include "aerosim/contracts/ResourceModels.h"

#include <QDir>
#include <QUrl>

namespace aerosim {
namespace {

bool isWindowsDriveAbsolute(const QString &path)
{
    return path.size() >= 3 && path.at(0).isLetter()
        && path.at(1) == QLatin1Char(':')
        && (path.at(2) == QLatin1Char('/') || path.at(2) == QLatin1Char('\\'));
}

bool isWindowsUncAbsolute(const QString &path)
{
    return path.startsWith(QStringLiteral("\\\\"))
        || path.startsWith(QStringLiteral("//"));
}

bool isAbsolutePath(const QString &path)
{
    return QDir::isAbsolutePath(path) || isWindowsDriveAbsolute(path)
        || isWindowsUncAbsolute(path);
}

bool isValidProjectRelativePath(const QString &path)
{
    if (path.isEmpty() || isAbsolutePath(path) || path.contains(QLatin1Char('\\'))
        || path.contains(QLatin1Char(':')) || path.contains(QChar(0)))
        return false;

    const QStringList segments = path.split(QLatin1Char('/'), QString::KeepEmptyParts);
    for (const QString &segment : segments) {
        if (segment.isEmpty() || segment == QStringLiteral(".")
            || segment == QStringLiteral(".."))
            return false;
    }
    return true;
}

} // namespace

ResourceLocation ResourceLocation::projectRelative(const QString &path)
{
    ResourceLocation location;
    location.kind = LocationKind::ProjectRelative;
    location.value = path;
    return location;
}

ResourceLocation ResourceLocation::externalAbsolute(const QString &path)
{
    ResourceLocation location;
    location.kind = LocationKind::ExternalAbsolute;
    location.value = path;
    return location;
}

ResourceLocation ResourceLocation::uri(const QString &value)
{
    ResourceLocation location;
    location.kind = LocationKind::Uri;
    location.value = value;
    return location;
}

bool ResourceLocation::isValid() const
{
    switch (kind) {
    case LocationKind::ProjectRelative:
        return isValidProjectRelativePath(value);
    case LocationKind::ExternalAbsolute:
        return !value.isEmpty() && isAbsolutePath(value);
    case LocationKind::Uri: {
        const QUrl parsed(value, QUrl::StrictMode);
        return parsed.isValid() && !parsed.scheme().isEmpty();
    }
    }
    return false;
}

bool ContentFingerprint::isValid() const
{
    if (status == FingerprintStatus::NotComputed)
        return algorithm.isEmpty() && digest.isEmpty();
    return !algorithm.isEmpty() && !digest.isEmpty();
}

bool ResourceRefV2::isValid() const
{
    return !projectId.isEmpty() && !resourceId.isEmpty() && revision > 0
        && type != ResourceType::Unknown && location.isValid() && fingerprint.isValid();
}

} // namespace aerosim
