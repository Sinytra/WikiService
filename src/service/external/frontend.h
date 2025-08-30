#pragma once

#include <service/error.h>
#include <drogon/drogon.h>

namespace service {
    class FrontendService {
    public:
        explicit FrontendService(const std::string &frontendUrl, const std::string &apiKey);

        drogon::Task<Error> revalidateProject(std::string id) const;
    private:
        const std::string frontendUrl_;
        const std::string apiKey_;
    };
}

namespace global {
    extern std::shared_ptr<service::FrontendService> frontend;
}