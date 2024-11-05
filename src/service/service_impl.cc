#include "service_impl.h"
#include "log/log.h"

#include <drogon/drogon.h>
#include <drogon/orm/Mapper.h>

#include <models/Mod.h>

using namespace std;
using namespace logging;
using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace service
{
    ServiceImpl::ServiceImpl()
    {
    }

    Error ServiceImpl::status() const
    {
        logger.debug("Requesting status");

        return Error::Ok;
    }

    tuple<HelloResponse, Error> ServiceImpl::greet(const HelloRequest& req)
    {
        logger.debug("Requesting greet");
        return {HelloResponse{.response = "Hi there!, you told me: " + req.greeting}, Error::Ok};
    }

    drogon::Task<tuple<optional<ModResponse>, Error>> ServiceImpl::getMod(const ModRequest& req)
    {
        try
        {
            const auto dbClientPtr = drogon::app().getDbClient("default");
            CoroMapper<Mod> mapper(dbClientPtr);
            const auto result = co_await mapper.findByPrimaryKey(req.id);

            co_return {std::optional<ModResponse>{{.id = *result.getId(), .name = *result.getName()}}, Error::Ok};
        } catch (const DrogonDbException &e)
        {
            LOG_ERROR << e.base().what();
            co_return {std::nullopt, Error::ErrInternal};
        }
    }
}
