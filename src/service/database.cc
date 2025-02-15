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

namespace service {
    Database::Database() = default;

    Task<std::tuple<std::optional<Project>, Error>> Database::getProjectSource(const std::string id) const {
        co_return co_await handleDatabaseOperation<Project>([id](const DbClientPtr &client) -> Task<Project> {
            CoroMapper<Project> mapper(client);
            co_return co_await mapper.findByPrimaryKey(id);
        });
    }

    Task<std::vector<std::string>> Database::getProjectIDs() const {
        const auto [res, err] =
            co_await handleDatabaseOperation<std::vector<std::string>>([](const DbClientPtr &client) -> Task<std::vector<std::string>> {
                std::vector<std::string> ids;
                for (const auto result = co_await client->execSqlCoro("SELECT id FROM project"); const auto &row: result) {
                    ids.push_back(row["id"].as<std::string>());
                }
                co_return ids;
            });
        co_return res.value_or(std::vector<std::string>());
    }

    Task<std::tuple<std::optional<Project>, Error>> Database::createProject(const Project &project) const {
        co_return co_await handleDatabaseOperation<Project>([project](const DbClientPtr &client) -> Task<Project> {
            CoroMapper<Project> mapper(client);
            co_return co_await mapper.insert(project);
        });
    }

    Task<Error> Database::updateProject(const Project &project) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([project](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<Project> mapper(client);
            co_await mapper.update(project);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> Database::removeProject(const std::string &id) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([id](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<Project> mapper(client);
            co_await mapper.deleteByPrimaryKey(id);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<std::tuple<ProjectSearchResponse, Error>> Database::findProjects(const std::string query, const std::string types,
                                                                          const std::string sort, int page) const {
        const auto [res, err] = co_await handleDatabaseOperation<ProjectSearchResponse>(
            [query, types, sort, page](const DbClientPtr &client) -> Task<ProjectSearchResponse> {
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
        if (res) {
            co_return {*res, err};
        }
        co_return {{}, err};
    }

    Task<bool> Database::existsForRepo(const std::string repo, const std::string branch, const std::string path) const {
        const auto [res, err] = co_await handleDatabaseOperation<bool>([repo, branch, path](const DbClientPtr &client) -> Task<bool> {
            CoroMapper<Project> mapper(client);
            const auto results = co_await mapper.findBy(Criteria(Project::Cols::_source_repo, CompareOperator::EQ, repo) &&
                                                        Criteria(Project::Cols::_source_branch, CompareOperator::EQ, branch) &&
                                                        Criteria(Project::Cols::_source_path, CompareOperator::EQ, path));
            co_return !results.empty();
        });
        co_return res.value_or(false);
    }

    Task<bool> Database::existsForData(const std::string id, const nlohmann::json platforms) const {
        const auto [res, err] = co_await handleDatabaseOperation<bool>([id, platforms](const DbClientPtr &client) -> Task<bool> {
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

    Task<Error> Database::createUserIfNotExists(const std::string username) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([username](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<User> mapper(client);
            co_await mapper.findByPrimaryKey(username);
            co_return Error::Ok;
        });
        if (res) {
            co_return *res;
        }

        const auto [ires, ierr] = co_await handleDatabaseOperation<Error>([username](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<User> mapper(client);
            User user;
            user.setId(username);
            const auto result = co_await mapper.insert(user);
            co_return Error::Ok;
        });
        co_return ires.value_or(ierr);
    }

    Task<Error> Database::deleteUserProjects(std::string username) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([username](const DbClientPtr &client) -> Task<Error> {
            co_await client->execSqlCoro(
                "DELETE FROM project USING user_project WHERE project.id = user_project.project_id AND user_project.user_id = $1;",
                username);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> Database::deleteUser(const std::string username) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([username](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<User> mapper(client);
            co_await mapper.deleteByPrimaryKey(username);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> Database::linkUserModrinthAccount(const std::string username, const std::string mrAccountId) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([username, mrAccountId](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<User> mapper(client);
            co_await mapper.updateBy({User::Cols::_modrinth_id}, Criteria(User::Cols::_id, CompareOperator::EQ, username), mrAccountId);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> Database::unlinkUserModrinthAccount(std::string username) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([username](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<User> mapper(client);
            co_await mapper.updateBy({User::Cols::_modrinth_id}, Criteria(User::Cols::_id, CompareOperator::EQ, username), nullptr);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<std::optional<User>> Database::getUser(const std::string username) const {
        const auto [res, err] = co_await handleDatabaseOperation<User>([username](const DbClientPtr &client) -> Task<User> {
            CoroMapper<User> mapper(client);
            const auto result = co_await mapper.findByPrimaryKey(username);
            co_return result;
        });
        co_return res;
    }

    Task<std::vector<Project>> Database::getUserProjects(std::string username) const {
        const auto [res, err] =
            co_await handleDatabaseOperation<std::vector<Project>>([username](const DbClientPtr &client) -> Task<std::vector<Project>> {
                CoroMapper<User> mapper(client);

                const auto results = co_await client->execSqlCoro(
                    "SELECT * FROM project JOIN user_project ON project.id = user_project.project_id WHERE user_id = $1;", username);

                std::vector<Project> projects;
                for (const auto &row: results) {
                    projects.emplace_back(row);
                }

                co_return projects;
            });
        co_return res.value_or(std::vector<Project>{});
    }

    Task<std::optional<Project>> Database::getUserProject(std::string username, std::string id) const {
        const auto [res, err] = co_await handleDatabaseOperation<Project>([username, id](const DbClientPtr &client) -> Task<Project> {
            CoroMapper<UserProject> mapper(client);
            const auto results = co_await client->execSqlCoro("SELECT * FROM project "
                                                              "JOIN user_project ON project.id = user_project.project_id "
                                                              "WHERE user_id = $1 AND project_id = $2;",
                                                              username, id);
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

    Task<Error> Database::assignUserProject(const std::string username, const std::string id, const std::string role) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([username, id, role](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<UserProject> mapper(client);
            UserProject userProject;
            userProject.setUserId(username);
            userProject.setProjectId(id);
            userProject.setRole(role);

            co_await mapper.insert(userProject);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<std::vector<ProjectContent>> Database::getProjectContents(const std::string project) const {
        const auto [res, err] = co_await handleDatabaseOperation<std::vector<ProjectContent>>(
            [project](const DbClientPtr &client) -> Task<std::vector<ProjectContent>> {
                const auto results =
                    co_await client->execSqlCoro("SELECT item_id, path FROM item "
                                                 "JOIN item_page ON item.id = item_page.id "
                                                 "WHERE item.project_id = $1 AND starts_with(item_page.path, '.content/');",
                                                 project);
                std::vector<ProjectContent> contents;
                for (const auto &row: results) {
                    const auto id = row.at("item_id").as<std::string>();
                    const auto path = row.at("path").as<std::string>();
                    contents.emplace_back(id, path);
                }
                co_return contents;
            });
        co_return res.value_or(std::vector<ProjectContent>{});
    }

    Task<int> Database::getProjectContentCount(std::string project) const {
        const auto [res, err] = co_await handleDatabaseOperation<int>([project](const DbClientPtr &client) -> Task<int> {
            const auto results = co_await client->execSqlCoro("SELECT count(*) FROM item "
                                                              "JOIN item_page ON item.id = item_page.id "
                                                              "WHERE item.project_id = $1 AND starts_with(item_page.path, '.content/');",
                                                              project);
            if (results.size() != 1) {
                throw DrogonDbException{};
            }
            co_return results.front().at("count").as<int>();
        });
        co_return res.value_or(0);
    }

    Task<std::optional<std::string>> Database::getProjectContentPath(std::string project, std::string id) const {
        const auto [res, err] =
            co_await handleDatabaseOperation<std::string>([project, id](const DbClientPtr &client) -> Task<std::string> {
                const auto results = co_await client->execSqlCoro("SELECT path FROM item "
                                                                  "JOIN item_page ON item.id = item_page.id "
                                                                  "WHERE item.project_id = $1 AND item.item_id = $2;",
                                                                  project, id);
                if (results.size() != 1) {
                    throw DrogonDbException{};
                }
                const auto row = results.front();
                const auto path = row.at("path").as<std::string>();
                co_return path;
            });
        co_return res;
    }

    Task<std::optional<Recipe>> Database::getProjectRecipe(std::string project, std::string recipe) const {
        const auto [res, err] = co_await handleDatabaseOperation<Recipe>([project, recipe](const DbClientPtr &client) -> Task<Recipe> {
            CoroMapper<Recipe> mapper(client);
            const auto results = co_await mapper.findBy(Criteria(Recipe::Cols::_project_id, CompareOperator::EQ, project) &&
                                                        Criteria(Recipe::Cols::_loc, CompareOperator::EQ, recipe));
            if (results.size() != 1) {
                throw DrogonDbException{};
            }
            co_return results.front();
        });
        co_return res;
    }

    Task<std::vector<Recipe>> Database::getItemUsageInRecipes(std::string item) const {
        const auto [res, err] =
            co_await handleDatabaseOperation<std::vector<Recipe>>([item](const DbClientPtr &client) -> Task<std::vector<Recipe>> {
                const auto results = co_await client->execSqlCoro("SELECT * FROM recipe "
                                                                  "JOIN recipe_ingredient_item ri ON recipe.id = ri.recipe_id "
                                                                  "WHERE ri.item_id = $1",
                                                                  item);
                std::vector<Recipe> recipes;
                for (const auto &row: results) {
                    recipes.emplace_back(Recipe{row});
                }
                co_return recipes;
            });
        co_return res.value_or(std::vector<Recipe>{});
    }

    Task<std::vector<ContentUsage>> Database::getObtainableItemsBy(std::string item) const {
        const auto [res, err] = co_await handleDatabaseOperation<std::vector<ContentUsage>>(
            [item](const DbClientPtr &client) -> Task<std::vector<ContentUsage>> {
                const auto results =
                    co_await client->execSqlCoro("SELECT r.project_id, ri.item_id "
                                                 "FROM recipe r "
                                                 "JOIN recipe_ingredient_item ri ON r.id = ri.recipe_id "
                                                 "WHERE NOT ri.input "
                                                 "AND EXISTS (SELECT 1 FROM recipe_ingredient_item ri_sub "
                                                 "            WHERE ri_sub.recipe_id = r.id AND ri_sub.item_id = $1 AND ri_sub.input)",
                                                 item);
                std::vector<ContentUsage> usages;
                for (const auto &row: results) {
                    const auto projectId = row[0].as<std::string>();
                    const auto loc = row[1].as<std::string>();
                    usages.emplace_back(loc, projectId);
                }
                co_return usages;
            });
        co_return res.value_or(std::vector<ContentUsage>{});
    }

    Task<std::vector<TagItem>> Database::getTagItemsFlat(std::string tag) const {
        const auto [res, err] =
            co_await handleDatabaseOperation<std::vector<TagItem>>([tag](const DbClientPtr &client) -> Task<std::vector<TagItem>> {
                const auto results = co_await client->execSqlCoro("SELECT child FROM tag_item_flat WHERE parent = $1", tag);
                std::vector<TagItem> tagItems;
                for (const auto &row: results) {
                    const auto itemId = row[0].as<std::string>();
                    tagItems.emplace_back(itemId);
                }
                co_return tagItems;
            });
        co_return res.value_or(std::vector<TagItem>{});
    }
}
