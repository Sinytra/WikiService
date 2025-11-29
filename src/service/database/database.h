#pragma once

#include "database_base.h"

#include <drogon/HttpAppFramework.h>
#include <drogon/utils/coroutine.h>
#include <log/log.h>
#include <models/DataImport.h>
#include <models/Deployment.h>
#include <models/ProjectIssue.h>
#include <models/ProjectVersion.h>
#include <models/Recipe.h>
#include <models/Report.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <service/error.h>
#include <service/storage/issues.h>

using namespace drogon_model::postgres;

// TODO Hide virtual projects
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
        bool active;

        friend void to_json(nlohmann::json &j, const DeploymentData &d) {
            j = nlohmann::json{{"id", d.id},
                               {"project_id", d.project_id},
                               {"commit_hash", d.commit_hash},
                               {"commit_message", d.commit_message},
                               {"status", d.status},
                               {"user_id", d.user_id.empty() ? nlohmann::json(nullptr) : nlohmann::json(d.user_id)},
                               {"created_at", d.created_at},
                               {"active", d.active}};
        }
    };

    class Database : public DatabaseBase {
    public:
        explicit Database();
        explicit Database(const drogon::orm::DbClientPtr &);

        drogon::orm::DbClientPtr getDbClientPtr() const override;

        drogon::Task<std::vector<std::string>> getProjectIDs() const;
        drogon::Task<TaskResult<Project>> createProject(const Project &project) const;
        drogon::Task<TaskResult<>> removeProject(const std::string &id) const;
        drogon::Task<TaskResult<ProjectVersion>> createProjectVersion(ProjectVersion version) const;
        drogon::Task<TaskResult<ProjectVersion>> getProjectVersion(std::string project, std::string name) const;
        drogon::Task<TaskResult<ProjectVersion>> getDefaultProjectVersion(std::string project) const;
        drogon::Task<TaskResult<>> deleteProjectVersions(std::string project) const;

        drogon::Task<TaskResult<Project>> getProjectSource(std::string id) const;

        drogon::Task<TaskResult<ProjectSearchResponse>> findProjects(std::string query, std::string types, std::string sort,
                                                                     int page) const;

        drogon::Task<bool> existsForRepo(std::string repo, std::string branch, std::string path) const;

        drogon::Task<bool> existsForData(std::string id, nlohmann::json platforms) const;

        drogon::Task<TaskResult<>> createUserIfNotExists(std::string username) const;
        drogon::Task<TaskResult<>> deleteUserProjects(std::string username) const;
        drogon::Task<TaskResult<>> deleteUser(std::string username) const;

        drogon::Task<TaskResult<>> linkUserModrinthAccount(std::string username, std::string mrAccountId) const;
        drogon::Task<TaskResult<>> unlinkUserModrinthAccount(std::string username) const;
        drogon::Task<std::vector<Project>> getUserProjects(std::string username) const;
        drogon::Task<TaskResult<Project>> getUserProject(std::string username, std::string id) const;
        drogon::Task<TaskResult<>> assignUserProject(std::string username, std::string id, std::string role) const;

        drogon::Task<TaskResult<>> refreshFlatTagItemView() const;
        drogon::Task<TaskResult<>> addItem(std::string item) const;
        drogon::Task<TaskResult<>> addTag(std::string tag) const;
        drogon::Task<TaskResult<>> addTagItemEntry(std::string tag, std::string item) const;
        drogon::Task<TaskResult<>> addTagTagEntry(std::string parentTag, std::string childTag) const;
        drogon::Task<int64_t> addRecipe(std::string id, std::string type) const;
        drogon::Task<TaskResult<>> addRecipeIngredientItem(int64_t recipe_id, std::string item, int slot, int count, bool input) const;
        drogon::Task<TaskResult<>> addRecipeIngredientTag(int64_t recipe_id, std::string tag, int slot, int count, bool input) const;

        drogon::Task<std::vector<std::string>> getItemSourceProjects(int64_t item) const;
        drogon::Task<std::vector<GlobalItem>> getGlobalTagItems(int64_t tagId) const;
        drogon::Task<std::vector<Recipe>> getItemUsageInRecipes(std::string item) const;
        drogon::Task<std::vector<ContentUsage>> getObtainableItemsBy(std::string item) const;
        drogon::Task<std::vector<ContentUsage>> getRecipeTypeWorkbenches(int64_t id) const;

        drogon::Task<TaskResult<DataImport>> addDataImportRecord(DataImport data) const;
        drogon::Task<PaginatedData<DataImport>> getDataImports(std::string searchQuery, int page) const;

        drogon::Task<PaginatedData<DeploymentData>> getDeployments(std::string projectId, int page) const;
        drogon::Task<TaskResult<Deployment>> getActiveDeployment(std::string projectId) const;
        drogon::Task<TaskResult<Deployment>> getLoadingDeployment(std::string projectId) const;
        drogon::Task<TaskResult<>> deactivateDeployments(std::string projectId) const;
        drogon::Task<TaskResult<>> deleteDeployment(std::string id) const;
        drogon::Task<bool> hasFailingDeployment(std::string projectId) const;
        drogon::Task<TaskResult<>> failLoadingDeployments() const;
        drogon::Task<std::vector<std::string>> getUndeployedProjects() const;

        drogon::Task<TaskResult<ProjectIssue>> getProjectIssue(std::string deploymentId, ProjectIssueLevel level, ProjectIssueType type,
                                                               std::string path) const;
        drogon::Task<std::vector<ProjectIssue>> getDeploymentIssues(std::string deploymentId) const;
        drogon::Task<std::unordered_map<std::string, int64_t>> getActiveProjectIssueStats(std::string projectId) const;

        drogon::Task<PaginatedData<Report>> getReports(int page) const;

    private:
        drogon::orm::DbClientPtr clientPtr_;
    };

    template<typename F, typename Ret = WrapperInnerType_T<std::invoke_result_t<F, const Database &>>,
             typename Type = std::conditional_t<std::is_void_v<Ret>, std::monostate, Ret>>
    drogon::Task<TaskResult<Type>> executeTransaction(F &&func) {
        try {
            const auto clientPtr = drogon::app().getFastDbClient();
            const auto transPtr = co_await clientPtr->newTransactionCoro();
            const Database transDb{transPtr};

            if constexpr (std::is_void_v<Ret>) {
                co_await func(transDb);
                co_return Error::Ok;
            } else {
                const Ret result = co_await func(transDb);
                co_return result;
            }
        } catch (const drogon::orm::Failure &e) {
            logging::logger.error("Error executing transaction: {}", e.what());
            co_return Error::ErrInternal;
        } catch ([[maybe_unused]] const drogon::orm::DrogonDbException &e) {
            co_return Error::ErrNotFound;
        }
    }
}

namespace global {
    extern std::shared_ptr<service::Database> database;
}
