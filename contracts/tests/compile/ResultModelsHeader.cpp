#include "aerosim/contracts/ResultModels.h"

#include <type_traits>

static_assert(std::is_copy_constructible<aerosim::ResultDescriptor>::value,
    "ResultDescriptor must remain a copyable value type");
static_assert(std::is_copy_constructible<aerosim::DatasetDescriptor>::value,
    "DatasetDescriptor must remain a copyable value type");
static_assert(std::is_copy_constructible<aerosim::PostProcessScene>::value,
    "PostProcessScene must remain a copyable value type");

int compileResultModelsHeader()
{
    aerosim::DatasetDescriptor dataset;
    dataset.fields.append(aerosim::FieldDescriptor());
    aerosim::PostProcessScene scene;
    scene.objects.append(aerosim::SceneObjectDescriptor());
    return dataset.fields.size() == 1 && scene.objects.size() == 1 ? 0 : 1;
}
