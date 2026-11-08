#pragma once
#include "aerosim/contracts/WorkflowModels.h"
namespace aerosim { namespace workflow {
WorkflowView toLegacyWorkflowView(const WorkflowViewV2 &view);
} }
