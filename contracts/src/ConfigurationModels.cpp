#include "aerosim/contracts/ConfigurationModels.h"

namespace aerosim {
namespace {

bool entriesHaveUniqueKeys(const QVector<ParameterEntry> &entries)
{
    for (int i = 0; i < entries.size(); ++i) {
        for (int j = i + 1; j < entries.size(); ++j) {
            if (entries.at(i).key == entries.at(j).key)
                return false;
        }
    }
    return true;
}

} // namespace

ParameterRange ParameterRange::between(double minimum, double maximum)
{
    ParameterRange range;
    range.hasMinimum = true;
    range.minimum = minimum;
    range.hasMaximum = true;
    range.maximum = maximum;
    return range;
}

ParameterRange ParameterRange::atLeast(double minimum)
{
    ParameterRange range;
    range.hasMinimum = true;
    range.minimum = minimum;
    return range;
}

ParameterRange ParameterRange::atMost(double maximum)
{
    ParameterRange range;
    range.hasMaximum = true;
    range.maximum = maximum;
    return range;
}

bool ParameterRange::isValid() const
{
    if (hasMinimum && !qIsFinite(minimum))
        return false;
    if (hasMaximum && !qIsFinite(maximum))
        return false;
    if (hasMinimum && hasMaximum && minimum > maximum)
        return false;
    return true;
}

bool ParameterRange::contains(double value) const
{
    if (!qIsFinite(value))
        return false;
    if (hasMinimum && value < minimum)
        return false;
    if (hasMaximum && value > maximum)
        return false;
    return true;
}

ParameterValue ParameterValue::boolean(bool value)
{
    ParameterValue parameter;
    parameter.kind = ParameterValueKind::Boolean;
    parameter.booleanValue = value;
    return parameter;
}

ParameterValue ParameterValue::integer(qint64 value, const QString &unit)
{
    ParameterValue parameter;
    parameter.kind = ParameterValueKind::Integer;
    parameter.integerValue = value;
    parameter.unit = unit;
    return parameter;
}

ParameterValue ParameterValue::real(double value, const QString &unit)
{
    ParameterValue parameter;
    parameter.kind = ParameterValueKind::Real;
    parameter.realValue = value;
    parameter.unit = unit;
    return parameter;
}

ParameterValue ParameterValue::text(const QString &value)
{
    ParameterValue parameter;
    parameter.kind = ParameterValueKind::Text;
    parameter.textValue = value;
    return parameter;
}

ParameterValue ParameterValue::enumeration(const QString &token)
{
    ParameterValue parameter;
    parameter.kind = ParameterValueKind::Enumeration;
    parameter.textValue = token;
    return parameter;
}

bool ParameterValue::isValid() const
{
    switch (kind) {
    case ParameterValueKind::Unspecified:
        return false;
    case ParameterValueKind::Boolean:
        return true;
    case ParameterValueKind::Integer:
        return true;
    case ParameterValueKind::Real:
        return qIsFinite(realValue);
    case ParameterValueKind::Text:
        return !textValue.isEmpty();
    case ParameterValueKind::Enumeration:
        return !textValue.isEmpty();
    }
    return false;
}

bool ParameterValue::isNumeric() const
{
    return kind == ParameterValueKind::Integer || kind == ParameterValueKind::Real;
}

ParameterEntry ParameterEntry::requiredReal(const QString &key, double value,
                                            const QString &unit)
{
    ParameterEntry entry;
    entry.key = key;
    entry.value = ParameterValue::real(value, unit);
    entry.required = true;
    return entry;
}

ParameterEntry ParameterEntry::optionalReal(const QString &key, double value,
                                            const QString &unit)
{
    ParameterEntry entry;
    entry.key = key;
    entry.value = ParameterValue::real(value, unit);
    return entry;
}

ParameterEntry ParameterEntry::requiredBoolean(const QString &key, bool value)
{
    ParameterEntry entry;
    entry.key = key;
    entry.value = ParameterValue::boolean(value);
    entry.required = true;
    return entry;
}

bool ParameterEntry::isValid() const
{
    return !key.isEmpty() && value.isValid() && range.isValid();
}

bool ParameterEntry::isApplicable() const
{
    return applicabilityRule.isEmpty();
}

bool ParameterEntry::valueWithinRange() const
{
    if (!value.isNumeric())
        return true;
    const double numeric = value.kind == ParameterValueKind::Integer
        ? static_cast<double>(value.integerValue) : value.realValue;
    return range.contains(numeric);
}

bool ParameterCategoryConfig::isValid() const
{
    for (const ParameterEntry &entry : entries) {
        if (!entry.isValid())
            return false;
    }
    return hasUniqueKeys();
}

bool ParameterCategoryConfig::hasUniqueKeys() const
{
    return entriesHaveUniqueKeys(entries);
}

const ParameterEntry *ParameterCategoryConfig::find(const QString &key) const
{
    for (const ParameterEntry &entry : entries) {
        if (entry.key == key)
            return &entry;
    }
    return nullptr;
}

bool SolverConfigPayload::isValid() const
{
    if (solverId.isEmpty() || schemaId.isEmpty())
        return false;
    for (const ParameterEntry &entry : entries) {
        if (!entry.isValid())
            return false;
    }
    return hasUniqueKeys();
}

bool SolverConfigPayload::hasUniqueKeys() const
{
    return entriesHaveUniqueKeys(entries);
}

bool ParameterSnapshot::isIdentified() const
{
    return !ref.snapshotId.isEmpty() && ref.revision > 0;
}

bool ParameterSnapshot::isPinned() const
{
    return isIdentified() && state.isValid();
}

bool ParameterSnapshot::scopeMirrorsState() const
{
    // The legacy ScopeRef.caseId is only a projection of the State identity and
    // must either be absent or equal to state.stateId.
    return ref.scope.caseId.isEmpty() || ref.scope.caseId == state.stateId;
}

bool ParameterSnapshot::isValid() const
{
    if (!isPinned() || !scopeMirrorsState() || !fingerprint.isValid())
        return false;
    for (const ParameterCategoryConfig &config : categories) {
        if (!config.isValid())
            return false;
    }
    for (const SolverConfigPayload &payload : solverConfigs) {
        if (!payload.isValid())
            return false;
    }
    return true;
}

const ParameterCategoryConfig *
ParameterSnapshot::category(ParameterCategory category) const
{
    for (const ParameterCategoryConfig &config : categories) {
        if (config.category == category)
            return &config;
    }
    return nullptr;
}

const ParameterEntry *ParameterSnapshot::entry(ParameterCategory category,
                                               const QString &key) const
{
    const ParameterCategoryConfig *config = this->category(category);
    return config ? config->find(key) : nullptr;
}

bool StateConfig::isValid() const
{
    return ref.isValid() && !name.isEmpty() && !solverId.isEmpty();
}

bool StateConfig::isComputable() const
{
    if (!isValid() || !computationMesh.isValid())
        return false;
    return !parameterSnapshot.snapshotId.isEmpty()
        && parameterSnapshot.revision > 0;
}

bool BatchPlanItem::isValid() const
{
    return !itemId.isEmpty() && state.isValid();
}

bool BatchPlan::isValid() const
{
    if (!ref.isValid() || items.isEmpty())
        return false;
    for (const BatchPlanItem &planItem : items) {
        if (!planItem.isValid())
            return false;
    }
    return hasUniqueItemIds() && hasUniqueOrders() && hasUniqueStates();
}

bool BatchPlan::hasUniqueItemIds() const
{
    for (int i = 0; i < items.size(); ++i) {
        for (int j = i + 1; j < items.size(); ++j) {
            if (items.at(i).itemId == items.at(j).itemId)
                return false;
        }
    }
    return true;
}

bool BatchPlan::hasUniqueOrders() const
{
    for (int i = 0; i < items.size(); ++i) {
        for (int j = i + 1; j < items.size(); ++j) {
            if (items.at(i).order == items.at(j).order)
                return false;
        }
    }
    return true;
}

bool BatchPlan::hasUniqueStates() const
{
    for (int i = 0; i < items.size(); ++i) {
        for (int j = i + 1; j < items.size(); ++j) {
            if (items.at(i).state.sameIdentity(items.at(j).state))
                return false;
        }
    }
    return true;
}

const BatchPlanItem *BatchPlan::item(const QString &itemId) const
{
    for (const BatchPlanItem &planItem : items) {
        if (planItem.itemId == itemId)
            return &planItem;
    }
    return nullptr;
}

bool BatchPlanHistory::isValid() const
{
    if (projectId.isEmpty() || planId.isEmpty() || revisions.isEmpty())
        return false;
    quint64 expected = kInitialRevision;
    for (const BatchPlan &plan : revisions) {
        if (plan.ref.projectId != projectId || plan.ref.planId != planId)
            return false;
        if (plan.ref.revision != expected || !plan.isValid())
            return false;
        expected = aerosim::nextRevision(expected);
    }
    return true;
}

const BatchPlan *BatchPlanHistory::latest() const
{
    return revisions.isEmpty() ? nullptr : &revisions.last();
}

const BatchPlan *BatchPlanHistory::findRevision(quint64 revision) const
{
    for (const BatchPlan &plan : revisions) {
        if (plan.ref.revision == revision)
            return &plan;
    }
    return nullptr;
}

quint64 BatchPlanHistory::nextRevision() const
{
    return aerosim::nextRevision(revisions.isEmpty()
        ? 0 : revisions.last().ref.revision);
}

bool BatchPlanHistory::appendRevision(const BatchPlan &plan)
{
    if (!plan.isValid())
        return false;
    if (projectId.isEmpty() || planId.isEmpty())
        return false;
    if (plan.ref.projectId != projectId || plan.ref.planId != planId)
        return false;
    if (plan.ref.revision != nextRevision())
        return false;
    if (findRevision(plan.ref.revision) != nullptr)
        return false;
    revisions.append(plan);
    return true;
}

bool BatchPlanExecutionSnapshot::isValid() const
{
    if (!resource.isValid() || resource.type != ResourceType::BatchPlanSnapshot)
        return false;
    if (!resource.fingerprint.isComputed())
        return false;
    if (!sourcePlan.isValid() || resource.projectId != sourcePlan.projectId)
        return false;
    if (items.isEmpty())
        return false;
    for (const BatchPlanItem &planItem : items) {
        if (!planItem.isValid())
            return false;
    }
    return true;
}

bool BatchPlanExecutionSnapshot::matches(const BatchPlan &plan) const
{
    if (!sourcePlan.sameVersion(plan.ref))
        return false;
    if (items.size() != plan.items.size())
        return false;
    for (int i = 0; i < items.size(); ++i) {
        const BatchPlanItem &pinned = items.at(i);
        const BatchPlanItem &current = plan.items.at(i);
        if (pinned.itemId != current.itemId || pinned.order != current.order)
            return false;
        if (!pinned.state.sameVersion(current.state))
            return false;
    }
    return true;
}

} // namespace aerosim
