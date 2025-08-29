#pragma once

#include <drogon/HttpFilter.h>

using namespace drogon;

class PublicFilter final : public HttpFilter<PublicFilter> {
public:
    PublicFilter() = default;

    void doFilter(const HttpRequestPtr &req, FilterCallback &&fcb, FilterChainCallback &&fccb) override;
};
