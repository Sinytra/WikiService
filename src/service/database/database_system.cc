#include "database.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    Task<PaginatedData<DataImport>> Database::getDataImports(const std::string searchQuery, const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT * FROM data_import \
                                       WHERE game_version ILIKE '%' || $1 || '%' \
                                       ORDER BY created_at DESC";

        co_return co_await handlePaginatedQueryWithArgs<DataImport>(query, page, searchQuery);
    }
}
