#include "database.h"

namespace service {
    drogon::Task<PaginatedData<Report>> Database::getReports(const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT * FROM report \
                                       ORDER BY created_at DESC";

        co_return co_await handlePaginatedQuery<Report>(query, page);
    }
}