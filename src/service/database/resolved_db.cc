#include "resolved_db.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    ProjectDatabaseAccess::ProjectDatabaseAccess(const ResolvedProject &p) :
        project_(p), projectId_(p.getProject().getValueOfId()), versionId_(p.getProjectVersion().getValueOfId()) {}

    DbClientPtr ProjectDatabaseAccess::getDbClientPtr() const { return clientPtr_ ? clientPtr_ : DatabaseBase::getDbClientPtr(); }

    void ProjectDatabaseAccess::setDBClientPointer(const DbClientPtr &client) { clientPtr_ = client; }

    Task<std::vector<ProjectVersion>> ProjectDatabaseAccess::getVersions() const {
        const auto [res, err] = co_await handleDatabaseOperation<std::vector<ProjectVersion>>(
            [&](const DbClientPtr &client) -> Task<std::vector<ProjectVersion>> {
                CoroMapper<ProjectVersion> mapper(client);
                co_return co_await mapper.findBy(Criteria(ProjectVersion::Cols::_project_id, CompareOperator::EQ, projectId_) &&
                                                 Criteria(ProjectVersion::Cols::_name, CompareOperator::IsNotNull));
            });
        co_return res.value_or(std::vector<ProjectVersion>{});
    }

    Task<PaginatedData<ProjectVersion>> ProjectDatabaseAccess::getVersionsDev(const std::string searchQuery, const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT * FROM project_version \
                                       WHERE project_id = (SELECT project_id FROM project_version WHERE id = $1) \
                                       AND name IS NOT NULL \
                                       AND (name ILIKE '%' || $2 || '%' OR branch ILIKE '%' || $2 || '%') \
                                       ORDER BY name";

        co_return co_await handlePaginatedQuery<ProjectVersion>(query, searchQuery, page);
    }

    Task<Error> ProjectDatabaseAccess::deleteUnusedVersions(const std::vector<std::string> keep) const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&](const DbClientPtr &client) -> Task<Error> {
            CoroMapper<ProjectVersion> mapper(client);

            co_await mapper.deleteBy(Criteria(ProjectVersion::Cols::_project_id, CompareOperator::EQ, projectId_)
                && Criteria(ProjectVersion::Cols::_name, CompareOperator::IsNotNull)
                && Criteria(ProjectVersion::Cols::_name, CompareOperator::NotIn, keep));

            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> ProjectDatabaseAccess::addProjectItem(const std::string item) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO project_item (item_id, version_id) \
                                       SELECT id, $2 FROM item WHERE loc = $1 \
                                       ON CONFLICT DO NOTHING";
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, item](const DbClientPtr &client) -> Task<Error> {
            // language=postgresql
            co_await client->execSqlCoro("INSERT INTO item VALUES (DEFAULT, $1) ON CONFLICT DO NOTHING", item);
            co_await client->execSqlCoro(query, item, versionId_);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> ProjectDatabaseAccess::addTag(const std::string tag) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO project_tag (tag_id, version_id) \
                                       SELECT id, $2 FROM tag WHERE loc = $1 \
                                       ON CONFLICT DO NOTHING";

        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, tag](const DbClientPtr &client) -> Task<Error> {
            // language=postgresql
            co_await client->execSqlCoro("INSERT INTO tag VALUES (DEFAULT, $1) ON CONFLICT DO NOTHING", tag);
            co_await client->execSqlCoro(query, tag, versionId_);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    // TODO How does this differ from getProjectTagItemsFlat? ptag.version_id = $2 makes it do the same
    Task<std::vector<Item>> ProjectDatabaseAccess::getTagItemsFlat(const int64_t tag) const {
        // language=postgresql
        static constexpr auto query = "SELECT item.* FROM tag_item_flat \
                                       JOIN project_item pitem ON pitem.id = child \
                                       JOIN item ON item.id = pitem.item_id \
                                       WHERE parent IN ( \
                                          SELECT ptag.id FROM project_tag ptag \
                                          JOIN tag t on ptag.tag_id = t.id \
                                          WHERE t.id = $1 \
                                          AND (ptag.version_id IS NULL OR ptag.version_id = $2))";

        const auto [res, err] =
            co_await handleDatabaseOperation<std::vector<Item>>([&, tag](const DbClientPtr &client) -> Task<std::vector<Item>> {
                const auto results = co_await client->execSqlCoro(query, tag, versionId_);
                std::vector<Item> tagItems;
                for (const auto &row: results) {
                    tagItems.emplace_back(row);
                }
                co_return tagItems;
            });
        co_return res.value_or(std::vector<Item>{});
    }

    Task<std::vector<Item>> ProjectDatabaseAccess::getProjectTagItemsFlat(const int64_t tag) const {
        // language=postgresql
        static constexpr auto query = "SELECT item.* FROM tag_item_flat \
                                       JOIN project_item pitem ON pitem.id = child \
                                       JOIN item ON item.id = pitem.item_id \
                                       WHERE parent = $1 \
                                       AND (pitem.version_id IS NULL OR pitem.version_id = $2)";

        const auto [res, err] =
            co_await handleDatabaseOperation<std::vector<Item>>([&, tag](const DbClientPtr &client) -> Task<std::vector<Item>> {
                const auto results = co_await client->execSqlCoro(query, tag, versionId_);
                std::vector<Item> tagItems;
                for (const auto &row: results) {
                    tagItems.emplace_back(row);
                }
                co_return tagItems;
            });
        co_return res.value_or(std::vector<Item>{});
    }

    Task<Error> ProjectDatabaseAccess::addTagItemEntry(const std::string tag, const std::string item) const {
        // language=postgresql
        static constexpr auto tagItemQuery = "INSERT INTO tag_item (tag_id, item_id) \
                                              SELECT pt.id, pip.id FROM project_tag pt CROSS JOIN project_item pip \
                                              JOIN tag ON tag.id = pt.tag_id \
                                              JOIN item ON item.id = pip.item_id \
                                              WHERE pt.version_id = $1 \
                                                AND pip.version_id = $1 \
                                                AND tag.loc = $2 AND item.loc = $3 \
                                              ON CONFLICT DO NOTHING";
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, tag, item](const DbClientPtr &client) -> Task<Error> {
            co_await client->execSqlCoro(tagItemQuery, versionId_, tag, item);
            co_return Error::Ok;
        });
        co_return res.value_or(Error::ErrInternal);
    }

    Task<Error> ProjectDatabaseAccess::addTagTagEntry(const std::string parentTag, const std::string childTag) const {
        // language=postgresql
        static constexpr auto tagTagQuery = "INSERT INTO tag_tag (parent, child) \
                                             SELECT tp.id, tc.id FROM project_tag tp CROSS JOIN project_tag tc \
                                             JOIN tag p ON p.id = tp.tag_id \
                                             JOIN tag c ON c.id = tc.tag_id \
                                             WHERE tp.version_id = $1 \
                                               AND tc.version_id = $1 \
                                               AND p.loc = $2 AND c.loc = $3 \
                                             ON CONFLICT DO NOTHING";
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, parentTag, childTag](const DbClientPtr &client) -> Task<Error> {
            co_await client->execSqlCoro(tagTagQuery, versionId_, parentTag, childTag);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<PaginatedData<ProjectContent>> ProjectDatabaseAccess::getProjectItemsDev(const std::string searchQuery, const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT item.loc, path FROM project_item pitem \
                                       LEFT JOIN project_item_page pip ON pitem.id = pip.item_id \
                                       JOIN item ON item.id = pitem.item_id \
                                       WHERE pitem.version_id = $1 AND (pip IS NULL OR starts_with(pip.path, '.content/')) \
                                       AND item.loc ILIKE '%' || $2 || '%' \
                                       ORDER BY item.loc";

        co_return co_await handlePaginatedQuery<ProjectContent>(query, searchQuery, page, [](const Row &row) {
            const auto id = row[0].as<std::string>();
            const auto path = row[1].isNull() ? "" : row[1].as<std::string>();
            return ProjectContent{id, path};
        });
    }

    Task<PaginatedData<ProjectContent>> ProjectDatabaseAccess::getProjectTagItemsDev(const std::string tag, const std::string searchQuery,
                                                                                     const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT item.loc, path "
                                      "FROM project_tag ptag "
                                      "    JOIN tag t ON ptag.tag_id = t.id "
                                      "    JOIN tag_item_flat flat ON flat.parent = ptag.id "
                                      "    JOIN project_item pitem ON pitem.id = flat.child "
                                      "    JOIN item ON item.id = pitem.item_id "
                                      "    LEFT JOIN project_item_page pip ON pitem.id = pip.item_id "
                                      " WHERE ptag.version_id = $1 "
                                      "     AND pitem.version_id = $1 AND t.loc = '{}' "
                                      "     AND (pip IS NULL OR starts_with(pip.path, '.content/')) "
                                      "     AND item.loc ILIKE '%' || $2 || '%' "
                                      " ORDER BY item.loc";

        co_return co_await handlePaginatedQuery<ProjectContent>(std::format(query, tag), searchQuery, page, [](const Row &row) {
            const auto id = row[0].as<std::string>();
            const auto path = row[1].isNull() ? "" : row[1].as<std::string>();
            return ProjectContent{id, path};
        });
    }

    Task<PaginatedData<ProjectTag>> ProjectDatabaseAccess::getProjectTagsDev(const std::string searchQuery, const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT ptag.id, tag.loc FROM project_tag ptag \
                                       JOIN tag ON tag.id = ptag.tag_id \
                                       WHERE ptag.version_id = $1 \
                                       AND tag.loc ILIKE '%' || $2 || '%' \
                                       ORDER BY tag.loc";

        co_return co_await handlePaginatedQuery<ProjectTag>(query, searchQuery, page, [](const Row &row) {
            const auto id = row[0].as<int64_t>();
            const auto loc = row[1].as<std::string>();
            return ProjectTag{id, loc};
        });
    }

    Task<PaginatedData<Recipe>> ProjectDatabaseAccess::getProjectRecipesDev(std::string searchQuery, int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT * FROM recipe \
                                       WHERE version_id = $1 \
                                       AND recipe.loc ILIKE '%' || $2 || '%' \
                                       ORDER BY recipe.loc";

        co_return co_await handlePaginatedQuery<Recipe>(query, searchQuery, page);
    }

    Task<std::vector<ProjectContent>> ProjectDatabaseAccess::getProjectItemPages() const {
        // language=postgresql
        static constexpr auto query = "SELECT item.loc, path FROM project_item pitem \
                                       JOIN project_item_page pip ON pitem.id = pip.item_id \
                                       JOIN item ON item.id = pitem.item_id \
                                       WHERE pitem.version_id = $1 AND starts_with(pip.path, '.content/')";

        const auto [res, err] = co_await handleDatabaseOperation<std::vector<ProjectContent>>(
            [&](const DbClientPtr &client) -> Task<std::vector<ProjectContent>> {
                const auto results = co_await client->execSqlCoro(query, versionId_);
                std::vector<ProjectContent> contents;
                for (const auto &row: results) {
                    const auto id = row[0].as<std::string>();
                    const auto path = row[1].as<std::string>();
                    contents.emplace_back(id, path);
                }
                co_return contents;
            });
        co_return res.value_or(std::vector<ProjectContent>{});
    }

    Task<int> ProjectDatabaseAccess::getProjectContentCount() const {
        // language=postgresql
        static constexpr auto query = "SELECT count(*) FROM project_item pitem \
                                       JOIN project_item_page pip ON pitem.id = pip.item_id \
                                       WHERE pitem.version_id = $1 AND starts_with(pip.path, '.content/')";

        const auto [res, err] = co_await handleDatabaseOperation<int>([&](const DbClientPtr &client) -> Task<int> {
            const auto results = co_await client->execSqlCoro(query, versionId_);
            if (results.size() != 1) {
                throw DrogonDbException{};
            }
            co_return results.front().at("count").as<int>();
        });
        co_return res.value_or(0);
    }

    Task<std::optional<std::string>> ProjectDatabaseAccess::getProjectContentPath(const std::string id) const {
        // language=postgresql
        static constexpr auto query = "SELECT path FROM project_item pitem \
                                       JOIN project_item_page pip ON pitem.id = pip.item_id \
                                       JOIN item i ON pitem.item_id = i.id \
                                       WHERE pitem.version_id = $1 AND i.loc = $2";

        const auto [res, err] = co_await handleDatabaseOperation<std::string>([&, id](const DbClientPtr &client) -> Task<std::string> {
            const auto results = co_await client->execSqlCoro(query, versionId_, id);
            if (results.size() != 1) {
                throw DrogonDbException{};
            }
            const auto row = results.front();
            const auto path = row.at("path").as<std::string>();
            co_return path;
        });
        co_return res;
    }

    Task<Error> ProjectDatabaseAccess::addProjectContentPage(const std::string id, const std::string path) const {
        // language=postgresql
        static constexpr auto pageQuery = "INSERT INTO project_item_page (item_id, path) \
                                           SELECT pitem.id as item_id, $1 as path \
                                           FROM project_item pitem \
                                           JOIN item i ON pitem.item_id = i.id \
                                           WHERE i.loc = $2 AND pitem.version_id = $3";
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, id, path](const DbClientPtr &client) -> Task<Error> {
            co_await client->execSqlCoro(pageQuery, path, id, versionId_);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<size_t> ProjectDatabaseAccess::addRecipeWorkbenches(const std::string recipeType, const std::vector<std::string> items) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO recipe_workbench (type_id, item_id) \
                                       SELECT r.id, pitem.id \
                                       FROM recipe_type r \
                                       JOIN project_item pitem \
                                           ON pitem.item_id IN (SELECT id FROM item i WHERE $1::jsonb ? i.loc) \
                                       WHERE r.loc = $2 \
                                       AND r.version_id = $3 AND (pitem.version_id IS NULL OR pitem.version_id = $3)";
        const auto [res, err] = co_await handleDatabaseOperation<size_t>([&, recipeType, items](const DbClientPtr &client) -> Task<size_t> {
            const auto results = co_await client->execSqlCoro(query, unparkourJson(items), recipeType, versionId_);
            co_return results.affectedRows();
        });
        co_return res.value_or(0);
    }

    Task<std::optional<Recipe>> ProjectDatabaseAccess::getProjectRecipe(std::string recipe) const {
        const auto [res, err] = co_await handleDatabaseOperation<Recipe>([&, recipe](const DbClientPtr &client) -> Task<Recipe> {
            CoroMapper<Recipe> mapper(client);
            const auto results = co_await mapper.findBy(Criteria(Recipe::Cols::_version_id, CompareOperator::EQ, versionId_) &&
                                                        Criteria(Recipe::Cols::_loc, CompareOperator::EQ, recipe));
            if (results.size() != 1) {
                throw DrogonDbException{};
            }
            co_return results.front();
        });
        co_return res;
    }

    Task<std::optional<RecipeType>> ProjectDatabaseAccess::getRecipeType(std::string type) const {
        const auto [res, err] = co_await handleDatabaseOperation<RecipeType>([&, type](const DbClientPtr &client) -> Task<RecipeType> {
            CoroMapper<RecipeType> mapper(client);
            const auto results = co_await mapper.findBy(Criteria(Recipe::Cols::_loc, CompareOperator::EQ, type) &&
                                                        (Criteria(Recipe::Cols::_version_id, CompareOperator::IsNull) ||
                                                         Criteria(Recipe::Cols::_version_id, CompareOperator::EQ, versionId_)));
            if (results.size() != 1) {
                throw DrogonDbException{};
            }
            co_return results.front();
        });
        co_return res;
    }

    Task<std::vector<std::string>> ProjectDatabaseAccess::getItemRecipes(std::string item) const {
        // language=postgresql
        static constexpr auto query = "SELECT recipe.loc FROM recipe \
                                       JOIN project_version ver ON ver.id = version_id \
                                       JOIN recipe_ingredient_item ritem ON ritem.recipe_id = recipe.id \
                                       JOIN item ON item.id = ritem.item_id \
                                       WHERE NOT ritem.input AND version_id = $1 AND item.loc = $2";
        const auto [res, err] = co_await handleDatabaseOperation<std::vector<std::string>>(
            [&, item](const DbClientPtr &client) -> Task<std::vector<std::string>> {
                const auto results = co_await client->execSqlCoro(query, versionId_, item);
                std::vector<std::string> ids;
                for (const auto &row: results) {
                    ids.emplace_back(row[0].as<std::string>());
                }
                co_return ids;
            });
        co_return res.value_or(std::vector<std::string>{});
    }
}
