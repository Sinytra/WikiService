#pragma once

#include <drogon/utils/coroutine.h>
#include <service/error.h>
#include <service/resolved.h>

namespace content {
  class Ingestor {
  public:
    explicit Ingestor(service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &);

    drogon::Task<service::Error> ingestGameContentData() const;
  private:
    drogon::Task<service::Error> ingestTags() const;
    drogon::Task<service::Error> ingestRecipes() const;
    drogon::Task<service::Error> ingestContentPaths() const;

    drogon::Task<> addProjectItem(drogon::orm::DbClientPtr clientPtr, std::string project, std::string item) const;
    drogon::Task<> addTag(drogon::orm::DbClientPtr clientPtr, std::string tag, std::optional<std::string> project) const;

    const service::ResolvedProject &project_;
    const std::shared_ptr<spdlog::logger> &logger_;
  };
}
