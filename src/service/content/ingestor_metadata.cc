#include "ingestor_metadata.h"
#include "database/resolved_db.h"
#include "ingestor.h"
#include "schemas.h"

using namespace drogon;
using namespace drogon::orm;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    MetadataSubIngestor::MetadataSubIngestor(const ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log,
                                             ProjectFileIssueCallback &issues) : SubIngestor(proj, log, issues) {}

    Task<PreparationResult> MetadataSubIngestor::prepare() {
        const auto docsRoot = project_.getDocsDirectoryPath();
        const auto workbenchesFile = docsRoot / ".data" / "workbenches.json";

        if (exists(workbenchesFile)) {
            const ProjectFileIssueCallback fileIssues{issues_, workbenchesFile};

            const auto json = fileIssues.readAndValidateJson(schemas::recipeWorkbenches);
            if (!json) {
                co_return {};
            }

            for (const auto &[key, val]: json->items()) {
                workbenches_.emplace_back(StubWorkbenches{key, val}, fileIssues);
            }
        }

        co_return {};
    }

    Task<Error> MetadataSubIngestor::addWorkbenches(const PreparedData<StubWorkbenches> workbenches) const {
        const auto count = co_await project_.getProjectDatabase().addRecipeWorkbenches(workbenches.data.recipeType, workbenches.data.items);
        logger_->debug("Inserted {} workbenches for type {}", count, workbenches.data.recipeType);

        if (const auto expected = workbenches.data.items.size(); count != expected) {
            const auto msg = std::format("Expected to insert {} workbenches, was {}", expected, count);
            co_await issues_.addIssue(ProjectIssueLevel::WARNING, ProjectIssueType::INGESTOR, ProjectError::UNKNOWN, msg);
            co_return Error::ErrInternal;
        }

        co_return Error::Ok;
    }

    Task<Error> MetadataSubIngestor::execute() {
        logger_->info("Adding {} recipes workbenches", workbenches_.size());

        for (const auto &workbench: workbenches_) {
            logger_->trace("Registering {} workbenches for recipe type '{}'", workbench.data.items.size(), workbench.data.recipeType);
            co_await addWorkbenches(workbench);
        }

        co_return Error::Ok;
    }
}
