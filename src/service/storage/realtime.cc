#include "realtime.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#define WS_SUCCESS "<<success"
#define WS_ERROR "<<error"

using namespace drogon;

namespace realtime {
    class FormattedCallbackSink final : public spdlog::sinks::base_sink<std::mutex> {
    public:
        explicit FormattedCallbackSink(const std::function<void(const std::string &msg)> &callback) : callback_(callback) {}

    protected:
        void sink_it_(const spdlog::details::log_msg &msg) override {
            spdlog::memory_buf_t formatted;
            formatter_->format(msg, formatted);
            callback_(fmt::to_string(formatted));
        }
        void flush_() override {}

    private:
        const std::function<void(const std::string &msg)> callback_;
    };

    std::shared_ptr<spdlog::sinks::sink> ConnectionManager::createBroadcastSink(const std::string &project) {
        const auto sink = std::make_shared<FormattedCallbackSink>([&, project](const std::string &msg) { broadcast(project, msg); });
        sink->set_pattern("[%^%L%$] [%Y-%m-%d %T] [%n] %v");
        return sink;
    }

    void ConnectionManager::connect(const std::string &project, const WebSocketConnectionPtr &connection) {
        std::unique_lock lock(mutex_);

        if (!connection || !connection->connected()) {
            return;
        }

        connections_[project].push_back(connection);
        if (const auto messages = pending_.find(project); messages != pending_.end()) {
            for (const auto &message: messages->second) {
                if (connection->connected()) {
                    connection->send(message);
                }
            }
        }
    }

    void ConnectionManager::disconnect(const WebSocketConnectionPtr &connection) {
        std::unique_lock lock(mutex_);

        for (auto it = connections_.begin(); it != connections_.end();) {
            auto &vec = it->second;

            std::erase(vec, connection);

            if (vec.empty()) {
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void ConnectionManager::broadcast(const std::string &project, const std::string &message) {
        {
            std::shared_lock lock(mutex_);

            if (const auto conns = connections_.find(project); conns != connections_.end()) {
                for (const WebSocketConnectionPtr &conn: conns->second) {
                    conn->send(message);
                }
                return;
            }
        }

        std::unique_lock lock(mutex_);
        pending_[project].push_back(message);
    }

    void ConnectionManager::shutdown(const std::string &project) {
        std::unique_lock lock(mutex_);

        if (const auto it = connections_.find(project); it != connections_.end()) {
            for (const std::vector<WebSocketConnectionPtr> copy = it->second; const WebSocketConnectionPtr &conn: copy) {
                conn->shutdown();
            }
        }
        connections_.erase(project);
        pending_.erase(project);
    }

    void ConnectionManager::complete(const std::string &project, const bool success) {
        broadcast(project, success ? WS_SUCCESS : WS_ERROR);
        shutdown(project);
    }
}
