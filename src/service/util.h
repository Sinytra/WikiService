#pragma once

#include <drogon/HttpClient.h>
#include <drogon/HttpTypes.h>
#include <log/log.h>
#include <models/Project.h>
#include <nlohmann/json-schema.hpp>
#include <optional>
#include <string>
#include "error.h"

#define EXT_JSON ".json"
#define DOCS_FILE_EXT ".mdx"

template<class>
struct WrapperInnerType;

template<class T>
struct WrapperInnerType<drogon::Task<T>> {
    using type = T;
};

template<class T>
using WrapperInnerType_T = WrapperInnerType<T>::type;

template<typename T = std::monostate>
struct TaskResult {
    TaskResult(const T val) : value_(val), error_(service::Error::Ok) {}
    TaskResult(const std::optional<T> &val) : value_(val), error_(service::Error::Ok) {}
    TaskResult(const service::Error err) : value_(std::nullopt), error_(err) {}
    TaskResult(const std::optional<T> &val, const service::Error err) : value_(val), error_(err) {}
    template<class U>
        requires std::convertible_to<U, T>
    explicit TaskResult(U &&value) : value_(std::forward<U>(value)), error_(service::Error::Ok) {}

    service::Error error() const { return error_; }
    T value_or(T &&value) const { return value_ ? *value_ : value; }
    [[nodiscard]] bool has_value() const noexcept { return value_.has_value(); }

    operator bool() const { return value_.has_value(); }

    T const &operator*() const {
        if (!value_) {
            throw std::runtime_error("No value available");
        }
        return *value_;
    }
    T *operator->() { return value_ ? &*value_ : nullptr; }
    T const *operator->() const { return value_ ? &*value_ : nullptr; }

    TaskResult operator||(TaskResult const &rhs) const { return has_value() ? *this : rhs; }

    friend struct nlohmann::adl_serializer<TaskResult>;

private:
    std::optional<T> value_;
    service::Error error_;
};

template<>
struct TaskResult<std::monostate> {
    TaskResult(const service::Error err) : error_(err) {}

    operator bool() const { return error_ == service::Error::Ok; }

    service::Error error() const { return error_; }

private:
    service::Error error_;
};

template<typename T>
struct nlohmann::adl_serializer<TaskResult<T>> {
    static TaskResult<T> from_json(const json &j) {
        json serializedType = j["value"];
        std::optional<T> value = serializedType.is_null() ? std::nullopt : std::make_optional(serializedType);
        service::Error error = j["error"];
        return TaskResult<T>{value, error};
    }

    static void to_json(json &j, TaskResult<T> t) { j = {{"value", t.value_}, {"error", t.error_}}; }
};

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
    name parse##name(const std::string &str) {                                                                                             \
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

    static bool validate(const std::string &str);
    static std::optional<ResourceLocation> parse(const std::string &str);

    operator std::string() const { return namespace_ + ":" + path_; }
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

drogon::Task<TaskResult<Json::Value>> sendAuthenticatedRequest(
    drogon::HttpClientPtr client, drogon::HttpMethod method, std::string path, std::string token,
    std::function<void(drogon::HttpRequestPtr &)> callback = [](drogon::HttpRequestPtr &) {});

drogon::Task<TaskResult<Json::Value>> sendApiRequest(
    drogon::HttpClientPtr client, drogon::HttpMethod method, std::string path,
    std::function<void(drogon::HttpRequestPtr &)> callback = [](drogon::HttpRequestPtr &) {});

drogon::Task<TaskResult<std::pair<drogon::HttpResponsePtr, Json::Value>>> sendRawApiRequest(
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

template<class CharT, class Traits, class Allocator>
std::basic_istream<CharT, Traits> &getLineSafe(std::basic_istream<CharT, Traits> &is, std::basic_string<CharT, Traits, Allocator> &s) {
    auto &ret = std::getline(is, s, is.widen('\n'));

    // Handle CRLF
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());

    return ret;
}

template<typename T>
T unwrap(TaskResult<T> opt) {
    if (!opt) {
        throw ApiException(service::Error::ErrInternal, "Internal server error");
    }
    return *opt;
}

template<typename T>
T unwrap(std::optional<T> opt) {
    if (!opt) {
        throw ApiException(service::Error::ErrInternal, "Internal server error");
    }
    return opt.value();
}

std::string formatDateTime(const std::string &databaseData);

bool isSubpath(std::filesystem::path path, std::filesystem::path base);