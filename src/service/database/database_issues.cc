#include <service/project_issue.h>
#include "database.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    Task<std::optional<ProjectIssue>> Database::getProjectIssue(const std::string deploymentId, const ProjectIssueLevel level,
                                                                const ProjectIssueType type, const std::string path) const {
        const auto [res, err] = co_await handleDatabaseOperation<ProjectIssue>(
            [deploymentId, path, level, type](const DbClientPtr &client) -> Task<ProjectIssue> {
                CoroMapper<ProjectIssue> mapper(client);
                mapper.limit(1);

                auto criteria{
                    Criteria(ProjectIssue::Cols::_deployment_id, CompareOperator::EQ, deploymentId)
                    && Criteria(ProjectIssue::Cols::_level, CompareOperator::EQ, enumToStr(level))
                    && Criteria(ProjectIssue::Cols::_type, CompareOperator::EQ, enumToStr(type))
                };
                if (!path.empty()) {
                    criteria = criteria && Criteria(ProjectIssue::Cols::_file, CompareOperator::EQ, path);
                }

                co_return co_await mapper.findOne(criteria);
            });
        co_return res;
    }

    Task<std::vector<ProjectIssue>> Database::getDeploymentIssues(const std::string deploymentId) const {
        const auto [res, err] = co_await handleDatabaseOperation<std::vector<ProjectIssue>>(
            [deploymentId](const DbClientPtr &client) -> Task<std::vector<ProjectIssue>> {
                CoroMapper<ProjectIssue> mapper(client);
                mapper.orderBy("array_position(array['error', 'warning'], level)");
                co_return co_await mapper.findBy(Criteria(ProjectIssue::Cols::_deployment_id, CompareOperator::EQ, deploymentId));
            });
        co_return res.value_or(std::vector<ProjectIssue>{});
    }

    Task<std::unordered_map<std::string, int64_t>> Database::getActiveProjectIssueStats(std::string projectId) const {
        // language=postgresql
        static constexpr auto query = "SELECT level, count(level) AS count FROM project_issue \
                                       WHERE deployment_id = (SELECT id FROM deployment d WHERE d.project_id = $1 AND d.active) \
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
