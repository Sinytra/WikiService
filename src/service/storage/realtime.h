#pragma once

#include <unordered_map>
#include <shared_mutex>
#include <drogon/WebSocketConnection.h>
#include <spdlog/sinks/callback_sink.h>

namespace realtime {
    class ConnectionManager {
    public:
        void connect(const std::string &project, const drogon::WebSocketConnectionPtr &connection);

        void disconnect(const drogon::WebSocketConnectionPtr &connection);

        void broadcast(const std::string &project, const std::string &message);

        void shutdown(const std::string &project);

        std::shared_ptr<spdlog::sinks::sink> createBroadcastSink(const std::string &project);

        void complete(const std::string &project, bool success);
    private:
        mutable std::shared_mutex mutex_;
        std::unordered_map<std::string, std::vector<std::string>> pending_;
        std::unordered_map<std::string, std::vector<drogon::WebSocketConnectionPtr>> connections_;
    };
}