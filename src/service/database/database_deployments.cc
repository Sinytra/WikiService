#include "database.h"
#include "service/deployment.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    Task<PaginatedData<DeploymentData>> Database::getDeployments(const std::string projectId, const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT id, project_id, revision ->> 'hash' hash, revision ->> 'message' message, status, user_id, \
                                              active, created_at FROM deployment \
                                       WHERE project_id = $1 \
                                       ORDER BY created_at DESC";

        co_return co_await handlePaginatedQuery<DeploymentData>(
            query, page,
            [](const Row &row) {
                DeploymentData data;
                data.id = row.at("id").as<std::string>();
                data.project_id = row.at("project_id").as<std::string>();
                data.commit_hash = row.at("hash").as<std::string>();
                data.commit_message = row.at("message").as<std::string>();
                data.status = row.at("status").as<std::string>();
                data.user_id = row.at("user_id").as<std::string>();
                const auto createdAt = row.at("created_at").as<std::string>();
                data.created_at = trantor::Date::fromDbString(createdAt).toCustomFormattedString("%Y-%m-%dT%H:%M:%SZ");
                data.active = row.at("active").as<bool>();
                return data;
            },
            projectId);
    }

    Task<std::optional<Deployment>> Database::getActiveDeployment(const std::string projectId) const {
        const auto [res, err] = co_await handleDatabaseOperation<Deployment>([projectId](const DbClientPtr &client) -> Task<Deployment> {
            CoroMapper<Deployment> mapper(client);
            mapper.orderBy(Deployment::Cols::_created_at, SortOrder::DESC);
            mapper.limit(1);
            co_return co_await mapper.findOne(
                Criteria(Deployment::Cols::_project_id, CompareOperator::EQ, projectId) &&
                Criteria(Deployment::Cols::_status, CompareOperator::EQ, enumToStr(DeploymentStatus::SUCCESS)));
        });
        co_return res;
    }

    Task<std::optional<Deployment>> Database::getLoadingDeployment(std::string projectId) const {
        const auto [res, err] = co_await handleDatabaseOperation<Deployment>([projectId](const DbClientPtr &client) -> Task<Deployment> {
            CoroMapper<Deployment> mapper(client);
            mapper.limit(1);
            co_return co_await mapper.findOne(
                Criteria(Deployment::Cols::_project_id, CompareOperator::EQ, projectId) &&
                Criteria(Deployment::Cols::_status, CompareOperator::In,
                         std::vector{enumToStr(DeploymentStatus::CREATED), enumToStr(DeploymentStatus::LOADING)}));
        });
        co_return res;
    }

    Task<Error> Database::deactivateDeployments(std::string projectId) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([projectId](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<Deployment> mapper(client);
            co_await mapper.updateBy({Deployment::Cols::_active}, Criteria(Deployment::Cols::_project_id, CompareOperator::EQ, projectId),
                                     false);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> Database::deleteDeployment(const std::string id) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&id](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<Deployment> mapper(client);
            const auto rows = co_await mapper.deleteByPrimaryKey(id);
            co_return rows < 1 ? Error::ErrInternal : Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<bool> Database::hasFailingDeployment(const std::string projectId) const {
        const auto [res, err] = co_await handleDatabaseOperation<bool>([projectId](const DbClientPtr &client) -> Task<bool> {
            CoroMapper<Deployment> mapper(client);
            mapper.orderBy(Deployment::Cols::_created_at, SortOrder::DESC);
            mapper.limit(1);
            const auto results = co_await mapper.findBy(Criteria(Deployment::Cols::_project_id, CompareOperator::EQ, projectId));
            co_return !results.empty() && results.front().getValueOfStatus() == enumToStr(DeploymentStatus::ERROR);
        });
        co_return res.value_or(false);
    }

    // TODO Delete failing deployments folders
    Task<Error> Database::failLoadingDeployments() const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<Deployment> mapper(client);
            co_await mapper.updateBy({Deployment::Cols::_status},
                                     Criteria(Deployment::Cols::_status, CompareOperator::In,
                                              std::vector{enumToStr(DeploymentStatus::CREATED), enumToStr(DeploymentStatus::LOADING)}),
                                     enumToStr(DeploymentStatus::ERROR));
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<std::vector<std::string>> Database::getUndeployedProjects() const {
        // language=postgresql
        static constexpr auto query = "SELECT project.id FROM project \
                                       WHERE NOT exists( \
                                           SELECT * FROM deployment \
                                           WHERE project_id = project.id \
                                               AND active = TRUE \
                                       )";

        const auto [res, err] = co_await handleDatabaseOperation<std::vector<std::string>>([](const DbClientPtr &client) -> Task<std::vector<std::string>> {
            std::vector<std::string> ids;

            const auto rows = co_await client->execSqlCoro(query);
            for (auto &row : rows) {
                const auto id = row[0].as<std::string>();
                ids.push_back(id);
            }

            co_return ids;
        });
        co_return res.value_or(std::vector<std::string>{});
    }
}
