#include "ingestor.h"

#include <drogon/drogon.h>
#include <fstream>
#include <service/database/database.h>
#include <service/database/project_database.h>

#define CONTENT_DIR ".content/"

using namespace drogon;
using namespace drogon::orm;
using namespace logging;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    SubIngestor::SubIngestor(const ProjectBase &proj, const std::shared_ptr<spdlog::logger> &log, ProjectFileIssueCallback &issues) :
        project_(proj), logger_(log), issues_(issues) {}

    Task<Error> SubIngestor::finish() { co_return Error::Ok; }

    Ingestor::Ingestor(ProjectBase &proj, const std::shared_ptr<spdlog::logger> &log, const std::shared_ptr<ProjectIssueCallback> &issues,
                       const std::set<std::string> &enableModules, const bool deleteExisting) :
        project_(proj), logger_(log), issues_(issues), enableModules_(enableModules), deleteExisting_(deleteExisting) {}

    Task<Error> wipeExistingData(const DbClientPtr clientPtr, const std::string id) {
        logger.info("Wiping existing data for project {}", id);
        // language=postgresql
        co_await clientPtr->execSqlCoro("DELETE FROM recipe WHERE version_id IN (SELECT id FROM project_version WHERE project_id = $1)",
                                        id);
        // language=postgresql
        co_await clientPtr->execSqlCoro(
            "DELETE FROM recipe_type WHERE version_id IN (SELECT id FROM project_version WHERE project_id = $1)", id);
        // language=postgresql
        co_await clientPtr->execSqlCoro(
            "DELETE FROM project_item WHERE version_id IN (SELECT id FROM project_version WHERE project_id = $1)", id);
        // language=postgresql
        co_await clientPtr->execSqlCoro(
            "DELETE FROM project_tag WHERE version_id IN (SELECT id FROM project_version WHERE project_id = $1)", id);
        co_return Error::Ok;
    }

    Task<Error> Ingestor::runIngestor() const {
        auto projectLog = *logger_;

        if (!project_.getProject().getModid()) {
            projectLog.debug("No content data to ingest.");
            co_return Error::Ok;
        }

        const auto clientPtr = app().getFastDbClient();

        try {
            const auto transPtr = co_await clientPtr->newTransactionCoro();
            project_.getProjectDatabase().setDBClientPointer(transPtr);

            const auto result = co_await ingestGameContentData();

            project_.getProjectDatabase().setDBClientPointer(clientPtr);

            co_return result;
        } catch (std::exception &e) {
            logger.error("Error ingesting project data: {}", e.what());
            issues_->addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::UNKNOWN, e.what());

            project_.getProjectDatabase().setDBClientPointer(clientPtr);
            co_return Error::ErrInternal;
        }
    }

    bool Ingestor::enableModule(const std::string &name) const { return enableModules_.empty() || enableModules_.contains(name); }

    Task<Error> Ingestor::ingestGameContentData() const {
        auto projectLog = *logger_;

        const auto projectId = project_.getProject().getValueOfId();
        const auto projectModid = project_.getProject().getValueOfModid();
        projectLog.info("====================================");
        projectLog.info("Ingesting game data for project {}", projectId);

        if (deleteExisting_) {
            co_await wipeExistingData(project_.getProjectDatabase().getDbClientPtr(), projectId);
        }

        ProjectFileIssueCallback rootFileIssues{issues_, project_.getFormat().getRoot()};
        std::vector<std::pair<std::string, std::unique_ptr<SubIngestor>>> ingestors;

        addSubModule<ContentPathsSubIngestor>(ingestors, INGESTOR_CONTENT_PATHS, rootFileIssues);
        addSubModule<TagsSubIngestor>(ingestors, INGESTOR_TAGS, rootFileIssues);
        addSubModule<RecipesSubIngestor>(ingestors, INGESTOR_RECIPES, rootFileIssues);
        addSubModule<MetadataSubIngestor>(ingestors, INGESTOR_METADATA, rootFileIssues);

        // Prepare ingestors
        PreparationResult allResults;
        for (const auto &[name, ingestor]: ingestors) {
            projectLog.info("Preparing ingestor [{}]", name);
            try {
                const auto [items] = co_await ingestor->prepare();
                allResults.items.insert(items.begin(), items.end());
            } catch (std::exception &e) {
                issues_->addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::UNKNOWN, e.what());
                co_return Error::ErrInternal;
            }
        }

        std::set<std::string> candidateItems;
        for (const auto &item: allResults.items) {
            if (const auto parsedId = ResourceLocation::parse(item); parsedId && parsedId->namespace_ == projectModid) {
                candidateItems.insert(item);
            }
        }

        // Register items
        if (!candidateItems.empty()) {
            projectLog.info("Registering {} items", candidateItems.size());

            for (const auto &item: candidateItems) {
                projectLog.trace("Registering item '{}'", item);
                if (const auto result = co_await project_.getProjectDatabase().addProjectItem(item); !result) {
                    // TODO add issue
                    co_return Error::ErrBadRequest;
                }
            }

            projectLog.debug("Done registering items");
        }

        // Execute ingestors
        for (const auto &[name, ingestor]: ingestors) {
            projectLog.info("Executing ingestor [{}]", name);
            try {
                if (const auto error = co_await ingestor->execute(); error == Error::Ok) {
                    projectLog.debug("Ingestor executed successfully");
                } else {
                    projectLog.error("Encountered error while executing ingestor");
                }
            } catch (std::exception &e) {
                const auto details = std::format("[{}] {}", name, e.what());
                issues_->addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::UNKNOWN, details);
                co_return Error::ErrInternal;
            }
        }

        // Finish
        for (const auto &[name, ingestor]: ingestors) {
            projectLog.info("Finishing ingestor [{}]", name);
            try {
                if (const auto error = co_await ingestor->finish(); error == Error::Ok) {
                    projectLog.debug("Ingestor finished successfully");
                } else {
                    projectLog.error("Encountered error while finishing ingestor");
                }
            } catch (std::exception &e) {
                const auto details = std::format("[{}] {}", name, e.what());
                issues_->addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::UNKNOWN, details);
                co_return Error::ErrInternal;
            }
        }

        co_return Error::Ok;
    }
}
