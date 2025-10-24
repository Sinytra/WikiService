#include "ingestor.h"

#include <database/resolved_db.h>
#include <drogon/drogon.h>

// TODO Use Format
#define CONTENT_DIR ".content/"

using namespace drogon;
using namespace drogon::orm;
using namespace logging;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    ContentPathsSubIngestor::ContentPathsSubIngestor(const ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log,
                                                     ProjectFileIssueCallback &issues) : SubIngestor(proj, log, issues) {}

    Task<PreparationResult> ContentPathsSubIngestor::prepare() {
        PreparationResult result;

        for (const auto docsRoot = project_.getRootDirectory(); const auto &entry: fs::recursive_directory_iterator(docsRoot)) {
            if (const auto fileName = entry.path().filename().string(); !entry.is_regular_file() ||
                                                                        entry.path().extension() != DOCS_FILE_EXT ||
                                                                        fileName.starts_with(".") && !fileName.starts_with(CONTENT_DIR))
            {
                continue;
            }
            const auto relativePath = relative(entry.path(), docsRoot);
            ProjectFileIssueCallback fileIssues{issues_, entry.path()};

            if (const auto pageAttributes = project_.readPageAttributes(relativePath.string());
                pageAttributes && !pageAttributes->id.empty())
            {
                const auto id = pageAttributes->id;
                if (pagePaths_.contains(id)) {
                    logger_->warn("Skipping duplicate page for item {} at {}", id, relativePath.string());
                    co_await fileIssues.addIssue(ProjectIssueLevel::WARNING, ProjectIssueType::INGESTOR, ProjectError::DUPLICATE_PAGE, id);
                    continue;
                }

                if (!fileIssues.validateResourceLocation(id)) {
                    continue;
                }

                logger_->trace("Found page for '{}' at '{}'", id, relativePath.string());

                result.items.insert(id);
                pagePaths_[id] = relativePath.string();
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
