#include "aerosim/contracts/EnumTokens.h"

int compileEnumTokensHeader()
{
    return aerosim::enumToken(aerosim::AccessStatus::Ok).isEmpty() ? 1 : 0;
}
