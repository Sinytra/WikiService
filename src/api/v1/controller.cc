#include "controller.h"

#include <drogon/HttpClient.h>
#include <models/Mod.h>
#include <trantor/utils/AsyncFileLogger.h>

#include "RedisCache.h"

#include "log/log.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1
{
    HelloWorld::HelloWorld(Service& s) : service_(s)
    {
    }

    Task<> HelloWorld::status(drogon::HttpRequestPtr req,
        std::function<void(const drogon::HttpResponsePtr&)> callback) const
    {
        const std::string& username = "su5ed";

        nosql::RedisClientPtr redisPtr = app().getFastRedisClient();
        const std::string& key = "user:" + username;

        try
        {
            auto cachedId = co_await redisPtr->execCommandCoro("get %s", key.data());

            if (cachedId.type() == nosql::RedisResultType::kString)
            {
                const auto strRed = cachedId.asString();

                Json::Value json;
                json["message"] = "Hello";
                json["id"] = strRed;
                json["cached"] = true;
                const auto resp = HttpResponse::newHttpJsonResponse(std::move(json));
                resp->setStatusCode(drogon::k200OK);
                callback(resp);
                co_return;
            }
        } catch (const nosql::RedisException &err)
        {
            const auto resp = HttpResponse::newHttpResponse();
            resp->setBody(err.what());
            callback(resp);
            co_return;
        }

        try
        {
            auto client = HttpClient::newHttpClient("https://api.modrinth.com");
            auto httpReq = HttpRequest::newHttpRequest();
            httpReq->setMethod(drogon::Get);
            httpReq->setPath("/v3/user/" + username);

            auto response = co_await client->sendRequestCoro(httpReq);
            auto jsonResp = response->getJsonObject();
            if (!jsonResp)
            {
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody("missing 'value' in body");
                resp->setStatusCode(k400BadRequest);
                callback(resp);
                co_return;
            }

            std::string value = (*jsonResp)["id"].asString();
            logger.info("Received response {}", value);

            Json::Value json;
            json["message"] = "Hello";
            json["id"] = value;
            json["cached"] = false;
            const auto resp = HttpResponse::newHttpJsonResponse(std::move(json));
            resp->setStatusCode(drogon::k200OK);

            callback(resp);

            logger.info("Updating cached value for key {}", key);
            co_await redisPtr->execCommandCoro("set %s %s", key.data(), value.data());
        } catch (const drogon::HttpException& err)
        {
            const auto resp = HttpResponse::newHttpResponse();
            resp->setBody(err.what());
            callback(resp);
        }

        co_return;

        // auto dbClientPtr = drogon::app().getDbClient("default");
        // drogon::orm::Mapper<Items> mapper(dbClientPtr);
        // mapper.findOne(
        //     Criteria("registry_name", CompareOperator::EQ, "examplemod:steel_ingot"),
        //     [dbClientPtr, callbackPtr, this](Items r)
        //     {
        //         (*callbackPtr)(HttpResponse::newHttpJsonResponse(r.toJson()));
        //     },
        //     [callbackPtr](const DrogonDbException& e)
        //     {
        //         const drogon::orm::UnexpectedRows* s = dynamic_cast<const drogon::orm::UnexpectedRows*>(&e.base());
        //         if (s)
        //         {
        //             auto resp = HttpResponse::newHttpResponse();
        //             resp->setStatusCode(k404NotFound);
        //             (*callbackPtr)(resp);
        //             return;
        //         }
        //         LOG_ERROR << e.base().what();
        //         Json::Value ret;
        //         ret["error"] = "database error";
        //         auto resp = HttpResponse::newHttpJsonResponse(ret);
        //         resp->setStatusCode(k500InternalServerError);
        //         (*callbackPtr)(resp);
        //     });

        // resp->setStatusCode(code);
        // resp->setContentTypeCode(CT_APPLICATION_JSON);
        // resp->setBody(R"({"message": "hello"})");
        // return callback(resp);
    }
}
