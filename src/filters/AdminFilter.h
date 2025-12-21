#pragma once

#include <drogon/HttpFilter.h>

using namespace drogon;

class AdminFilter final : public HttpCoroFilter<AdminFilter> {
public:
    AdminFilter() = default;

    Task<HttpResponsePtr> doFilter(const HttpRequestPtr &req) override;
};
