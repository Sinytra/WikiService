#pragma once

#include "error.h"

#include <drogon/HttpClient.h>
#include <drogon/HttpTypes.h>
#include <json/json.h>
#include <log/log.h>
#include <models/Project.h>
#include <nlohmann/json-schema.hpp>
#include <optional>
#include <string>

using namespace logging;
using namespace service;

struct ResourceLocation {
    static constexpr std::string DEFAULT_NAMESPACE = "minecraft";

    const std::string namespace_;
    const std::string path_;

    static std::optional<ResourceLocation> parse(const std::string &str);
};

struct JsonValidationError {
    const nlohmann::json_pointer<std::basic_string<char>> pointer;
    const std::string msg;
};

void replaceAll(std::string &s, std::string const &toReplace, std::string const &replaceWith);

std::optional<Json::Value> parseJsonString(const std::string &str);
std::optional<nlohmann::json> parseJsonFile(const std::filesystem::path &path);

std::string serializeJsonString(const Json::Value &value);

std::string toCamelCase(std::string s);

std::string removeLeadingSlash(const std::string &s);

std::string removeTrailingSlash(const std::string &s);

bool isSuccess(const drogon::HttpStatusCode &code);

drogon::HttpClientPtr createHttpClient(const std::string &url);

drogon::Task<std::tuple<std::optional<Json::Value>, Error>> sendAuthenticatedRequest(
    drogon::HttpClientPtr client, drogon::HttpMethod method, std::string path, std::string token,
    std::function<void(drogon::HttpRequestPtr &)> callback = [](drogon::HttpRequestPtr &) {});

drogon::Task<std::tuple<std::optional<Json::Value>, Error>> sendApiRequest(
    drogon::HttpClientPtr client, drogon::HttpMethod method, std::string path,
    std::function<void(drogon::HttpRequestPtr &)> callback = [](drogon::HttpRequestPtr &) {});

drogon::Task<std::tuple<std::optional<std::pair<drogon::HttpResponsePtr, Json::Value>>, Error>> sendRawApiRequest(
    drogon::HttpClientPtr client, drogon::HttpMethod method, std::string path,
    std::function<void(drogon::HttpRequestPtr &)> callback = [](drogon::HttpRequestPtr &) {});

template <class T = nlohmann::json>
std::optional<T> tryParseJson(const std::string_view json) {
    try {
        return T::parse(json);
    } catch (const nlohmann::json::parse_error &e) {
        logger.error("JSON parse error: {}", e.what());
        return std::nullopt;
    }
}

nlohmann::json parkourJson(const Json::Value &json);

std::optional<JsonValidationError> validateJson(const nlohmann::json &schema, const Json::Value &json);

std::optional<JsonValidationError> validateJson(const nlohmann::json &schema, const nlohmann::json &json);

Json::Value projectToJson(const drogon_model::postgres::Project &project);

std::string strToLower(std::string copy);