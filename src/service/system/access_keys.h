#pragma once

#include <memory>
#include <models/AccessKey.h>
#include <service/database/database.h>

namespace service {
    // TODO AccessKey json serializer
    class AccessKeys {
    public:
        explicit AccessKeys(const std::string &);

        drogon::Task<std::pair<AccessKey, std::string>> createAccessKey(std::string name, std::string user, int expiryDays) const;
        drogon::Task<PaginatedData<AccessKey>> getAccessKeys(std::string searchQuery, int page) const;
        drogon::Task<TaskResult<AccessKey>> getAccessKey(std::string value) const;
        drogon::Task<TaskResult<>> deleteAccessKey(int64_t id) const;
        bool isValidKey(const AccessKey &key) const;
    private:
        std::string salt_;
    };
}

namespace global {
    extern std::shared_ptr<service::AccessKeys> accessKeys;
}