#pragma once

#define DOCS_META_FILE_PATH "/sinytra-wiki.json"

#include <drogon/HttpClient.h>
#include <drogon/HttpTypes.h>
#include <json/json.h>
#include <log/log.h>
#include <nlohmann/json-schema.hpp>
#include <optional>
#include <string>

using namespace logging;

struct ResourceLocation {
    const std::string namespace_;
    const std::string path_;

    static std::optional<ResourceLocation> parse(const std::string &str);
};

struct JsonValidationError {
    const nlohmann::json &root;
    const std::string msg;
};

void replace_all(std::string &s, std::string const &toReplace, std::string const &replaceWith);

std::string decodeBase64(std::string encoded);

std::optional<Json::Value> parseJsonString(const std::string &str);

std::string serializeJsonString(const Json::Value &value);

std::string toCamelCase(std::string s);

std::string removeLeadingSlash(const std::string &s);
std::string removeTrailingSlash(const std::string &s);

bool isSuccess(const drogon::HttpStatusCode &code);

drogon::HttpClientPtr createHttpClient(const std::string &url);

drogon::Task<std::optional<Json::Value>> sendAuthenticatedRequest(
    drogon::HttpClientPtr client, drogon::HttpMethod method, std::string path, std::string token,
    std::function<void(drogon::HttpRequestPtr &)> callback = [](drogon::HttpRequestPtr &) {});

drogon::Task<std::optional<Json::Value>> sendApiRequest(
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

drogon::HttpResponsePtr jsonResponse(const nlohmann::json &json);