#include "aerosim/contracts/AccessResult.h"

int compileAccessResultHeader()
{
    const aerosim::AccessResult<QString> result =
        aerosim::AccessResult<QString>::success(QStringLiteral("ok"));
    return result.ok() && result.hasValue() ? 0 : 1;
}
