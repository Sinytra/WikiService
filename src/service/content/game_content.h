#pragma once

#include <drogon/utils/coroutine.h>
#include <service/database.h>
#include <service/error.h>
#include <service/resolved.h>

namespace content {
  class Ingestor {
  public:
    drogon::Task<service::Error> ingestGameContentData(service::ResolvedProject project, std::shared_ptr<spdlog::logger> projectLogPtr) const;
  private:
    drogon::Task<service::Error> ingestContentPaths(drogon::orm::DbClientPtr clientPtr, service::ResolvedProject project, std::shared_ptr<spdlog::logger> projectLog) const;
  };
}

namespace global {
  extern std::shared_ptr<content::Ingestor> ingestor;
}