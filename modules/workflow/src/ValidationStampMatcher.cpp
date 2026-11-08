#include "aerosim/workflow/ValidationStampMatcher.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace aerosim {
namespace workflow {
namespace {

bool scopeEquals(const ScopeRef &left, const ScopeRef &right)
{
    return left.projectId == right.projectId
        && left.caseId == right.caseId
        && left.objectId == right.objectId;
}

bool resourceLess(const ResourceInputStamp &left,
                  const ResourceInputStamp &right)
{
    if (left.projectId != right.projectId)
        return left.projectId < right.projectId;
    return left.resourceId < right.resourceId;
}

QString resourceIdentityKey(const ResourceInputStamp &resource)
{
    return resource.projectId + QChar(0x1f) + resource.resourceId;
}

AccessResult<ValidationInputStamp> invalidStamp(const QString &message)
{
    return AccessResult<ValidationInputStamp>::failure(
        AccessStatus::InvalidArgument,
        QStringLiteral("workflow.validation.invalid_input_stamp"), message);
}

} // namespace

bool validationInputStampIsValid(const ValidationInputStamp &stamp)
{
    if (stamp.scope.projectId.isEmpty() || stamp.definitionVersion.isEmpty()
        || stamp.parameter.snapshotId.isEmpty()
        || !scopeEquals(stamp.scope, stamp.parameter.scope)
        || stamp.parameter.revision == 0) {
        return false;
    }

    QSet<QString> identities;
    for (const ResourceInputStamp &resource : stamp.resources) {
        if (resource.projectId.isEmpty() || resource.resourceId.isEmpty()
            || resource.revision == 0) {
            return false;
        }
        if (resource.hasVerifiedFingerprint
            && (!resource.fingerprint.isValid()
                || !resource.fingerprint.isVerified())) {
            return false;
        }
        const QString identity = resourceIdentityKey(resource);
        if (identities.contains(identity)) {
            return false;
        }
        identities.insert(identity);
    }
    return true;
}

AccessResult<ValidationInputStamp>
normalizeValidationInputStamp(const ValidationInputStamp &stamp)
{
    if (!validationInputStampIsValid(stamp)) {
        return invalidStamp(QStringLiteral(
            "input stamp identities, revisions, fingerprints, and resources must be valid"));
    }
    ValidationInputStamp normalized = stamp;
    std::sort(normalized.resources.begin(), normalized.resources.end(),
              resourceLess);
    return AccessResult<ValidationInputStamp>::success(normalized);
}

AccessResult<bool> validationInputStampsMatch(
    const ValidationInputStamp &validated,
    const ValidationInputStamp &current,
    quint64 *operationCount)
{
    if (!validationInputStampIsValid(validated)
        || !validationInputStampIsValid(current)) {
        return AccessResult<bool>::failure(
            AccessStatus::InvalidArgument,
            QStringLiteral("workflow.validation.invalid_input_stamp"),
            QStringLiteral("input stamps must be valid before comparison"));
    }
    if (operationCount)
        *operationCount += 1;
    if (!scopeEquals(validated.scope, current.scope)
        || validated.definitionVersion != current.definitionVersion
        || validated.parameter.snapshotId != current.parameter.snapshotId
        || !scopeEquals(validated.parameter.scope, current.parameter.scope)
        || validated.parameter.revision != current.parameter.revision
        || validated.resources.size() != current.resources.size()) {
        return AccessResult<bool>::success(false);
    }

    QHash<QString, const ResourceInputStamp *> currentResources;
    for (const ResourceInputStamp &resource : current.resources)
        currentResources.insert(resourceIdentityKey(resource), &resource);
    for (const ResourceInputStamp &validatedResource : validated.resources) {
        if (operationCount)
            *operationCount += 1;
        const auto found = currentResources.constFind(
            resourceIdentityKey(validatedResource));
        if (found == currentResources.constEnd()
            || validatedResource.revision != found.value()->revision) {
            return AccessResult<bool>::success(false);
        }
        const ResourceInputStamp &currentResource = *found.value();
        if (validatedResource.hasVerifiedFingerprint) {
            if (!currentResource.hasVerifiedFingerprint
                || !currentResource.fingerprint.isVerified()
                || validatedResource.fingerprint.algorithm
                    != currentResource.fingerprint.algorithm
                || validatedResource.fingerprint.digest
                    != currentResource.fingerprint.digest
                || validatedResource.fingerprint.sizeBytes
                    != currentResource.fingerprint.sizeBytes) {
                return AccessResult<bool>::success(false);
            }
        }
    }
    return AccessResult<bool>::success(true);
}

} // namespace workflow
} // namespace aerosim
