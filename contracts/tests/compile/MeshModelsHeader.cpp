#include "aerosim/contracts/MeshModels.h"

#include <type_traits>

static_assert(std::is_copy_constructible<aerosim::MeshRefV2>::value,
    "MeshRefV2 must remain a copyable value type");

int compileMeshModelsHeader()
{
    aerosim::MeshRefV2 mesh;
    mesh.revision = 1;
    return mesh.isValid() ? 1 : 0;
}
