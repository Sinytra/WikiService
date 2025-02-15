#include "game_content.h"

#include <drogon/drogon.h>
#include <fstream>
#include <models/Recipe.h>

#define EXT_MDX ".mdx"
#define CONTENT_DIR ".content/"

using namespace drogon;
using namespace drogon::orm;
using namespace logging;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    Ingestor::Ingestor(ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log) : project_(proj), logger_(log) {}

    Task<Error> wipeExistingData(const DbClientPtr clientPtr, const std::string id) {
        logger.info("Wiping existing data for project {}", id);
        co_await clientPtr->execSqlCoro("DELETE FROM recipe WHERE project_id = $1", id);
        co_await clientPtr->execSqlCoro("DELETE FROM item WHERE project_id = $1", id);
        co_await clientPtr->execSqlCoro("DELETE FROM tag WHERE project_id = $1", id);
        co_return Error::Ok;
    }

    Task<> addProjectItem(const DbClientPtr clientPtr, std::string project, std::string item) {
        co_await clientPtr->execSqlCoro("INSERT INTO item_id VALUES ($1) ON CONFLICT DO NOTHING", item);
        co_await clientPtr->execSqlCoro("INSERT INTO item VALUES (DEFAULT, $1, $2) ON CONFLICT DO NOTHING", item, project);
    }

    Task<Error> Ingestor::ingestContentPaths() const {
        const auto clientPtr = app().getFastDbClient();

        const auto docsRoot = project_.getDocsDirectoryPath();
        const auto projectId = project_.getProject().getValueOfId();
        std::set<std::string> items;

        for (const auto &entry: fs::recursive_directory_iterator(docsRoot)) {
            const auto fileName = entry.path().filename().string();
            if (!entry.is_regular_file() || entry.path().extension() != EXT_MDX || fileName.starts_with(".") && !fileName.starts_with(CONTENT_DIR)) {
                continue;
            }
            fs::path relative_path = relative(entry.path(), docsRoot);
            if (const auto id = project_.readPageAttribute(relative_path.string(), "id")) {
                if (items.contains(*id)) {
                    logger_->warn("Skipping duplicate item {} path {}", *id, relative_path.string());
                    continue;
                }

                logger_->debug("Found entry '{}' at '{}'", *id, relative_path.string());

                items.insert(*id);
                co_await addProjectItem(clientPtr, projectId, *id);
                co_await clientPtr->execSqlCoro("INSERT INTO item_page (id, path) "
                                                "SELECT id, $1 as path FROM item "
                                                "WHERE item_id = $2 AND project_id = $3",
                                                relative_path.string(), *id, projectId);
            }
        }

        logger_->info("Added {} entries", items.size());

        co_return Error::Ok;
    }

    Task<Error> Ingestor::ingestGameContentData() const {
        auto projectLog = *logger_;

        const auto projectId = project_.getProject().getValueOfId();
        projectLog.info("====================================");
        projectLog.info("Ingesting game data for project {}", projectId);

        const auto clientPtr = app().getFastDbClient();

        co_await wipeExistingData(clientPtr, projectId);

        co_await ingestContentPaths();

        co_await ingestRecipes();

        co_return Error::Ok;
    }
}
