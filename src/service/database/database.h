#pragma once

#include <drogon/HttpAppFramework.h>
#include <drogon/utils/coroutine.h>
#include <log/log.h>
#include <models/Recipe.h>
#include <models/User.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <service/error.h>
#include "database_base.h"

#include <models/ProjectVersion.h>

#include <service/project_issue.h>
#include "models/DataImport.h"
#include "models/Deployment.h"
#include "models/ProjectIssue.h"

#include "models/Report.h"

using namespace drogon_model::postgres;

namespace service {
    struct ProjectSearchResponse {
        int pages;
        int total;
        std::vector<Project> data;
    };
    struct GlobalItem {
        int64_t version_id;
        std::string version_name;
        std::string project_id;
        std::string loc;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(GlobalItem, project_id, loc, version_id, version_name)
    };
    struct ContentUsage {
        int64_t id;
        std::string loc;
        std::string project;
        std::string path;
    };
    struct DeploymentData {
        std::string id;
        std::string project_id;
        std::string commit_hash;
        std::string commit_message;
        std::string status;
        std::string user_id;
        std::string created_at;
        bool current;

        friend void to_json(nlohmann::json &j, const DeploymentData &d) {
            j = nlohmann::json{{"id", d.id},
                               {"project_id", d.project_id},
                               {"commit_hash", d.commit_hash},
                               {"commit_message", d.commit_message},
                               {"status", d.status},
                               {"user_id", d.user_id.empty() ? nlohmann::json(nullptr) : nlohmann::json(d.user_id)},
                               {"created_at", d.created_at},
                               {"current", d.current}};
        }
    };

    class Database : public DatabaseBase {
    public:
        explicit Database();
        explicit Database(const drogon::orm::DbClientPtr &);

        drogon::orm::DbClientPtr getDbClientPtr() const override;

        drogon::Task<std::vector<std::string>> getProjectIDs() const;
        drogon::Task<std::tuple<std::optional<Project>, Error>> createProject(const Project &project) const;
        drogon::Task<Error> removeProject(const std::string &id) const;
        drogon::Task<std::tuple<std::optional<ProjectVersion>, Error>> createProjectVersion(ProjectVersion version) const;
        drogon::Task<std::optional<ProjectVersion>> getProjectVersion(std::string project, std::string name) const;
        drogon::Task<std::optional<ProjectVersion>> getDefaultProjectVersion(std::string project) const;
        drogon::Task<Error> deleteProjectVersions(std::string project) const;

        drogon::Task<std::tuple<std::optional<Project>, Error>> getProjectSource(std::string id) const;

        drogon::Task<std::tuple<ProjectSearchResponse, Error>> findProjects(std::string query, std::string types, std::string sort,
                                                                            int page) const;

        drogon::Task<bool> existsForRepo(std::string repo, std::string branch, std::string path) const;

        drogon::Task<bool> existsForData(std::string id, nlohmann::json platforms) const;

        drogon::Task<Error> createUserIfNotExists(std::string username) const;
        drogon::Task<Error> deleteUserProjects(std::string username) const;
        drogon::Task<Error> deleteUser(std::string username) const;

        drogon::Task<Error> linkUserModrinthAccount(std::string username, std::string mrAccountId) const;
        drogon::Task<Error> unlinkUserModrinthAccount(std::string username) const;
        drogon::Task<std::vector<Project>> getUserProjects(std::string username) const;
        drogon::Task<std::optional<Project>> getUserProject(std::string username, std::string id) const;
        drogon::Task<Error> assignUserProject(std::string username, std::string id, std::string role) const;

        drogon::Task<Error> addItem(std::string item) const;
        drogon::Task<Error> addTag(std::string tag) const;
        drogon::Task<Error> addTagItemEntry(std::string tag, std::string item) const;
        drogon::Task<Error> addTagTagEntry(std::string parentTag, std::string childTag) const;
        drogon::Task<int64_t> addRecipe(std::string id, std::string type) const;
        drogon::Task<Error> addRecipeIngredientItem(int64_t recipe_id, std::string item, int slot, int count, bool input) const;
        drogon::Task<Error> addRecipeIngredientTag(int64_t recipe_id, std::string tag, int slot, int count, bool input) const;

        drogon::Task<std::vector<std::string>> getItemSourceProjects(int64_t item) const;
        drogon::Task<std::vector<GlobalItem>> getGlobalTagItems(int64_t tagId) const;
        drogon::Task<std::vector<Recipe>> getItemUsageInRecipes(std::string item) const;
        drogon::Task<std::vector<ContentUsage>> getObtainableItemsBy(std::string item) const;

        drogon::Task<std::optional<DataImport>> addDataImportRecord(DataImport data) const;
        drogon::Task<PaginatedData<DataImport>> getDataImports(std::string searchQuery, int page) const;

        drogon::Task<PaginatedData<DeploymentData>> getDeployments(std::string projectId, int page) const;
        drogon::Task<std::optional<Deployment>> getActiveDeployment(std::string projectId) const;
        drogon::Task<std::optional<Deployment>> getLoadingDeployment(std::string projectId) const;
        drogon::Task<Error> deactivateDeployments(std::string projectId) const;
        drogon::Task<Error> deleteDeployment(std::string id) const;
        drogon::Task<bool> hasFailingDeployment(std::string projectId) const;
        drogon::Task<Error> failLoadingDeployments() const;

        drogon::Task<std::optional<ProjectIssue>> getProjectIssue(std::string deploymentId, ProjectIssueLevel level,
                                                                  ProjectIssueType type, std::string path) const;
        drogon::Task<std::vector<ProjectIssue>> getDeploymentIssues(std::string deploymentId) const;
        drogon::Task<std::unordered_map<std::string, int64_t>> getActiveProjectIssueStats(std::string projectId) const;

        drogon::Task<PaginatedData<Report>> getReports(int page) const;
    private:
        drogon::orm::DbClientPtr clientPtr_;
    };

    template<typename Ret>
    drogon::Task<std::tuple<std::optional<Ret>, Error>>
    executeTransaction(const std::function<drogon::Task<Ret>(const Database &client)> &func) {
        try {
            const auto clientPtr = drogon::app().getFastDbClient();
            const auto transPtr = co_await clientPtr->newTransactionCoro();
            const Database transDb{transPtr};

            const Ret result = co_await func(transDb);
            co_return {result, Error::Ok};
        } catch (const drogon::orm::Failure &e) {
            logging::logger.error("Error executing transaction: {}", e.what());
            co_return {std::nullopt, Error::ErrInternal};
        } catch ([[maybe_unused]] const drogon::orm::DrogonDbException &e) {
            co_return {std::nullopt, Error::ErrNotFound};
        }
    }
}

namespace global {
    extern std::shared_ptr<service::Database> database;
}
