#include "database.h"
#include "service/deployment.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    Task<PaginatedData<DeploymentData>> Database::getDeployments(const std::string projectId, const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT id, project_id, revision ->> 'hash' hash, revision ->> 'message' message, status, user_id, \
                                              created_at FROM deployment \
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
                return data;
            },
            projectId);
    }

    Task<std::optional<Deployment>> Database::getDeployment(const std::string id) const {
        const auto [res, err] = co_await handleDatabaseOperation<Deployment>([id](const DbClientPtr &client) -> Task<Deployment> {
            CoroMapper<Deployment> mapper(client);
            co_return co_await mapper.findByPrimaryKey(id);
        });
        co_return res;
    }

    Task<std::optional<Deployment>> Database::getActiveDeployment(const std::string projectId) const {
        const auto [res, err] = co_await handleDatabaseOperation<Deployment>([projectId](const DbClientPtr &client) -> Task<Deployment> {
            CoroMapper<Deployment> mapper(client);
            mapper.orderBy(Deployment::Cols::_created_at, SortOrder::DESC);
            co_return co_await mapper.findOne(
                Criteria(Deployment::Cols::_id, CompareOperator::EQ, projectId) &&
                Criteria(Deployment::Cols::_status, CompareOperator::EQ, deploymentStatusToString(DeploymentStatus::SUCCESS)));
        });
        co_return res;
    }

    Task<std::optional<Deployment>> Database::addDeployment(const Deployment deployment) const {
        const auto [res, err] = co_await handleDatabaseOperation<Deployment>([&deployment](const DbClientPtr &client) -> Task<Deployment> {
            CoroMapper<Deployment> mapper(client);
            co_return co_await mapper.insert(deployment);
        });
        co_return res;
    }

    Task<std::optional<Deployment>> Database::updateDeployment(const Deployment deployment) const {
        const auto [res, err] = co_await handleDatabaseOperation<Deployment>([&deployment](const DbClientPtr &client) -> Task<Deployment> {
            CoroMapper<Deployment> mapper(client);
            if (const auto rows = co_await mapper.update(deployment); rows < 1) {
                throw DrogonDbException();
            }
            co_return deployment;
        });
        co_return res;
    }

    Task<Error> Database::deleteDeployment(const std::string id) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&id](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<Deployment> mapper(client);
            const auto rows = co_await mapper.deleteByPrimaryKey(id);
            co_return rows < 1 ? Error::ErrInternal : Error::Ok;
        });
        co_return res.value_or(err);
    }
}
