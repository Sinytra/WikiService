#include "database.h"
#include "log/log.h"

#include <drogon/drogon.h>
#include <ranges>

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    Database::Database() = default;

    Task<std::tuple<std::optional<Project>, Error>> Database::getProjectSource(std::string id) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<Project> mapper(clientPtr);

            const auto result = co_await mapper.findByPrimaryKey(id);
            co_return {result, Error::Ok};
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return {std::nullopt, Error::ErrInternal};
        } catch (const DrogonDbException &e) {
            // Not found
            co_return {std::nullopt, Error::ErrNotFound};
        }
    }

    Task<std::vector<Project>> Database::getProjectsForRepos(std::vector<std::string> repos) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<Project> mapper(clientPtr);

            co_return co_await mapper.findBy(Criteria(Project::Cols::_source_repo, CompareOperator::In, repos));
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
        } catch (const DrogonDbException &e) {
            // Not found
        }
        co_return std::vector<Project>();
    }

    Task<std::vector<std::string>> Database::getProjectIDs() const {
        std::vector<std::string> ids;
        try {
            const auto clientPtr = app().getFastDbClient();
            for (const auto result = co_await clientPtr->execSqlCoro("SELECT id FROM projects"); const auto &row: result) {
                ids.push_back(row["id"].as<std::string>());
            }
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
        } catch (const DrogonDbException &e) {
            // Not found
        }

        co_return ids;
    }

    Task<std::tuple<std::optional<Project>, Error>> Database::createProject(const Project &project) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<Project> mapper(clientPtr);

            const auto result = co_await mapper.insert(project);
            co_return {result, Error::Ok};
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return {std::nullopt, Error::ErrInternal};
        } catch (const DrogonDbException &e) {
            // Not found
            co_return {std::nullopt, Error::ErrNotFound};
        }
    }

    Task<Error> Database::updateProject(const Project &project) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<Project> mapper(clientPtr);

            co_await mapper.update(project);

            co_return Error::Ok;
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return Error::ErrInternal;
        } catch (const DrogonDbException &e) {
            // Not found
            co_return Error::ErrNotFound;
        }
    }

    Task<Error> Database::removeProject(const std::string &id) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<Project> mapper(clientPtr);

            co_await mapper.deleteByPrimaryKey(id);

            co_return Error::Ok;
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return Error::ErrInternal;
        } catch (const DrogonDbException &e) {
            // Not found
            co_return Error::ErrNotFound;
        }
    }

    Task<Error> Database::updateRepository(const std::string &repo, const std::string &newRepo) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<Project> mapper(clientPtr);

            const auto count =
                co_await mapper.updateBy({Project::Cols::_source_repo}, Criteria(Project::Cols::_source_repo, repo), newRepo);
            logger.info("Updated {} rows", count);

            co_return Error::Ok;
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return Error::ErrInternal;
        } catch (const DrogonDbException &e) {
            // Not found
            co_return Error::ErrNotFound;
        }
    }

    std::string buildQuery(std::string query) {
        auto result = query | std::ranges::views::split(' ') |
                      std::views::transform([](auto rng) { return std::string(rng.data(), rng.size()) + ":*"; });
        std::ostringstream result_stream;
        for (auto it = result.begin(); it != result.end(); ++it) {
            if (it != result.begin())
                result_stream << '|';
            result_stream << *it;
        }
        return result_stream.str();
    }

    Task<std::tuple<ProjectSearchResponse, Error>> Database::findProjects(std::string query, int page) const {
        try {
            // TODO Improve sorting
            const auto clientPtr = app().getFastDbClient();
            const auto results = co_await clientPtr->execSqlCoro(
                "WITH search_data AS ("
                "    SELECT *"
                "    FROM project"
                "    WHERE (cast($1 as varchar) IS NULL OR $1 = '' OR search_vector @@ to_tsquery('simple', $1))"
                "    ORDER BY \"created_at\" desc"
                "),"
                "     total_count AS ("
                "         SELECT COUNT(*) AS total_rows FROM search_data"
                "     )"
                "SELECT"
                "    search_data.*,"
                "    total_count.total_rows,"
                "    CEIL(total_count.total_rows::DECIMAL / 20) AS total_pages "
                "FROM search_data, total_count "
                "LIMIT 20 OFFSET ($2 - 1) * 20;",
                buildQuery(query), page < 1 ? 1 : page);

            if (results.empty()) {
                co_return {{0, 0, std::vector<Project>()}, Error::Ok};
            }

            int totalRows = results[0]["total_rows"].as<int>();
            int totalPages = results[0]["total_pages"].as<int>();
            std::vector<Project> projects;
            for (const auto &row: results) {
                projects.emplace_back(row);
            }

            co_return {ProjectSearchResponse{.pages = totalPages, .total = totalRows, .data = projects}, Error::Ok};
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return {{}, Error::ErrInternal};
        } catch (const DrogonDbException &e) {
            // Not found
            co_return {{}, Error::ErrNotFound};
        }
    }

    Task<bool> Database::existsForRepo(const std::string repo, const std::string branch, const std::string path) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<Project> mapper(clientPtr);

            const auto results = co_await mapper.findBy(Criteria(Project::Cols::_source_repo, CompareOperator::EQ, repo) &&
                                                        Criteria(Project::Cols::_source_branch, CompareOperator::EQ, branch) &&
                                                        Criteria(Project::Cols::_source_path, CompareOperator::EQ, path));

            co_return !results.empty();
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return false;
        } catch (const DrogonDbException &e) {
            // Not found
            co_return false;
        }
    }

    Task<bool> Database::existsForData(const std::string id, const std::string platform, const std::string slug) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<Project> mapper(clientPtr);

            const auto results = co_await mapper.findBy(Criteria(Project::Cols::_id, CompareOperator::EQ, id) ||
                                                        (Criteria(Project::Cols::_platform, CompareOperator::EQ, platform) &&
                                                         Criteria(Project::Cols::_slug, CompareOperator::EQ, slug)));

            co_return !results.empty();
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return false;
        } catch (const DrogonDbException &e) {
            // Not found
            co_return false;
        }
    }
}
