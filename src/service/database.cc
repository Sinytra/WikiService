#include "database.h"
#include "log/log.h"
#include "util.h"

#include <drogon/drogon.h>
#include <models/User.h>
#include <models/UserProject.h>
#include <ranges>

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    Database::Database() = default;

    Task<std::tuple<std::optional<Project>, Error>> Database::getProjectSource(const std::string id) const {
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

    Task<std::vector<std::string>> Database::getProjectIDs() const {
        std::vector<std::string> ids;
        try {
            const auto clientPtr = app().getFastDbClient();
            for (const auto result = co_await clientPtr->execSqlCoro("SELECT id FROM project"); const auto &row: result) {
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

    std::string buildSearchVectorQuery(std::string query) {
        auto result = query | std::views::split(' ') |
                      std::views::filter([](auto const &str) { return !std::all_of(str.begin(), str.end(), isspace); }) |
                      std::views::transform([](auto rng) { return std::string(rng.data(), rng.size()) + ":*"; });
        std::ostringstream result_stream;
        for (auto it = result.begin(); it != result.end(); ++it) {
            if (it != result.begin())
                result_stream << '|';
            result_stream << *it;
        }
        return result_stream.str();
    }

    Task<std::tuple<ProjectSearchResponse, Error>> Database::findProjects(const std::string query, const std::string types,
                                                                          const std::string sort, int page) const {
        try {
            std::string sortQuery;
            if (sort == "az")
                sortQuery = "ORDER BY \"name\" asc";
            else if (sort == "za")
                sortQuery = "ORDER BY \"name\" desc";
            else if (sort == "popularity") {
                // TODO Sorting by popularity
            } else if (sort == "creation_date" || query.empty())
                sortQuery = "ORDER BY \"created_at\" desc";
            else /*if (sort == "relevance")*/
                sortQuery = "ORDER BY ts_rank(search_vector, to_tsquery('simple', $1)) DESC";

            const auto clientPtr = app().getFastDbClient();
            // clang-format off
            const auto results = co_await clientPtr->execSqlCoro(
                "WITH search_data AS ("
                "    SELECT *"
                "    FROM project"
                "    WHERE ((cast($1 as varchar) IS NULL OR $1 = '' OR search_vector @@ to_tsquery('simple', $1))"
                "    AND ((cast($2 as varchar) IS NULL OR $2 = '' OR type = ANY (STRING_TO_ARRAY($2, ',')))))"
                "    " + sortQuery +
                "),"
                "     total_count AS ("
                "         SELECT COUNT(*) AS total_rows FROM search_data"
                "     )"
                "SELECT"
                "    search_data.*,"
                "    total_count.total_rows,"
                "    CEIL(total_count.total_rows::DECIMAL / 20) AS total_pages "
                "FROM search_data, total_count "
                "LIMIT 20 OFFSET ($3 - 1) * 20;",
                buildSearchVectorQuery(query), types, page < 1 ? 1 : page);
            // clang-format on

            if (results.empty()) {
                co_return {{0, 0, std::vector<Project>()}, Error::Ok};
            }

            const int totalRows = results[0]["total_rows"].as<int>();
            const int totalPages = results[0]["total_pages"].as<int>();
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

    Task<bool> Database::existsForData(const std::string id, const nlohmann::json platforms) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<Project> mapper(clientPtr);

            auto criteria = Criteria(Project::Cols::_id, CompareOperator::EQ, id);
            for (const auto &[key, val]: platforms.items()) {
                criteria = criteria || Criteria(CustomSql(Project::Cols::_platforms + " ILIKE '%\"' || $? || '\":\"' || $? || '\"%'"), key,
                                                val.get<std::string>());
            }

            const auto results = co_await mapper.findBy(criteria);

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

    Task<Error> Database::createUserIfNotExists(std::string username) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<User> mapper(clientPtr);

            co_await mapper.findByPrimaryKey(username);
            co_return Error::Ok;
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return Error::ErrInternal;
        } catch (const DrogonDbException &e) {
            // Not found
        }

        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<User> mapper(clientPtr);

            User user;
            user.setId(username);
            const auto result = co_await mapper.insert(user);
            co_return Error::Ok;
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return Error::ErrInternal;
        }
    }

    Task<Error> Database::linkUserModrinthAccount(std::string username, std::string mrAccountId) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<User> mapper(clientPtr);

            co_await mapper.updateBy({User::Cols::_modrinth_id}, Criteria(User::Cols::_id, CompareOperator::EQ, username), mrAccountId);
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

    Task<std::optional<User>> Database::getUser(const std::string username) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<User> mapper(clientPtr);

            const auto result = co_await mapper.findByPrimaryKey(username);
            co_return result;
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return std::nullopt;
        } catch (const DrogonDbException &e) {
            // Not found
            co_return std::nullopt;
        }
    }

    Task<std::vector<Project>> Database::getUserProjects(std::string username) const {
        std::vector<Project> projects;
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<UserProject> mapper(clientPtr);
            const auto results = co_await clientPtr->execSqlCoro(
                "SELECT * FROM project "
                "JOIN user_project ON project.id = user_project.project_id "
                "WHERE user_id = $1;",
                username
            );
            for (const auto &row: results) {
                projects.emplace_back(row);
            }
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
        } catch (const DrogonDbException &e) {
            // Not found
        }
        co_return projects;
    }

    Task<std::optional<Project>> Database::getUserProject(std::string username, std::string id) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<UserProject> mapper(clientPtr);
            const auto results = co_await clientPtr->execSqlCoro(
                "SELECT * FROM project "
                "JOIN user_project ON project.id = user_project.project_id "
                "WHERE user_id = $1 AND project_id = $2;",
                username, id
            );
            if (results.empty()) {
                co_return std::nullopt;
            }
            if (results.size() > 1) {
                logger.critical("Duplicate user project found, bug? user {} <=> project {}", username, id);
                co_return std::nullopt;
            }
            co_return Project(results.front());
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return std::nullopt;
        } catch (const DrogonDbException &e) {
            // Not found
            co_return std::nullopt;
        }
    }

    Task<Error> Database::assignUserProject(const std::string username, const std::string id, const std::string role) const {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<UserProject> mapper(clientPtr);

            UserProject userProject;
            userProject.setUserId(username);
            userProject.setProjectId(id);
            userProject.setRole(role);

            co_await mapper.insert(userProject);
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
}
