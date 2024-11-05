#include <drogon/drogon.h>

#include "api/v1/docs.h"
#include "log/log.h"
#include "service/service_impl.h"
#include "service/github.h"

using namespace std;
using namespace drogon;
using namespace logging;

int main() {
    const auto port(8080);
    logger.info("Starting hello-world server on port {}", port);

    auto cache(service::MemoryCache{});
    auto servis(service::ServiceImpl{});
    auto github(service::GitHub{cache});
    auto controller(make_shared<api::v1::DocsController>(servis, github));

    app().loadConfigFile("config.json");

    app()
        .setLogPath("./")
        .setLogLevel(trantor::Logger::kDebug)
        .addListener("0.0.0.0", port)
        .setThreadNum(16);

    app().createRedisClient("127.0.0.1", 6379);
    app().registerController(controller);

    app().run();

    return 0;
}
