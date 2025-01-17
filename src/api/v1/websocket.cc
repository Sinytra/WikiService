#include "websocket.h"

#include <models/Project.h>

#include <service/util.h>
#include "log/log.h"

#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    ProjectWebSocketController::ProjectWebSocketController(RealtimeConnectionStorage &c, Users &u) : connections_(c), users_(u) {}

    void ProjectWebSocketController::handleNewMessage(const WebSocketConnectionPtr &wsConnPtr, std::string &&message,
                                          const WebSocketMessageType &type) {}

    void ProjectWebSocketController::handleNewConnection(const HttpRequestPtr &req, const WebSocketConnectionPtr &wsConnPtr) {
        static std::string prefix = "/ws/api/v1/project/log/";

        const auto path = req->getPath();
        const auto tokenParam = req->getOptionalParameter<std::string>("token");
        if (!tokenParam) {
            wsConnPtr->shutdown(CloseCode::kViolation);
            return;
        }

        const auto token = *tokenParam;
        const auto projectId = path.substr(prefix.size());

        app().getLoop()->queueInLoop(async_func([&, projectId, token]() -> Task<> {
            if (const auto userInfo = co_await users_.getExistingUserInfo(token)) {
                connections_.connect(projectId, wsConnPtr);
            } else {
                wsConnPtr->shutdown(CloseCode::kViolation);
            }
        }));
    }

    void ProjectWebSocketController::handleConnectionClosed(const WebSocketConnectionPtr &wsConnPtr) {
        connections_.disconnect(wsConnPtr);
    }
}
