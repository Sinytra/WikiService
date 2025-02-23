#pragma once

#include <drogon/utils/coroutine.h>
#include <service/error.h>
#include <service/resolved.h>

struct ModRecipe;

namespace content {
  class Ingestor {
  public:
    explicit Ingestor(service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &);

    drogon::Task<service::Error> ingestGameContentData() const;
  private:
    drogon::Task<int> ingestTags() const;
    drogon::Task<service::Error> ingestRecipes() const;
    drogon::Task<service::Error> ingestContentPaths() const;

    drogon::Task<service::Error> addRecipe(ModRecipe recipe) const;

    const service::ResolvedProject &project_;
    const std::shared_ptr<spdlog::logger> &logger_;
  };
}
