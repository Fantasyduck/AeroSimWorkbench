#include "TestSupport.h"

#include "aerosim/contracts/AccessResult.h"

int main()
{
    aerosim_test::TestContext context;

    const aerosim::AccessResult<QString> unset;
    AEROSIM_CHECK(context, !unset.ok());
    AEROSIM_CHECK(context, !unset.hasValue());
    AEROSIM_CHECK(context, unset.status() == aerosim::AccessStatus::InternalError);

    const aerosim::AccessResult<QString> success =
        aerosim::AccessResult<QString>::success(QStringLiteral("resource"));
    AEROSIM_CHECK(context, success.ok());
    AEROSIM_CHECK(context, success.hasValue());
    AEROSIM_CHECK(context, success.value() == QStringLiteral("resource"));
    AEROSIM_CHECK(context, success.errorCode().isEmpty());

    const aerosim::AccessResult<QString> missing =
        aerosim::AccessResult<QString>::failure(
            aerosim::AccessStatus::NotFound,
            QStringLiteral("project.resource.not_found"),
            QStringLiteral("Resource was not found"));
    AEROSIM_CHECK(context, !missing.ok());
    AEROSIM_CHECK(context, !missing.hasValue());
    AEROSIM_CHECK(context, missing.status() == aerosim::AccessStatus::NotFound);
    AEROSIM_CHECK(context, missing.errorCode() == QStringLiteral("project.resource.not_found"));

    const aerosim::AccessResult<QString> invalidFailure =
        aerosim::AccessResult<QString>::failure(
            aerosim::AccessStatus::Ok, QString(), QString());
    AEROSIM_CHECK(context, !invalidFailure.ok());
    AEROSIM_CHECK(context,
        invalidFailure.status() == aerosim::AccessStatus::InternalError);
    AEROSIM_CHECK(context, !invalidFailure.errorCode().isEmpty());

    return context.result();
}
