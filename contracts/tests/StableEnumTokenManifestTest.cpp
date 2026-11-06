#include "TestSupport.h"

#include "aerosim/contracts/TaskModels.h"
#include "aerosim/contracts/Telemetry.h"
#include "aerosim/contracts/WorkflowModels.h"

#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace {

struct TokenSet {
    const char *typeName;
    std::vector<const char *> tokens;
};

} // namespace

int main()
{
    aerosim_test::TestContext context;

    // This is the version-1 persistence token manifest. PR 0 intentionally
    // does not add production serializers. Later mapping implementations must
    // round-trip these exact lowercase tokens instead of enum ordinals.
    const std::vector<TokenSet> manifest = {
        {"IssueSeverity", {"info", "warning", "error"}},
        {"StepState", {"pending", "needs_validation", "complete", "blocked"}},
        {"TaskCommandType", {"start", "pause", "resume", "stop", "cancel"}},
        {"TaskEventType", {"prepared", "started", "status_changed", "progress_updated",
                           "residual_updated", "log_generated", "warning", "failed",
                           "finished", "result_generated"}},
        {"TaskStatus", {"unknown", "preparing", "running", "paused", "finished",
                        "failed", "cancelled"}},
        {"LogLevel", {"debug", "info", "warning", "error"}}
    };

    AEROSIM_CHECK(context, manifest[0].tokens.size()
        == static_cast<std::size_t>(aerosim::IssueSeverity::Error) + 1);
    AEROSIM_CHECK(context, manifest[1].tokens.size()
        == static_cast<std::size_t>(aerosim::StepState::Blocked) + 1);
    AEROSIM_CHECK(context, manifest[2].tokens.size()
        == static_cast<std::size_t>(aerosim::TaskCommandType::Cancel) + 1);
    AEROSIM_CHECK(context, manifest[3].tokens.size()
        == static_cast<std::size_t>(aerosim::TaskEventType::ResultGenerated) + 1);
    AEROSIM_CHECK(context, manifest[4].tokens.size()
        == static_cast<std::size_t>(aerosim::TaskStatus::Cancelled) + 1);
    AEROSIM_CHECK(context, manifest[5].tokens.size()
        == static_cast<std::size_t>(aerosim::LogLevel::Error) + 1);

    std::set<std::string> typeNames;
    for (const TokenSet &tokenSet : manifest) {
        AEROSIM_CHECK(context, tokenSet.typeName != nullptr);
        AEROSIM_CHECK(context, typeNames.insert(tokenSet.typeName).second);
        AEROSIM_CHECK(context, !tokenSet.tokens.empty());

        std::set<std::string> uniqueTokens;
        for (const char *token : tokenSet.tokens) {
            AEROSIM_CHECK(context, token != nullptr);
            const std::string value = token == nullptr ? std::string() : std::string(token);
            AEROSIM_CHECK(context, !value.empty());
            AEROSIM_CHECK(context, uniqueTokens.insert(value).second);
            for (char character : value) {
                const bool stableCharacter = (character >= 'a' && character <= 'z')
                    || (character >= '0' && character <= '9') || character == '_';
                AEROSIM_CHECK(context, stableCharacter);
            }
        }
    }

    return context.result();
}
