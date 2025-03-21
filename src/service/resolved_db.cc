#include "resolved_db.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

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

    ProjectDatabaseAccess::ProjectDatabaseAccess(const ResolvedProject &p) : project_(p), projectId_(p.getProject().getValueOfId()) {}

    Task<std::vector<ProjectVersion>> ProjectDatabaseAccess::getVersions() const {
        const auto [res, err] = co_await handleDatabaseOperation<std::vector<ProjectVersion>>(
            [&](const DbClientPtr &client) -> Task<std::vector<ProjectVersion>> {
                CoroMapper<ProjectVersion> mapper(client);
                co_return co_await mapper.findBy(Criteria(ProjectVersion::Cols::_project_id, CompareOperator::EQ, projectId_));
            });
        co_return res.value_or(std::vector<ProjectVersion>{});
    }

    Task<PaginatedData<ProjectVersion>> ProjectDatabaseAccess::getVersionsDev(const std::string searchQuery, const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT * FROM project_version \
                                       WHERE project_id = $1 \
                                       AND (name ILIKE '%' || $2 || '%' OR branch ILIKE '%' || $2 || '%') \
                                       ORDER BY name";

        const auto [res, err] = co_await handleDatabaseOperation<PaginatedData<ProjectVersion>>(
            [&](const DbClientPtr &client) -> Task<PaginatedData<ProjectVersion>> {
                constexpr int size = 20;
                const auto actualQuery = paginatedQuery(query, size, page);

                const auto results = co_await client->execSqlCoro(actualQuery, projectId_, searchQuery);

                if (results.empty()) {
                    throw DrogonDbException{};
                }

                const int totalRows = results[0]["total_rows"].as<int>();
                const int totalPages = results[0]["total_pages"].as<int>();

                std::vector<ProjectVersion> contents;
                for (const auto &row: results) {
                    contents.emplace_back(row);
                }

                co_return PaginatedData{.total = totalRows, .pages = totalPages, .size = size, .data = contents};
            });
        co_return res.value_or(PaginatedData<ProjectVersion>{});
    }

    Task<Error> ProjectDatabaseAccess::addProjectItem(const std::string item) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO project_item (item_id, project_id) \
                                       SELECT id, $2 FROM item WHERE loc = $1 \
                                       ON CONFLICT DO NOTHING";
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, item](const DbClientPtr &client) -> Task<Error> {
            // language=postgresql
            co_await client->execSqlCoro("INSERT INTO item VALUES (DEFAULT, $1) ON CONFLICT DO NOTHING", item);
            co_await client->execSqlCoro(query, item, projectId_);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> ProjectDatabaseAccess::addTag(const std::string tag) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO project_tag (tag_id, project_id) \
                                       SELECT id, $2 FROM tag WHERE loc = $1 \
                                       ON CONFLICT DO NOTHING";

        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, tag](const DbClientPtr &client) -> Task<Error> {
            // language=postgresql
            co_await client->execSqlCoro("INSERT INTO tag VALUES (DEFAULT, $1) ON CONFLICT DO NOTHING", tag);
            co_await client->execSqlCoro(query, tag, projectId_);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<std::vector<Item>> ProjectDatabaseAccess::getTagItemsFlat(const int64_t tag) const {
        // language=postgresql
        static constexpr auto query = "SELECT item.* FROM tag_item_flat \
                                       JOIN item ON item.id = child \
                                       JOIN project_item pitem ON pitem.item_id = item.id \
                                       WHERE parent = $1 AND (pitem.project_id IS NULL OR pitem.project_id = $2)";

        const auto [res, err] =
            co_await handleDatabaseOperation<std::vector<Item>>([&, tag](const DbClientPtr &client) -> Task<std::vector<Item>> {
                const auto results = co_await client->execSqlCoro(query, tag, projectId_);
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
                                              WHERE (pt.project_id IS NULL OR pt.project_id = $1) \
                                                AND (pip.project_id IS NULL OR pip.project_id = $1) \
                                                AND tag.loc = $2 AND item.loc = $3 \
                                              ON CONFLICT DO NOTHING";
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, tag, item](const DbClientPtr &client) -> Task<Error> {
            co_await client->execSqlCoro(tagItemQuery, projectId_, tag, item);
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
                                             WHERE (tp.project_id IS NULL OR tp.project_id = $1) \
                                               AND (tc.project_id IS NULL OR tc.project_id = $1) \
                                               AND p.loc = $2 AND c.loc = $3 \
                                             ON CONFLICT DO NOTHING";
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, parentTag, childTag](const DbClientPtr &client) -> Task<Error> {
            co_await client->execSqlCoro(tagTagQuery, projectId_, parentTag, childTag);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<PaginatedData<ProjectContent>> ProjectDatabaseAccess::getProjectItemsDev(const std::string searchQuery, const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT item.loc, path FROM project_item pitem \
                                       LEFT JOIN project_item_page pip ON pitem.id = pip.item_id \
                                       JOIN item ON item.id = pitem.item_id \
                                       WHERE pitem.project_id = $1 AND (pip IS NULL OR starts_with(pip.path, '.content/')) \
                                       AND (pip.version_id = $2 OR $2 IS NULL) \
                                       AND item.loc ILIKE '%' || $3 || '%' \
                                       ORDER BY item.loc";

        const auto [res, err] = co_await handleDatabaseOperation<PaginatedData<ProjectContent>>(
            [&](const DbClientPtr &client) -> Task<PaginatedData<ProjectContent>> {
                constexpr int size = 20;
                const auto actualQuery = paginatedQuery(query, size, page);

                const auto version = project_.getProjectVersion();
                const auto results = version ? co_await client->execSqlCoro(actualQuery, projectId_, version->getValueOfId(), searchQuery)
                                             : co_await client->execSqlCoro(actualQuery, projectId_, nullptr, searchQuery);

                if (results.empty()) {
                    throw DrogonDbException{};
                }

                const int totalRows = results[0]["total_rows"].as<int>();
                const int totalPages = results[0]["total_pages"].as<int>();

                std::vector<ProjectContent> contents;
                for (const auto &row: results) {
                    const auto id = row[0].as<std::string>();
                    const auto path = row[1].isNull() ? "" : row[1].as<std::string>();
                    contents.emplace_back(id, path);
                }

                co_return PaginatedData{.total = totalRows, .pages = totalPages, .size = size, .data = contents};
            });
        co_return res.value_or(PaginatedData<ProjectContent>{});
    }

    Task<std::vector<ProjectContent>> ProjectDatabaseAccess::getProjectItemPages() const {
        // language=postgresql
        static constexpr auto query = "SELECT item.loc, path FROM project_item pitem \
                                       JOIN project_item_page pip ON pitem.id = pip.item_id \
                                       JOIN item ON item.id = pitem.item_id \
                                       WHERE pitem.project_id = $1 AND starts_with(pip.path, '.content/') \
                                       AND (pip.version_id = $2 OR $2 IS NULL)";

        const auto [res, err] = co_await handleDatabaseOperation<std::vector<ProjectContent>>(
            [&](const DbClientPtr &client) -> Task<std::vector<ProjectContent>> {
                const auto version = project_.getProjectVersion();
                const auto results = version ? co_await client->execSqlCoro(query, projectId_, version->getValueOfId())
                                             : co_await client->execSqlCoro(query, projectId_, nullptr);
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
                                       WHERE pitem.project_id = $1 AND starts_with(pip.path, '.content/') \
                                       AND (pip.version_id = $2 OR $2 IS NULL)";

        const auto [res, err] = co_await handleDatabaseOperation<int>([&](const DbClientPtr &client) -> Task<int> {
            const auto version = project_.getProjectVersion();
            const auto results = version ? co_await client->execSqlCoro(query, projectId_, version->getValueOfId())
                                         : co_await client->execSqlCoro(query, projectId_, nullptr);
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
                                       WHERE pitem.project_id = $1 AND i.loc = $2 \
                                       AND (pip.version_id = $3 OR $3 IS NULL)";

        const auto [res, err] = co_await handleDatabaseOperation<std::string>([&, id](const DbClientPtr &client) -> Task<std::string> {
            const auto version = project_.getProjectVersion();
            const auto results = version ? co_await client->execSqlCoro(query, projectId_, id, version->getValueOfId())
                                         : co_await client->execSqlCoro(query, projectId_, id, nullptr);
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
        static constexpr auto pageQuery = "INSERT INTO project_item_page (version_id, item_id, path) \
                                           SELECT $1 as version_id, pitem.id as item_id, $2 as path \
                                           FROM project_item pitem \
                                           JOIN item i ON pitem.item_id = i.id \
                                           WHERE i.loc = $3 AND pitem.project_id = $4";
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, id, path](const DbClientPtr &client) -> Task<Error> {
            if (const auto version = project_.getProjectVersion()) {
                co_await client->execSqlCoro(pageQuery, version->getValueOfId(), path, id, projectId_);
            } else {
                co_await client->execSqlCoro(pageQuery, nullptr, path, id, projectId_);
            }
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }
}
