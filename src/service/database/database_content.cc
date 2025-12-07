#include "database.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    Task<TaskResult<>> Database::refreshFlatTagItemView() const {
        co_return co_await handleDatabaseOperation([](const DbClientPtr &client) -> Task<> {
            // language=postgresql
            co_await client->execSqlCoro("REFRESH MATERIALIZED VIEW tag_item_flat;");
        });
    }

    // TODO Bind to project
    Task<std::vector<GlobalItem>> Database::getGlobalTagItems(int64_t tagId) const {
        // language=postgresql
        static constexpr auto query = "SELECT ver.id, ver.name, ver.project_id, item.loc \
                                       FROM tag_item_flat \
                                       JOIN project_item pitem ON pitem.id = child \
                                       JOIN item ON item.id = pitem.item_id \
                                       LEFT JOIN project_version ver ON ver.id = pitem.version_id \
                                       WHERE parent IN (SELECT ptag.id FROM project_tag ptag \
                                           JOIN tag t on ptag.tag_id = t.id \
                                           WHERE t.id = $1)";
        const auto res = co_await handleDatabaseOperation([&, tagId](const DbClientPtr &client) -> Task<std::vector<GlobalItem>> {
            const auto results = co_await client->execSqlCoro(query, tagId);
            std::vector<GlobalItem> tagItems;
            for (const auto &row: results) {
                const auto versionId = row[0].isNull() ? 0 : row[0].as<int64_t>();
                const auto versionName = row[1].isNull() ? "" : row[1].as<std::string>();
                const auto projectId = row[2].isNull() ? "" : row[2].as<std::string>();
                const auto itemLocation = row[3].as<std::string>();
                tagItems.emplace_back(versionId, versionName, projectId, itemLocation);
            }
            co_return tagItems;
        });
        co_return res.value_or({});
    }
}
