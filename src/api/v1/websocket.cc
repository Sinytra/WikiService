#include "websocket.h"
#include "error.h"
#include "auth.h"

#include <database/database.h>
#include <log/log.h>

#define WS_DEPLOYMENT "<<hello<<"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
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
            // Handle unauthenticated user
            const auto session = co_await global::auth->getSession(token);
            if (!session) {
                wsConnPtr->shutdown(CloseCode::kViolation);
            }

            const auto project = co_await global::database->getUserProject(session->username, projectId);
            if (!project) {
                wsConnPtr->shutdown(CloseCode::kViolation);
                co_return;
            }

            // Handle already loaded project
            const auto loadingDeployment = co_await global::database->getLoadingDeployment(projectId);
            if (!loadingDeployment) {
                wsConnPtr->shutdown();
                co_return;
            }

            global::connections->connect(projectId, wsConnPtr);

            // Send initial deployment info
            wsConnPtr->send(WS_DEPLOYMENT + loadingDeployment->getValueOfId());
        }));
    }

    void ProjectWebSocketController::handleConnectionClosed(const WebSocketConnectionPtr &wsConnPtr) {
        global::connections->disconnect(wsConnPtr);
    }
}
