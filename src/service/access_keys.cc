#include "access_keys.h"

#include "crypto.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace service {
    AccessKeys::AccessKeys(const std::string &salt) : salt_(salt) {}

    Task<std::pair<AccessKey, std::string>> AccessKeys::createAccessKey(const std::string name, const std::string user, const int expiryDays) const {
        const auto rawValue = "mcw_" + crypto::generateSecureRandomString(64);
        const auto hashed = crypto::hashSecureString(rawValue, salt_);
        if (hashed.empty()) {
            throw std::runtime_error("Failed to hash access key");
        }

        AccessKey key;
        key.setName(name);
        key.setValue(hashed);
        key.setUserId(user);
        if (expiryDays > 0) {
            const auto expiry = trantor::Date::now().roundDay().after(expiryDays * 24 * 60 * 60);
            key.setExpiresAt(expiry);
        }
        const auto saved = unwrap(co_await global::database->addModel(key));

        co_return {saved, rawValue};
    }

    Task<PaginatedData<AccessKey>> AccessKeys::getAccessKeys(const std::string searchQuery, const int page) const {
        // language=postgresql
        static constexpr auto query = "SELECT * FROM access_key \
                                       WHERE name ILIKE '%' || $1 || '%' \
                                       ORDER BY created_at DESC";

        co_return co_await global::database->handlePaginatedQueryWithArgs<AccessKey>(query, page, searchQuery);
    }

    Task<std::optional<AccessKey>> AccessKeys::getAccessKey(const std::string value) const {
        const auto hashed = crypto::hashSecureString(value, salt_);
        if (hashed.empty()) {
            co_return std::nullopt;
        }
        co_return co_await global::database->findOne<AccessKey>(
            Criteria(AccessKey::Cols::_value, CompareOperator::EQ, hashed)
        );
    }

    Task<Error> AccessKeys::deleteAccessKey(const int64_t id) const {
        co_return co_await global::database->deleteByPrimaryKey<AccessKey>(id);
    }

    bool AccessKeys::isValidKey(const AccessKey &key) const {
        const auto now = trantor::Date::now();
        return !key.getExpiresAt() || key.getValueOfExpiresAt() > now;
    }
}