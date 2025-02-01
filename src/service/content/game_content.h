#pragma once

#include <drogon/utils/coroutine.h>
#include <service/database.h>
#include <service/error.h>
#include <service/resolved.h>

using namespace service;

namespace content {
  class Ingestor {
  public:
    explicit Ingestor(Database &);

    drogon::Task<Error> ingestGameContentData(ResolvedProject project) const;
  private:
    Database &database_;
  };
}