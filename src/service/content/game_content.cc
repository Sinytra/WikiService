#include "game_content.h"

#include <database.h>
#include <drogon/drogon.h>
#include <fstream>
#include <resolved_db.h>

#define EXT_JSON ".json"
#define CONTENT_DIR ".content/"

using namespace drogon;
using namespace drogon::orm;
using namespace logging;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    SubIngestor::SubIngestor(const ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log) : project_(proj), logger_(log) {}

    Task<Error> SubIngestor::finish() {
        co_return Error::Ok;
    }

    Ingestor::Ingestor(ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log) : project_(proj), logger_(log) {}

    Task<Error> wipeExistingData(const DbClientPtr clientPtr, const std::string id) {
        logger.info("Wiping existing data for project {}", id);
        // language=postgresql
        co_await clientPtr->execSqlCoro(
            "DELETE FROM recipe WHERE version_id IN (SELECT id FROM project_version WHERE project_id = $1)",
            id);
        // language=postgresql
        co_await clientPtr->execSqlCoro(
            "DELETE FROM project_item WHERE version_id IN (SELECT id FROM project_version WHERE project_id = $1)",
            id);
        // language=postgresql
        co_await clientPtr->execSqlCoro(
            "DELETE FROM project_tag WHERE version_id IN (SELECT id FROM project_version WHERE project_id = $1)",
            id);
        co_return Error::Ok;
    }

    Task<Error> Ingestor::ingestGameContentData() const {
        auto projectLog = *logger_;

        if (!project_.getProject().getModid()) {
            projectLog.debug("No content data to ingest.");
            co_return Error::Ok;
        }

        const auto projectId = project_.getProject().getValueOfId();
        const auto projectModid = project_.getProject().getValueOfModid();
        projectLog.info("====================================");
        projectLog.info("Ingesting game data for project {}", projectId);

        const auto clientPtr = app().getFastDbClient();

        co_await wipeExistingData(clientPtr, projectId);

        std::map<std::string, std::unique_ptr<SubIngestor>> ingestors;
        ingestors.emplace("Content paths", std::make_unique<ContentPathsSubIngestor>(project_, logger_));
        ingestors.emplace("Tags", std::make_unique<TagsSubIngestor>(project_, logger_));
        ingestors.emplace("Recipes", std::make_unique<RecipesSubIngestor>(project_, logger_));

        // Prepare ingestors
        PreparationResult allResults;
        for (const auto &[name, ingestor] : ingestors) {
            projectLog.info("Preparing ingestor [{}]", name);
            try {
                const auto [items] = co_await ingestor->prepare();
                allResults.items.insert(items.begin(), items.end());
            } catch (std::exception e) {
                projectLog.critical("Encountered exception while preparing ingestor: {}", e.what());
            }
        }

        std::set<std::string> candidateItems;
        for (const auto &item : allResults.items) {
            if (const auto parsedId = ResourceLocation::parse(item); parsedId && parsedId->namespace_ == projectModid) {
                candidateItems.insert(item);
            }
        }

        // Register items
        if (!candidateItems.empty()) {
            projectLog.info("Registering {} items", candidateItems.size());

            for (const auto &item : candidateItems) {
                co_await project_.getProjectDatabase().addProjectItem(item);
            }

            projectLog.info("Done registering items");
        }

        // Execute ingestors
        for (const auto &[name, ingestor] : ingestors) {
            projectLog.info("Executing ingestor [{}]", name);
            try {
                if (const auto error = co_await ingestor->execute(); error == Error::Ok) {
                    projectLog.debug("Ingestor executed successfully");
                } else {
                    projectLog.error("Encountered error while executing ingestor");
                }
            } catch (std::exception e) {
                projectLog.critical("Encountered exception while executing ingestor: {}", e.what());
            }
        }

        // Finish
        for (const auto &[name, ingestor] : ingestors) {
            projectLog.info("Finishing ingestor [{}]", name);
            try {
                if (const auto error = co_await ingestor->finish(); error == Error::Ok) {
                    projectLog.debug("Ingestor finished successfully");
                } else {
                    projectLog.error("Encountered error while finishing ingestor");
                }
            } catch (std::exception e) {
                projectLog.critical("Encountered exception while finishing ingestor: {}", e.what());
            }
        }

        co_return Error::Ok;
    }
}
