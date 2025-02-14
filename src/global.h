#pragma once

#include <memory>

#include <service/auth.h>
#include <service/cache.h>
#include <service/cloudflare.h>
#include <service/database.h>
#include <service/github.h>
#include <service/platforms.h>
#include <service/storage.h>

#include <service/content/game_content.h>

namespace global {
    extern std::shared_ptr<service::Database> database;
    extern std::shared_ptr<service::MemoryCache> cache;
    extern std::shared_ptr<service::GitHub> github;
    extern std::shared_ptr<service::RealtimeConnectionStorage> connections;
    extern std::shared_ptr<service::Storage> storage;
    extern std::shared_ptr<service::CloudFlare> cloudFlare;
    extern std::shared_ptr<service::Auth> auth;
    extern std::shared_ptr<service::Platforms> platforms;

    extern std::shared_ptr<content::Ingestor> ingestor;
}