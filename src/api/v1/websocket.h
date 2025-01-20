#pragma once

#include <drogon/WebSocketController.h>
#include <service/storage.h>
#include <service/users.h>

namespace api::v1 {
    class ProjectWebSocketController final : public drogon::WebSocketController<ProjectWebSocketController, false> {
    public:
        explicit ProjectWebSocketController(Database &, Storage &, RealtimeConnectionStorage &, Users &);

        void handleNewMessage(const drogon::WebSocketConnectionPtr&, std::string &&, const drogon::WebSocketMessageType &) override;
        void handleNewConnection(const drogon::HttpRequestPtr &, const drogon::WebSocketConnectionPtr&) override;
        void handleConnectionClosed(const drogon::WebSocketConnectionPtr&) override;

        WS_PATH_LIST_BEGIN
        WS_ADD_PATH_VIA_REGEX("/ws/api/v1/project/log/(.*)");
        WS_PATH_LIST_END
    private:
        Database &database_;
        Storage &storage_;
        RealtimeConnectionStorage &connections_;
        Users &users_;
    };
}
