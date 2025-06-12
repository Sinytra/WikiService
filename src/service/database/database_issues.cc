#include "database.h"
#include "service/deployment.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    Task<std::optional<ProjectIssue>> Database::addProjectIssue(ProjectIssue issue) const {
        const auto [res, err] = co_await handleDatabaseOperation<ProjectIssue>([&issue](const DbClientPtr &client) -> Task<ProjectIssue> {
            CoroMapper<ProjectIssue> mapper(client);
            co_return co_await mapper.insert(issue);
        });
        co_return res;
    }

    Task<std::optional<ProjectIssueData>> Database::getProjectIssue(std::string id) const {
        const auto [res, err] =
            co_await handleDatabaseOperation<ProjectIssueData>([id](const DbClientPtr &client) -> Task<ProjectIssueData> {
                CoroMapper<ProjectIssue> mapper(client);
                const auto issue = co_await mapper.findByPrimaryKey(id);
                co_return ProjectIssueData{.id = issue.getValueOfId(),
                                           .project_id = issue.getValueOfProjectId(),
                                           .level = issue.getValueOfLevel(),
                                           .page_path = issue.getValueOfPagePath(),
                                           .deployment_id = issue.getValueOfDeploymentId(),
                                           .body = nlohmann::json::parse(issue.getValueOfBody()),
                                           .created_at = issue.getValueOfCreatedAt().toCustomFormattedString("%Y-%m-%d")};
            });
        co_return res;
    }

    Task<std::vector<ProjectIssue>> Database::getProjectIssues(std::string projectId) const {
        const auto [res, err] = co_await handleDatabaseOperation<std::vector<ProjectIssue>>(
            [projectId](const DbClientPtr &client) -> Task<std::vector<ProjectIssue>> {
                CoroMapper<ProjectIssue> mapper(client);
                mapper.orderBy("array_position(array['error', 'warning'], level)");
                co_return co_await mapper.findBy(Criteria(ProjectIssue::Cols::_project_id, CompareOperator::EQ, projectId));
            });
        co_return res.value_or(std::vector<ProjectIssue>{});
    }

    Task<Error> Database::deleteProjectIssues(std::string projectId) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([projectId](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<ProjectIssue> mapper(client);
            co_await mapper.deleteBy(Criteria(ProjectIssue::Cols::_project_id, CompareOperator::EQ, projectId));
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> Database::deleteDeploymentIssues(std::string deploymentId) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([deploymentId](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<ProjectIssue> mapper(client);
            co_await mapper.deleteBy(Criteria(ProjectIssue::Cols::_deployment_id, CompareOperator::EQ, deploymentId));
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<std::unordered_map<std::string, int64_t>> Database::getProjectIssueStats(std::string projectId) const {
        // language=postgresql
        static constexpr auto query = "SELECT level, count(level) AS count FROM project_issue \
                                       WHERE project_id = $1 \
                                       GROUP BY level";

        const auto [res, err] = co_await handleDatabaseOperation<std::unordered_map<std::string, int64_t>>(
            [projectId](const DbClientPtr &client) -> Task<std::unordered_map<std::string, int64_t>> {
                std::unordered_map<std::string, int64_t> map;

                for (const auto rows = co_await client->execSqlCoro(query, projectId); const auto &row: rows) {
                    const auto level = row["level"].as<std::string>();
                    const auto count = row["count"].as<int64_t>();
                    map.emplace(level, count);
                }

                co_return map;
            });
        co_return res.value_or(std::unordered_map<std::string, int64_t>{});
    }
}
