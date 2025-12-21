#include "database.h"
#include <log/log.h>
#include <service/util.h>

#include <drogon/drogon.h>
#include <models/User.h>
#include <models/UserProject.h>
#include <ranges>

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

std::string buildSearchVectorQuery(std::string query) {
    auto result = query | std::views::split(' ') |
                  std::views::filter([](auto const &str) { return !std::all_of(str.begin(), str.end(), isspace); }) |
                  std::views::transform([](auto rng) {
                      const std::string token(rng.data(), rng.size());

                      std::string escaped_token;
                      escaped_token.reserve(token.size() + 2);
                      for (const char c: token) {
                          if (c == '\'') {
                              escaped_token += "''";
                          } else {
                              escaped_token += c;
                          }
                      }

                      return "'" + escaped_token + "':*";
                  });

    std::ostringstream result_stream;
    for (auto it = result.begin(); it != result.end(); ++it) {
        if (it != result.begin())
            result_stream << '|';
        result_stream << *it;
    }
    return result_stream.str();
}

namespace service {
    std::string paginatedQuery(const std::string &dataQuery, const int pageSize, const int page) {
        return std::format("WITH search_data AS ({}), "
                           "    total_count AS (SELECT COUNT(*) AS total_rows FROM search_data) "
                           "SELECT search_data.*, "
                           "    total_count.total_rows, "
                           "    CEIL(total_count.total_rows::DECIMAL / {}) AS total_pages "
                           "FROM search_data, total_count "
                           "LIMIT 20 OFFSET ({} - 1) * {};",
                           dataQuery, pageSize, page, pageSize);
    }

    DbClientPtr DatabaseBase::getDbClientPtr() const { return app().getFastDbClient(); }

    Database::Database() = default;

    Database::Database(const DbClientPtr &client) : clientPtr_(client) {}

    DbClientPtr Database::getDbClientPtr() const { return clientPtr_ ? clientPtr_ : app().getFastDbClient(); }

    Task<TaskResult<Project>> Database::getProjectSource(const std::string id) const {
        co_return co_await handleDatabaseOperation([id](const DbClientPtr &client) -> Task<Project> {
            CoroMapper<Project> mapper(client);
            co_return co_await mapper.findByPrimaryKey(id);
        });
    }

    Task<std::vector<std::string>> Database::getProjectIDs() const {
        // language=postgresql
        static constexpr auto query = "SELECT id FROM project";

        const auto res = co_await handleDatabaseOperation([](const DbClientPtr &client) -> Task<std::vector<std::string>> {
            std::vector<std::string> ids;
            for (const auto result = co_await client->execSqlCoro(query); const auto &row: result) {
                ids.push_back(row["id"].as<std::string>());
            }
            co_return ids;
        });
        co_return res.value_or({});
    }

    Task<PaginatedData<AdminProjectData>> Database::getAllProjects(std::string searchQuery, const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT id, name, type, modid, created_at FROM project \
                                       WHERE name ILIKE '%' || $1 || '%' \
                                       ORDER BY id";

        co_return co_await handlePaginatedQuery<AdminProjectData>(
            query, page,
            [](const Row &row) {
                AdminProjectData data;
                data.id = row.at(Project::Cols::_id).as<std::string>();
                data.name = row.at(Project::Cols::_name).as<std::string>();
                data.type = row.at(Project::Cols::_type).as<std::string>();
                data.modid = row.at(Project::Cols::_modid).as<std::string>();
                const auto createdAt = row.at(Project::Cols::_created_at).as<std::string>();
                data.createdAt = formatDateTimeISO(createdAt);
                return data;
            },
            searchQuery);
    }

    Task<TaskResult<Project>> Database::createProject(const Project &project) const {
        co_return co_await handleDatabaseOperation([project](const DbClientPtr &client) -> Task<Project> {
            CoroMapper<Project> mapper(client);
            co_return co_await mapper.insert(project);
        });
    }

    Task<TaskResult<>> Database::removeProject(const std::string &id) const {
        co_return co_await handleDatabaseOperation([id](const DbClientPtr &client) -> Task<> {
            CoroMapper<Project> mapper(client);
            co_await mapper.deleteByPrimaryKey(id);
        });
    }

    Task<TaskResult<ProjectVersion>> Database::createProjectVersion(ProjectVersion version) const {
        co_return co_await handleDatabaseOperation([version](const DbClientPtr &client) -> Task<ProjectVersion> {
            CoroMapper<ProjectVersion> mapper(client);
            co_return co_await mapper.insert(version);
        });
    }

    Task<TaskResult<ProjectVersion>> Database::getProjectVersion(const std::string project, const std::string name) const {
        co_return co_await findOne<ProjectVersion>(Criteria(ProjectVersion::Cols::_project_id, CompareOperator::EQ, project) &&
                                                   Criteria(ProjectVersion::Cols::_name, CompareOperator::EQ, name));
    }

    Task<TaskResult<ProjectVersion>> Database::getDefaultProjectVersion(std::string project) const {
        co_return co_await findOne<ProjectVersion>(Criteria(ProjectVersion::Cols::_project_id, CompareOperator::EQ, project) &&
                                                   Criteria(ProjectVersion::Cols::_name, CompareOperator::IsNull));
    }

    Task<TaskResult<>> Database::deleteProjectVersions(std::string project) const {
        co_return co_await handleDatabaseOperation([project](const DbClientPtr &client) -> Task<> {
            CoroMapper<ProjectVersion> mapper(client);
            co_await mapper.deleteBy(Criteria(ProjectVersion::Cols::_project_id, CompareOperator::EQ, project));
        });
    }

    Task<TaskResult<ProjectSearchResponse>> Database::findProjects(const std::string query, const std::string types, const std::string sort,
                                                                   int page) const {
        co_return co_await handleDatabaseOperation([query, types, sort, page](const DbClientPtr &client) -> Task<ProjectSearchResponse> {
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

            // clang-format off
            const auto results = co_await client->execSqlCoro(
                "WITH search_data AS ("
                "    SELECT *"
                "    FROM project"
                "    WHERE ((cast($1 as varchar) IS NULL OR $1 = '' OR search_vector @@ to_tsquery('simple', $1))"
                "    AND ((cast($2 as varchar) IS NULL OR $2 = '' OR type = ANY (STRING_TO_ARRAY($2, ',')))))"
                "    AND EXISTS(SELECT * FROM deployment WHERE project_id = project.id AND active = true)"
                "    AND NOT is_virtual"
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
                co_return {};
            }

            const int totalRows = results[0]["total_rows"].as<int>();
            const int totalPages = results[0]["total_pages"].as<int>();
            std::vector<Project> projects;
            for (const auto &row: results) {
                projects.emplace_back(row);
            }

            co_return ProjectSearchResponse{.pages = totalPages, .total = totalRows, .data = projects};
        });
    }

    Task<TaskResult<UserProject>> Database::getProjectMember(const std::string project, const std::string username) const {
        co_return co_await findOne<UserProject>(Criteria(UserProject::Cols::_project_id, CompareOperator::EQ, project) &&
                                                Criteria(UserProject::Cols::_user_id, CompareOperator::EQ, username));
    }

    Task<std::vector<UserProject>> Database::getProjectMembers(const std::string project) const {
        co_return co_await findBy<UserProject>(Criteria(UserProject::Cols::_project_id, CompareOperator::EQ, project));
    }

    Task<bool> Database::canUserLeaveProject(const std::string project, const std::string username) const {
        // language=postgresql
        static constexpr auto query = "SELECT (role != 'owner' OR \
                                           EXISTS(SELECT * FROM user_project WHERE project_id = $1 AND user_id != $2 AND role = 'owner') \
                                       ) as can_leave \
                                       FROM user_project \
                                       WHERE project_id = $1 AND user_id = $2";
        const auto result = co_await handleDatabaseOperation([project, username](const DbClientPtr &client) -> Task<bool> {
            const auto results = co_await client->execSqlCoro(query, project, username);
            if (results.empty()) {
                co_return false;
            }

            const auto canLeave = results[0]["can_leave"].as<bool>();
            co_return canLeave;
        });
        co_return result.value_or(false);
    }

    Task<bool> Database::existsForRepo(const std::string repo, const std::string branch, const std::string path) const {
        const auto res = co_await handleDatabaseOperation([repo, branch, path](const DbClientPtr &client) -> Task<bool> {
            CoroMapper<Project> mapper(client);
            const auto results = co_await mapper.findBy(Criteria(Project::Cols::_source_repo, CompareOperator::EQ, repo) &&
                                                        Criteria(Project::Cols::_source_branch, CompareOperator::EQ, branch) &&
                                                        Criteria(Project::Cols::_source_path, CompareOperator::EQ, path));
            co_return !results.empty();
        });
        co_return res.value_or(false);
    }

    Task<bool> Database::existsForData(const std::string id, const nlohmann::json platforms) const {
        const auto res = co_await handleDatabaseOperation([id, platforms](const DbClientPtr &client) -> Task<bool> {
            CoroMapper<Project> mapper(client);
            auto criteria = Criteria(Project::Cols::_id, CompareOperator::EQ, id);
            for (const auto &[key, val]: platforms.items()) {
                criteria = criteria || Criteria(CustomSql(Project::Cols::_platforms + " ILIKE '%\"' || $? || '\":\"' || $? || '\"%'"), key,
                                                val.get<std::string>());
            }

            const auto results = co_await mapper.findBy(criteria);
            co_return !results.empty();
        });
        co_return res.value_or(false);
    }

    Task<TaskResult<>> Database::createUserIfNotExists(const std::string username) const {
        const auto res = co_await handleDatabaseOperation([username](const DbClientPtr &client) -> Task<> {
            CoroMapper<User> mapper(client);
            co_await mapper.findByPrimaryKey(username);
        });
        if (res)
            co_return res;

        co_return co_await handleDatabaseOperation([username](const DbClientPtr &client) -> Task<> {
            CoroMapper<User> mapper(client);
            User user;
            user.setId(username);
            const auto result = co_await mapper.insert(user);
        });
    }

    Task<TaskResult<>> Database::deleteUserProjects(std::string username) const {
        // language=postgresql
        static constexpr auto query = "DELETE FROM project USING user_project \
                                       WHERE project.id = user_project.project_id AND user_project.user_id = $1";

        co_return co_await handleDatabaseOperation(
            [username](const DbClientPtr &client) -> Task<> { co_await client->execSqlCoro(query, username); });
    }

    Task<TaskResult<>> Database::deleteUser(const std::string username) const {
        co_return co_await handleDatabaseOperation([username](const DbClientPtr &client) -> Task<> {
            CoroMapper<User> mapper(client);
            co_await mapper.deleteByPrimaryKey(username);
        });
    }

    Task<TaskResult<>> Database::linkUserModrinthAccount(const std::string username, const std::string mrAccountId) const {
        co_return co_await handleDatabaseOperation([username, mrAccountId](const DbClientPtr &client) -> Task<> {
            CoroMapper<User> mapper(client);
            co_await mapper.updateBy({User::Cols::_modrinth_id}, Criteria(User::Cols::_id, CompareOperator::EQ, username), mrAccountId);
        });
    }

    Task<TaskResult<>> Database::unlinkUserModrinthAccount(std::string username) const {
        co_return co_await handleDatabaseOperation([username](const DbClientPtr &client) -> Task<> {
            CoroMapper<User> mapper(client);
            co_await mapper.updateBy({User::Cols::_modrinth_id}, Criteria(User::Cols::_id, CompareOperator::EQ, username), nullptr);
        });
    }

    Task<std::vector<Project>> Database::getUserProjects(std::string username) const {
        // language=postgresql
        static constexpr auto query = "SELECT * FROM project \
                                       JOIN user_project ON project.id = user_project.project_id \
                                       WHERE user_id = $1";

        const auto res = co_await handleDatabaseOperation([username](const DbClientPtr &client) -> Task<std::vector<Project>> {
            const auto results = co_await client->execSqlCoro(query, username);

            std::vector<Project> projects;
            for (const auto &row: results) {
                projects.emplace_back(row);
            }

            co_return projects;
        });
        co_return res.value_or({});
    }

    Task<TaskResult<Project>> Database::getUserProject(std::string username, std::string id) const {
        // language=postgresql
        static constexpr auto query = "SELECT * FROM project \
                                       JOIN user_project ON project.id = user_project.project_id \
                                       WHERE user_id = $1 AND project_id = $2";

        const auto res = co_await handleDatabaseOperation([username, id](const DbClientPtr &client) -> Task<Project> {
            const auto results = co_await client->execSqlCoro(query, username, id);
            if (results.empty()) {
                throw DrogonDbException{};
            }
            if (results.size() > 1) {
                logger.critical("Duplicate user project found, bug? user {} <=> project {}", username, id);
                throw DrogonDbException{};
            }
            co_return Project(results.front());
        });
        co_return res;
    }

    Task<std::vector<std::string>> Database::getItemSourceProjects(int64_t item) const {
        // language=postgresql
        static constexpr auto query = "SELECT pv.project_id FROM project_item pitem \
                                       JOIN project_version pv ON pv.id = pitem.version_id \
                                       WHERE pitem.item_id = $1";

        const auto res = co_await handleDatabaseOperation([item](const DbClientPtr &client) -> Task<std::vector<std::string>> {
            const auto results = co_await client->execSqlCoro(query, item);
            std::vector<std::string> projects;
            for (const auto &row: results) {
                const auto projectId = row[0].as<std::string>();
                projects.push_back(projectId);
            }
            co_return projects;
        });
        co_return res.value_or({});
    }

    Task<TaskResult<DataImport>> Database::getDataImport(std::string gameVersion) const {
        co_return co_await findOne<DataImport>(Criteria(DataImport::Cols::_game_version, CompareOperator::EQ, gameVersion));
    }
}
