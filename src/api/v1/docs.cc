#include "docs.h"

#include <drogon/HttpClient.h>
#include <models/Mod.h>
#include <trantor/utils/AsyncFileLogger.h>

#include "log/log.h"

#include <string>
#include <fstream>

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    DocsController::DocsController(Service &s, GitHub &g) : service_(s), github_(g) {
    }

    Task<> DocsController::page(drogon::HttpRequestPtr req,
                                std::function<void(const drogon::HttpResponsePtr &)> callback, std::string mod) const {
        try {
            // TODO Static asserts to return if tuple is empty
            const string repo = "Su5eD/WikiTest";
            const string path = "docs/examplemod/sinytra-wiki.json";

            const auto [token, error](co_await github_.getApplicationJWTToken());
            const auto [installationId, error2](co_await github_.getRepositoryInstallation(repo, token.value()));
            const auto [installationToken, error3](co_await github_.getInstallationToken(installationId.value(), token.value()));
            const auto [contents, error4](co_await github_.getRepositoryContents(repo, path, installationToken.value()));

            Json::Value root;
            Json::Reader reader;
            if (bool parsingSuccessful = reader.parse(contents.value().body, root); !parsingSuccessful) {
                cout << "Error parsing the string" << endl;
            }

            const auto resp = HttpResponse::newHttpJsonResponse(std::move(root));
            resp->setStatusCode(k200OK);
            callback(resp);

            // auto [modResponse, err](co_await service_.getMod(ModRequest{.id = mod}));
            // if (auto respValue = modResponse; respValue && err == Error::Ok) {
            //     Json::Value json;
            //     json["message"] = "Hello";
            //     json["id"] = modResponse.value().id;
            //     json["name"] = modResponse.value().name;
            //     const auto resp = HttpResponse::newHttpJsonResponse(std::move(json));
            //     resp->setStatusCode(drogon::k200OK);
            //
            //     callback(resp);
            // } else {
            //     Json::Value json;
            //     json["error"] = "Mod does not exist";
            //     const auto resp = HttpResponse::newHttpJsonResponse(std::move(json));
            //     resp->setStatusCode(drogon::k500InternalServerError);
            //
            //     callback(resp);
            // }
        } catch (const drogon::HttpException &err) {
            const auto resp = HttpResponse::newHttpResponse();
            resp->setBody(err.what());
            callback(resp);
        }

        co_return;
    }
}
