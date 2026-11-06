#include "aerosim/contracts/ResourceModels.h"

int compileResourceModelsHeader()
{
    aerosim::ResourceRefV2 resource;
    return resource.isValid() ? 1 : 0;
}
