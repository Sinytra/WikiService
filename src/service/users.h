#pragma once

#include "cache.h"
#include "database.h"
#include "github.h"

#include <json/value.h>
#include <set>

namespace service {
  struct UserInfo {
    Json::Value profile;
    Json::Value repositories;
    std::set<std::string> projects;
  };

  class Users : public CacheableServiceBase {
  public:
    explicit Users(Database &, GitHub &, MemoryCache &);

    drogon::Task<std::tuple<std::optional<UserInfo>, Error>> getUserInfo(std::string token) const;

    drogon::Task<std::optional<UserInfo>> getExistingUserInfo(std::string token) const;
  private:
    Database &database_;
    GitHub &github_;
    MemoryCache &cache_;
  };
}