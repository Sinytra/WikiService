#include "ingestor_metadata.h"
#include "database/resolved_db.h"
#include "game_content.h"
#include "schemas.h"

using namespace drogon;
using namespace drogon::orm;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    MetadataSubIngestor::MetadataSubIngestor(const ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log,
                                             ProjectIssueCallback &issues) : SubIngestor(proj, log, issues) {}

    Task<PreparationResult> MetadataSubIngestor::prepare() {
        const auto docsRoot = project_.getDocsDirectoryPath();
        const auto workbenchesFile = docsRoot / ".data" / "workbenches.json";
        const ProjectFileIssueCallback issues{issues_, relative(workbenchesFile, docsRoot).string()};

        if (exists(workbenchesFile)) {
            const auto json = parseJsonFile(workbenchesFile);
            if (!json) {
                issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_FILE);
                co_return {};
            }

            if (const auto error = validateJson(schemas::recipeWorkbenches, *json)) {
                issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_FORMAT, error->format());
                co_return {};
            }

            for (const auto &[key, val]: json->items()) {
                const std::vector<std::string> items = val;
                workbenches_.emplace_back(StubWorkbenches{key, items}, issues);
            }
        }

        co_return {};
    }

    Task<Error> MetadataSubIngestor::addWorkbenches(const PreparedData<StubWorkbenches> workbenches) const {
        const auto count = co_await project_.getProjectDatabase().addRecipeWorkbenches(workbenches.data.recipeType, workbenches.data.items);
        logger_->debug("Inserted {} workbenches for type {}", count, workbenches.data.recipeType);
        // TODO Report errors
        co_return count == workbenches.data.items.size() ? Error::Ok : Error::ErrInternal;
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
