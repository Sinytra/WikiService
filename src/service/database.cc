#include "database.h"
#include "log/log.h"

#include <drogon/drogon.h>
#include <ranges>

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    Database::Database() = default;

    Task<std::tuple<std::optional<Mod>, Error>> Database::getModSource(std::string id) {
        try {
            const auto clientPtr = app().getFastDbClient();
            CoroMapper<Mod> mapper(clientPtr);

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

    Task<std::tuple<BrowseModsResponse, Error>> Database::findMods(std::string query, int page) {
        try {
            // TODO Improve sorting
            const auto clientPtr = app().getFastDbClient();
            const auto results = co_await clientPtr->execSqlCoro(
                "WITH search_data AS ("
                "    SELECT *"
                "    FROM mod"
                "    WHERE (cast($1 as varchar) IS NULL OR $1 = '' OR search_vector @@ to_tsquery('simple', $1))"
                "    ORDER BY \"createdAt\" desc"
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
                co_return {{0, 0, std::vector<Mod>()}, Error::Ok};
            }

            int totalRows = results[0]["total_rows"].as<int>();
            int totalPages = results[0]["total_pages"].as<int>();
            std::vector<Mod> mods;
            for (const auto &row: results) {
                mods.emplace_back(row);
            }

            co_return {BrowseModsResponse{.pages = totalPages, .total = totalRows, .data = mods}, Error::Ok};
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return {{}, Error::ErrInternal};
        } catch (const DrogonDbException &e) {
            // Not found
            co_return {{}, Error::ErrNotFound};
        }
    }
}
