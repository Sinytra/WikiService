#pragma once

#include "error.h"

#include <drogon/HttpClient.h>
#include <drogon/HttpTypes.h>
#include <log/log.h>
#include <models/Project.h>
#include <nlohmann/json-schema.hpp>
#include <optional>
#include <string>

#define EXT_JSON ".json"
#define DOCS_FILE_EXT ".mdx"

template<typename K, typename V>
static std::unordered_map<V, K> reverse_map(const std::unordered_map<K, V> &m) {
    std::unordered_map<V, K> r;
    for (const auto &kv: m)
        r[kv.second] = kv.first;
    return r;
}

#define ENUM_TO_STR(name, ...)                                                                                                             \
    std::string enumToStr(const name value) {                                                                                              \
        const static std::unordered_map<name, std::string> map{__VA_ARGS__};                                                               \
        const auto val = map.find(value);                                                                                                  \
        return val == map.end() ? "unknown" : val->second;                                                                                 \
    }

#define ENUM_FROM_TO_STR(name, ...)                                                                                                        \
    const static std::unordered_map<name, std::string> _##name##_map{__VA_ARGS__};                                                         \
    const static std::unordered_map _##name##_map_rev{reverse_map(_##name##_map)};                                                         \
    std::string enumToStr(const name value) {                                                                                              \
        const auto str = _##name##_map.find(value);                                                                                        \
        return str == _##name##_map.end() ? "unknown" : str->second;                                                                       \
    }                                                                                                                                      \
    name parse##name##(const std::string &str) {                                                                                           \
        const auto value = _##name##_map_rev.find(str);                                                                                    \
        return value == _##name##_map_rev.end() ? name::UNKNOWN : value->second;                                                           \
    }

template<typename T>
concept JsonSerializable = requires(nlohmann::json &j, const T &obj) {
    {
        to_json(j, obj)
    };
};

struct TableQueryParams {
    std::string query;
    int page;
};

TableQueryParams getTableQueryParams(const drogon::HttpRequestPtr &req);

drogon::HttpResponsePtr jsonResponse(const nlohmann::json &json);

drogon::HttpResponsePtr simpleResponse(const std::string &msg);

drogon::HttpResponsePtr statusResponse(drogon::HttpStatusCode code);

struct ApiException final : std::runtime_error {
    using std::runtime_error::runtime_error;

    service::Error error;
    Json::Value data;

    ApiException(const service::Error err, const std::string &message, const std::function<void(Json::Value &)> &jsonBuilder = nullptr) :
        std::runtime_error(message), error(err) {
        data["error"] = message;
        if (jsonBuilder) {
            jsonBuilder(data);
        }
    }
};

struct ResourceLocation {
    static constexpr std::string DEFAULT_NAMESPACE = "minecraft";
    static constexpr std::string COMMON_NAMESPACE = "c";

    const std::string namespace_;
    const std::string path_;

    static std::optional<ResourceLocation> parse(const std::string &str);
};

struct JsonValidationError {
    const nlohmann::json value;
    const nlohmann::json_pointer<std::basic_string<char>> pointer;
    const std::string msg;

    std::string format() const;
};

void replaceAll(std::string &s, std::string const &toReplace, std::string const &replaceWith);

std::optional<Json::Value> parseJsonString(const std::string &str);
Json::Value parseJsonOrThrow(const std::string &str);
std::optional<nlohmann::json> parseJsonFile(const std::filesystem::path &path);

std::string serializeJsonString(const Json::Value &value);

std::string toCamelCase(std::string s);

std::string removeLeadingSlash(const std::string &s);

std::string removeTrailingSlash(const std::string &s);

void ltrim(std::string &s);

void rtrim(std::string &s);

bool isSuccess(const drogon::HttpStatusCode &code);

drogon::HttpClientPtr createHttpClient(const std::string &url);

drogon::Task<std::tuple<std::optional<Json::Value>, service::Error>> sendAuthenticatedRequest(
    drogon::HttpClientPtr client, drogon::HttpMethod method, std::string path, std::string token,
    std::function<void(drogon::HttpRequestPtr &)> callback = [](drogon::HttpRequestPtr &) {});

drogon::Task<std::tuple<std::optional<Json::Value>, service::Error>> sendApiRequest(
    drogon::HttpClientPtr client, drogon::HttpMethod method, std::string path,
    std::function<void(drogon::HttpRequestPtr &)> callback = [](drogon::HttpRequestPtr &) {});

drogon::Task<std::tuple<std::optional<std::pair<drogon::HttpResponsePtr, Json::Value>>, service::Error>> sendRawApiRequest(
    drogon::HttpClientPtr client, drogon::HttpMethod method, std::string path,
    std::function<void(drogon::HttpRequestPtr &)> callback = [](drogon::HttpRequestPtr &) {});

template<class T = nlohmann::json>
std::optional<T> tryParseJson(const std::string_view json) {
    try {
        return T::parse(json);
    } catch (const nlohmann::json::parse_error &e) {
        logging::logger.error("JSON parse error: {}", e.what());
        return std::nullopt;
    }
}

nlohmann::json parkourJson(const Json::Value &json);
Json::Value unparkourJson(const nlohmann::json &json);

std::optional<JsonValidationError> validateJson(const nlohmann::json &schema, const Json::Value &json);

std::optional<JsonValidationError> validateJson(const nlohmann::json &schema, const nlohmann::json &json);

Json::Value projectToJson(const drogon_model::postgres::Project &project, bool verbose = false);

std::string strToLower(std::string copy);

bool isSubpath(const std::filesystem::path &path, const std::filesystem::path &base);