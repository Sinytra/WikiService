#include "game_content.h"

#include <database/resolved_db.h>
#include <drogon/drogon.h>

#define EXT_MDX ".mdx"
#define CONTENT_DIR ".content/"

using namespace drogon;
using namespace drogon::orm;
using namespace logging;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    ContentPathsSubIngestor::ContentPathsSubIngestor(const ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log,
                                                     ProjectIssueCallback &issues) : SubIngestor(proj, log, issues) {}

    Task<PreparationResult> ContentPathsSubIngestor::prepare() {
        PreparationResult result;

        for (const auto docsRoot = project_.getDocsDirectoryPath(); const auto &entry: fs::recursive_directory_iterator(docsRoot)) {
            if (const auto fileName = entry.path().filename().string(); !entry.is_regular_file() || entry.path().extension() != EXT_MDX ||
                                                                        fileName.starts_with(".") && !fileName.starts_with(CONTENT_DIR))
            {
                continue;
            }
            fs::path relative_path = relative(entry.path(), docsRoot);
            if (const auto id = project_.readPageAttribute(relative_path.string(), "id")) {
                if (pagePaths_.contains(*id)) {
                    logger_->warn("Skipping duplicate page for item {} at {}", *id, relative_path.string());
                    co_await addIssue(ProjectIssueLevel::WARNING, ProjectIssueType::INGESTOR, ProjectError::DUPLICATE_PAGE, *id,
                                      relative_path.string());
                    continue;
                }

                logger_->trace("Found page for '{}' at '{}'", *id, relative_path.string());

                result.items.insert(*id);
                pagePaths_[*id] = relative_path.string();
            }
        }

        logger_->debug("Found {} pages in total", pagePaths_.size());

        co_return result;
    }

    Task<Error> ContentPathsSubIngestor::execute() {
        const auto clientPtr = app().getFastDbClient();

        for (auto &[id, path]: pagePaths_) {
            co_await project_.getProjectDatabase().addProjectContentPage(id, path);
        }

        co_return Error::Ok;
    }

}
