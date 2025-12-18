#include "project_database.h"
#include <models/RecipeType.h>
#include <project/virtual/virtual.h>

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    inline int64_t mcVersionId() { return global::virtualProject->getProjectVersion().getValueOfId(); }

    ProjectDatabaseAccess::ProjectDatabaseAccess(const ProjectBase &p) :
        project_(p), projectId_(p.getId()), versionId_(p.getProjectVersion().getValueOfId()) {}

    DbClientPtr ProjectDatabaseAccess::getDbClientPtr() const { return clientPtr_ ? clientPtr_ : DatabaseBase::getDbClientPtr(); }

    void ProjectDatabaseAccess::setDBClientPointer(const DbClientPtr &client) { clientPtr_ = client; }

    Task<std::vector<ProjectVersion>> ProjectDatabaseAccess::getVersions() const {
        const auto res = co_await handleDatabaseOperation([&](const DbClientPtr &client) -> Task<std::vector<ProjectVersion>> {
            CoroMapper<ProjectVersion> mapper(client);
            co_return co_await mapper.findBy(Criteria(ProjectVersion::Cols::_project_id, CompareOperator::EQ, projectId_) &&
                                             Criteria(ProjectVersion::Cols::_name, CompareOperator::IsNotNull));
        });
        co_return res.value_or({});
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

    Task<TaskResult<>> ProjectDatabaseAccess::deleteUnusedVersions(const std::vector<std::string> keep) const {
        co_return co_await handleDatabaseOperation([&](const DbClientPtr &client) -> Task<> {
            CoroMapper<ProjectVersion> mapper(client);

            Criteria criteria = Criteria(ProjectVersion::Cols::_project_id, CompareOperator::EQ, projectId_) &&
                                Criteria(ProjectVersion::Cols::_name, CompareOperator::IsNotNull);
            if (!keep.empty()) {
                criteria = criteria && Criteria(ProjectVersion::Cols::_name, CompareOperator::NotIn, keep);
            }
            co_await mapper.deleteBy(criteria);
        });
    }

    Task<TaskResult<>> ProjectDatabaseAccess::addProjectItem(const std::string item) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO project_item (item_id, version_id) \
                                       SELECT id, $2 FROM item WHERE loc = $1 \
                                       ON CONFLICT DO NOTHING";
        co_return co_await handleDatabaseOperation([&, item](const DbClientPtr &client) -> Task<> {
            // language=postgresql
            co_await client->execSqlCoro("INSERT INTO item VALUES (DEFAULT, $1) ON CONFLICT DO NOTHING", item);
            co_await client->execSqlCoro(query, item, versionId_);
        });
    }

    Task<TaskResult<>> ProjectDatabaseAccess::addTag(const std::string tag) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO project_tag (tag_id, version_id) \
                                       SELECT id, $2 FROM tag WHERE loc = $1 \
                                       ON CONFLICT DO NOTHING";

        co_return co_await handleDatabaseOperation([&, tag](const DbClientPtr &client) -> Task<> {
            // language=postgresql
            co_await client->execSqlCoro("INSERT INTO tag VALUES (DEFAULT, $1) ON CONFLICT DO NOTHING", tag);
            co_await client->execSqlCoro(query, tag, versionId_);
        });
    }

    Task<std::vector<Item>> ProjectDatabaseAccess::getProjectTagItemsFlat(const int64_t tag) const {
        // language=postgresql
        static constexpr auto query = "SELECT item.* FROM tag_item_flat \
                                       JOIN project_item pitem ON pitem.id = child \
                                       JOIN item ON item.id = pitem.item_id \
                                       WHERE parent = $1 \
                                       AND (pitem.version_id = $2 OR pitem.version_id = $3)";

        const auto res = co_await handleDatabaseOperation([&, tag](const DbClientPtr &client) -> Task<std::vector<Item>> {
            // TODO Rework tag&item relations
            const auto virtualVersionId = global::virtualProject->getProjectVersion().getValueOfId();
            const auto results = co_await client->execSqlCoro(query, tag, virtualVersionId, versionId_);
            std::vector<Item> tagItems;
            for (const auto &row: results) {
                tagItems.emplace_back(row);
            }
            co_return tagItems;
        });
        co_return res.value_or({});
    }

    Task<TaskResult<>> ProjectDatabaseAccess::addTagItemEntry(const std::string tag, const std::string item) const {
        // language=postgresql
        static constexpr auto tagItemQuery = "INSERT INTO tag_item (tag_id, item_id) \
                                              SELECT pt.id, pip.id FROM project_tag pt CROSS JOIN project_item pip \
                                              JOIN tag ON tag.id = pt.tag_id \
                                              JOIN item ON item.id = pip.item_id \
                                              WHERE pt.version_id = $1 \
                                                AND (pip.version_id = $1 OR pip.version_id = $2) \
                                                AND tag.loc = $3 AND item.loc = $4 \
                                              ON CONFLICT DO NOTHING";
        co_return co_await handleDatabaseOperation([&, tag, item](const DbClientPtr &client) -> Task<> {
            const auto virtualVersionId = global::virtualProject->getProjectVersion().getValueOfId();
            co_await client->execSqlCoro(tagItemQuery, versionId_, virtualVersionId, tag, item);
        });
    }

    Task<TaskResult<>> ProjectDatabaseAccess::addTagTagEntry(const std::string parentTag, const std::string childTag) const {
        // language=postgresql
        static constexpr auto tagTagQuery = "INSERT INTO tag_tag (parent, child) \
                                             SELECT tp.id, tc.id FROM project_tag tp CROSS JOIN project_tag tc \
                                             JOIN tag p ON p.id = tp.tag_id \
                                             JOIN tag c ON c.id = tc.tag_id \
                                             WHERE tp.version_id = $1 \
                                               AND (tc.version_id = $1 OR tc.version_id = $2) \
                                               AND p.loc = $3 AND c.loc = $4 \
                                             ON CONFLICT DO NOTHING";
        co_return co_await handleDatabaseOperation([&, parentTag, childTag](const DbClientPtr &client) -> Task<> {
            const auto virtualVersionId = global::virtualProject->getProjectVersion().getValueOfId();
            co_await client->execSqlCoro(tagTagQuery, versionId_, virtualVersionId, parentTag, childTag);
        });
    }

    Task<PaginatedData<ProjectContent>> ProjectDatabaseAccess::getProjectItemsDev(const std::string searchQuery, const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT pver.project_id, item.loc, path FROM project_item pitem \
                                       LEFT JOIN project_item_page pip ON pitem.id = pip.item_id \
                                       JOIN item ON item.id = pitem.item_id \
                                       JOIN project_version pver on pitem.version_id = pver.id \
                                       WHERE pitem.version_id = $1 AND (pip IS NULL OR starts_with(pip.path, '.content/')) \
                                       AND item.loc ILIKE '%' || $2 || '%' \
                                       ORDER BY item.loc";

        co_return co_await handlePaginatedQuery<ProjectContent>(query, searchQuery, page, [](const Row &row) {
            const auto projectId = row[0].as<std::string>();
            const auto loc = row[1].as<std::string>();
            const auto path = row[2].isNull() ? "" : row[2].as<std::string>();
            return ProjectContent{projectId, loc, path};
        });
    }

    Task<PaginatedData<ProjectContent>> ProjectDatabaseAccess::getProjectTagItemsDev(const std::string tag, const std::string searchQuery,
                                                                                     const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT pver.project_id, item.loc, path \
                                       FROM project_tag ptag \
                                           JOIN tag t ON ptag.tag_id = t.id \
                                           JOIN tag_item_flat flat ON flat.parent = ptag.id \
                                           JOIN project_item pitem ON pitem.id = flat.child \
                                           JOIN project_version pver on pitem.version_id = pver.id \
                                           JOIN item ON item.id = pitem.item_id \
                                           LEFT JOIN project_item_page pip ON pitem.id = pip.item_id \
                                        WHERE ptag.version_id = $1 \
                                            AND (pitem.version_id = $1 OR pitem.version_id = $3) AND t.loc = '{}' \
                                            AND (pip IS NULL OR starts_with(pip.path, '.content/')) \
                                            AND item.loc ILIKE '%' || $2 || '%' \
                                        ORDER BY item.loc";

        const auto virtualVersionId = global::virtualProject->getProjectVersion().getValueOfId();

        co_return co_await handlePaginatedQuery<ProjectContent>(
            std::format(query, tag), searchQuery, page,
            [](const Row &row) {
                const auto projectId = row[0].as<std::string>();
                const auto loc = row[1].as<std::string>();
                const auto path = row[2].isNull() ? "" : row[2].as<std::string>();
                return ProjectContent{projectId, loc, path};
            },
            virtualVersionId);
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

    Task<int> ProjectDatabaseAccess::getProjectContentCount() const {
        // language=postgresql
        static constexpr auto query = "SELECT count(*) FROM project_item pitem \
                                       JOIN project_item_page pip ON pitem.id = pip.item_id \
                                       WHERE pitem.version_id = $1 AND starts_with(pip.path, '.content/')";

        const auto res = co_await handleDatabaseOperation([&](const DbClientPtr &client) -> Task<int> {
            const auto results = co_await client->execSqlCoro(query, versionId_);
            if (results.size() != 1) {
                throw DrogonDbException{};
            }
            co_return results.front().at("count").as<int>();
        });
        co_return res.value_or(0);
    }

    Task<TaskResult<std::string>> ProjectDatabaseAccess::getProjectContentPath(const std::string id) const {
        // language=postgresql
        static constexpr auto query = "SELECT path FROM project_item pitem \
                                       JOIN project_item_page pip ON pitem.id = pip.item_id \
                                       JOIN item i ON pitem.item_id = i.id \
                                       WHERE pitem.version_id = $1 AND i.loc = $2";

        co_return co_await handleDatabaseOperation([&, id](const DbClientPtr &client) -> Task<std::string> {
            const auto results = co_await client->execSqlCoro(query, versionId_, id);
            if (results.size() != 1) {
                throw DrogonDbException{};
            }
            const auto row = results.front();
            const auto path = row.at("path").as<std::string>();
            co_return path;
        });
    }

    Task<TaskResult<>> ProjectDatabaseAccess::addProjectContentPage(const std::string id, const std::string path) const {
        // language=postgresql
        static constexpr auto pageQuery = "INSERT INTO project_item_page (item_id, path) \
                                           SELECT pitem.id as item_id, $1 as path \
                                           FROM project_item pitem \
                                           JOIN item i ON pitem.item_id = i.id \
                                           WHERE i.loc = $2 AND pitem.version_id = $3";
        co_return co_await handleDatabaseOperation(
            [&, id, path](const DbClientPtr &client) -> Task<> { co_await client->execSqlCoro(pageQuery, path, id, versionId_); });
    }

    Task<size_t> ProjectDatabaseAccess::addRecipeWorkbenches(const std::string recipeType, const std::vector<std::string> items) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO recipe_workbench (type_id, item_id) \
                                       SELECT r.id, pitem.id \
                                       FROM recipe_type r \
                                       JOIN project_item pitem \
                                           ON pitem.item_id IN (SELECT id FROM item i WHERE $1::jsonb ? i.loc) \
                                       WHERE r.loc = $2 \
                                       AND r.version_id = $3";
        const auto res = co_await handleDatabaseOperation([&, recipeType, items](const DbClientPtr &client) -> Task<size_t> {
            const auto results = co_await client->execSqlCoro(query, unparkourJson(items), recipeType, versionId_);
            co_return results.affectedRows();
        });
        co_return res.value_or(0);
    }

    Task<TaskResult<Recipe>> ProjectDatabaseAccess::getProjectRecipe(std::string recipe) const {
        co_return co_await handleDatabaseOperation([&, recipe](const DbClientPtr &client) -> Task<Recipe> {
            CoroMapper<Recipe> mapper(client);
            const auto results = co_await mapper.findBy(Criteria(Recipe::Cols::_version_id, CompareOperator::EQ, versionId_) &&
                                                        Criteria(Recipe::Cols::_loc, CompareOperator::EQ, recipe));
            if (results.size() != 1) {
                throw DrogonDbException{};
            }
            co_return results.front();
        });
    }

    Task<TaskResult<RecipeType>> ProjectDatabaseAccess::getRecipeType(std::string type) const {
        co_return co_await handleDatabaseOperation([&, type](const DbClientPtr &client) -> Task<RecipeType> {
            CoroMapper<RecipeType> mapper(client);
            const auto results = co_await mapper.findBy(Criteria(Recipe::Cols::_loc, CompareOperator::EQ, type) &&
                                                        Criteria(Recipe::Cols::_version_id, CompareOperator::EQ, versionId_));
            if (results.size() != 1) {
                throw DrogonDbException{};
            }
            co_return results.front();
        });
    }

    Task<std::vector<std::string>> ProjectDatabaseAccess::getRecipesForItem(std::string item) const {
        // language=postgresql
        static constexpr auto query = "SELECT r.loc FROM recipe r \
                                       JOIN recipe_ingredient_item ritem ON ritem.recipe_id = r.id \
                                       JOIN item ON item.id = ritem.item_id \
                                       WHERE NOT ritem.input AND r.version_id = $1 AND item.loc = $2";
        const auto res = co_await handleDatabaseOperation([&, item](const DbClientPtr &client) -> Task<std::vector<std::string>> {
            const auto results = co_await client->execSqlCoro(query, versionId_, item);
            std::vector<std::string> ids;
            for (const auto &row: results) {
                ids.emplace_back(row[0].as<std::string>());
            }
            co_return ids;
        });
        co_return res.value_or({});
    }

    Task<std::vector<ContentUsage>> ProjectDatabaseAccess::getObtainableItemsBy(std::string item) const {
        // language=postgresql
        static constexpr auto query = "SELECT ver.project_id, item.id, item.loc, pip.path FROM recipe r \
                                       JOIN recipe_ingredient_item ri ON r.id = ri.recipe_id \
                                       JOIN project_item pitem ON ri.item_id = pitem.item_id \
                                       JOIN project_version ver ON pitem.version_id = ver.id \
                                       JOIN item ON pitem.item_id = item.id \
                                       LEFT JOIN project_item_page pip ON pitem.id = pip.item_id \
                                       WHERE (r.version_id = $1 OR r.version_id = $2) \
                                       AND NOT ri.input \
                                       AND ( \
                                           EXISTS ( \
                                               SELECT 1 FROM recipe_ingredient_item ri_sub \
                                               JOIN item i ON ri_sub.item_id = i.id \
                                               WHERE ri_sub.recipe_id = r.id AND i.loc = $3 AND ri_sub.input \
                                           ) \
                                           OR EXISTS( \
                                               SELECT 1 FROM recipe_ingredient_tag rt_sub \
                                               JOIN project_tag pt ON pt.tag_id = rt_sub.tag_id \
                                               JOIN tag_item_flat flat ON flat.parent = pt.id \
                                               JOIN project_item pit on pit.id = flat.child \
                                               JOIN item i ON pit.item_id = i.id \
                                               WHERE rt_sub.recipe_id = r.id AND i.loc = $3 AND rt_sub.input \
                                           ) \
                                       )";

        const auto res = co_await handleDatabaseOperation([&, item](const DbClientPtr &client) -> Task<std::vector<ContentUsage>> {
            const auto results = co_await client->execSqlCoro(query, versionId_, mcVersionId(), item);
            std::vector<ContentUsage> usages;
            for (const auto &row: results) {
                const auto projectId = row[0].as<std::string>();
                const auto itemId = row[1].as<int64_t>();
                const auto loc = row[2].as<std::string>();
                const auto path = row[3].isNull() ? "" : row[3].as<std::string>();
                usages.emplace_back(itemId, loc, projectId, path);
            }
            co_return usages;
        });
        co_return res.value_or({});
    }

    Task<std::vector<ContentUsage>> ProjectDatabaseAccess::getRecipeTypeWorkbenches(const int64_t id) const {
        // language=postgresql
        static constexpr auto query = "SELECT ver.project_id, item.id, item.loc, pip.path FROM recipe_workbench \
                                       JOIN project_item pitem ON recipe_workbench.item_id = pitem.id \
                                       JOIN item ON pitem.item_id = item.id \
                                       JOIN project_version ver ON pitem.version_id = ver.id \
                                       LEFT JOIN project_item_page pip ON pitem.id = pip.item_id \
                                       WHERE (ver.id = $1 OR ver.id = $2) AND type_id = $3";

        const auto res = co_await handleDatabaseOperation([&, id](const DbClientPtr &client) -> Task<std::vector<ContentUsage>> {
            const auto results = co_await client->execSqlCoro(query, versionId_, mcVersionId(), id);
            std::vector<ContentUsage> usages;
            for (const auto &row: results) {
                const auto projectId = row[0].as<std::string>();
                const auto itemId = row[1].as<int64_t>();
                const auto loc = row[2].as<std::string>();
                const auto path = row[3].isNull() ? "" : row[3].as<std::string>();
                usages.emplace_back(itemId, loc, projectId, path);
            }
            co_return usages;
        });
        co_return res.value_or({});
    }
}
