#include "database.h"
#include "log/log.h"

#include <drogon/drogon.h>

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
            co_return {std::optional(result), Error::Ok};
        } catch (const Failure &e) {
            // SQL Error
            logger.error("Error querying database: {}", e.what());
            co_return {std::nullopt, Error::ErrInternal};
        } catch (const DrogonDbException &e) {
            // Not found
            co_return {std::nullopt, Error::ErrNotFound};
        }
    }
}
