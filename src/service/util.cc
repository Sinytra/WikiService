#include "util.h"
#include <log/log.h>
#include <api/v1/error.h>

#include <fstream>
#include <ranges>
#include <regex>

using namespace drogon;
using namespace logging;
using namespace service;
namespace fs = std::filesystem;

TableQueryParams getTableQueryParams(const HttpRequestPtr& req) {
    const auto query = req->getOptionalParameter<std::string>("query").value_or("");
    const auto page = req->getOptionalParameter<int>("page").value_or(1);
    return TableQueryParams{ .query = query, .page = page };
}

HttpResponsePtr jsonResponse(const nlohmann::json &json) {
    const auto resp = HttpResponse::newHttpJsonResponse(unparkourJson(json));
    resp->setStatusCode(k200OK);
    return resp;
}

HttpResponsePtr simpleResponse(const std::string &msg) {
    Json::Value root;
    root["message"] = msg;
    const auto resp = HttpResponse::newHttpJsonResponse(root);
    resp->setStatusCode(k200OK);
    return resp;
}

HttpResponsePtr statusResponse(const HttpStatusCode code) {
    const auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(code);
    return resp;
}

std::string removeLeadingSlash(const std::string &s) { return s.starts_with('/') ? s.substr(1) : s; }

std::string removeTrailingSlash(const std::string &s) { return s.ends_with('/') ? s.substr(0, s.size() - 1) : s; }

void ltrim(std::string &s) {
    s.erase(s.begin(), std::ranges::find_if(s, [](const unsigned char ch) { return !std::isspace(ch); }));
}

void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](const unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

bool ResourceLocation::validate(const std::string &str) {
    static const std::regex resLocRegex("^([a-z0-9_.-]+:)?[a-z0-9_./-]+$", std::regex_constants::ECMAScript);
    return std::regex_match(str.data(), resLocRegex);
}

std::optional<ResourceLocation> ResourceLocation::parse(const std::string &str) {
    if (!validate(str)) {
        return std::nullopt;
    }

    const auto delimeter = str.find(':');
    if (delimeter == str.size() - 1) {
        return std::nullopt;
    }
    if (delimeter == std::string::npos) {
        return ResourceLocation{DEFAULT_NAMESPACE, str};
    }

    const auto namespace_ = str.substr(0, delimeter);
    const auto path_ = str.substr(delimeter + 1);
    return ResourceLocation{namespace_, path_};
}

std::string JsonValidationError::format() const {
    auto field = pointer.to_string();
    if (field.starts_with("/")) field = field.substr(1);
    return field + ": " + msg + "\n> " + value.dump(2);
}

void replaceAll(std::string &s, std::string const &toReplace, std::string const &replaceWith) {
    std::string buf;
    std::size_t pos = 0;
    std::size_t prevPos;

    // Reserves rough estimate of final size of string.
    buf.reserve(s.size());

    while (true) {
        prevPos = pos;
        pos = s.find(toReplace, pos);
        if (pos == std::string::npos)
            break;
        buf.append(s, prevPos, pos - prevPos);
        buf += replaceWith;
        pos += toReplace.size();
    }

    buf.append(s, prevPos, s.size() - prevPos);
    s.swap(buf);
}

Json::Value parseJsonOrThrow(const std::string &str) {
    const auto parsed = parseJsonString(str);
    if (!parsed) {
        throw ApiException(Error::ErrInternal, "Error parsing JSON string");
    }
    return *parsed;
}

std::optional<Json::Value> parseJsonString(const std::string &str) {
    Json::Value root;
    JSONCPP_STRING err;
    const Json::CharReaderBuilder builder;
    if (const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        !reader->parse(str.c_str(), str.c_str() + str.size(), &root, &err))
    {
        return std::nullopt;
    }
    return root;
}

std::optional<nlohmann::json> parseJsonFile(const std::filesystem::path &path) {
    if (!exists(path)) {
        return std::nullopt;
    }
    std::ifstream ifs(path);
    try {
        nlohmann::json jf = nlohmann::json::parse(ifs);
        ifs.close();
        return jf;
    } catch (const nlohmann::json::parse_error &e) {
        ifs.close();
        logger.error("JSON parse error in (parseJsonFile) at {}: {}", path.string(), e.what());
        return std::nullopt;
    }
}

std::string toCamelCase(std::string s) {
    char previous = ' ';
    auto f = [&](const char current) {
        const char result = (std::isblank(previous) && std::isalpha(current)) ? std::toupper(current) : std::tolower(current);
        previous = current;
        return result;
    };
    std::ranges::transform(s, s.begin(), f);
    return s;
}

std::string serializeJsonString(const Json::Value &value) {
    Json::FastWriter fastWriter;
    fastWriter.omitEndingLineFeed();
    return fastWriter.write(value);
}

bool isSuccess(const HttpStatusCode &code) { return code == k200OK || code == k201Created || code == k202Accepted; }

HttpClientPtr createHttpClient(const std::string &url) {
    const auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
    return HttpClient::newHttpClient(url, currentLoop);
}

Task<std::tuple<std::optional<Json::Value>, Error>> sendAuthenticatedRequest(const HttpClientPtr client, const HttpMethod method,
                                                                             const std::string path, const std::string token,
                                                                             const std::function<void(HttpRequestPtr &)> callback) {
    // TODO Replace all "co_return co_await" with "return"
    co_return co_await sendApiRequest(client, method, path, [&](HttpRequestPtr &req) {
        req->addHeader("Authorization", "Bearer " + token);
        callback(req);
    });
}

Task<std::tuple<std::optional<Json::Value>, Error>> sendApiRequest(const HttpClientPtr client, const HttpMethod method,
                                                                   const std::string path,
                                                                   const std::function<void(HttpRequestPtr &)> callback) {
    const auto [resp, err] = co_await sendRawApiRequest(client, method, path, callback);
    if (resp) {
        co_return {resp->second, err};
    }
    co_return {std::nullopt, err};
}

Task<std::tuple<std::optional<std::pair<HttpResponsePtr, Json::Value>>, Error>>
sendRawApiRequest(const HttpClientPtr client, const HttpMethod method, std::string path,
                  const std::function<void(HttpRequestPtr &)> callback) {
    try {
        auto httpReq = HttpRequest::newHttpRequest();
        httpReq->setMethod(method);
        httpReq->setPath(path);
        httpReq->addHeader("Accept", "application/vnd.github+json");
        httpReq->addHeader("X-GitHub-Api-Version", "2022-11-28");
        callback(httpReq);

        logger.trace("=> Request to {}", path);
        const auto response = co_await client->sendRequestCoro(httpReq);
        const auto status = response->getStatusCode();
        if (isSuccess(status)) {
            if (const auto jsonResp = response->getJsonObject()) {
                logger.trace("<= Response ({}) from {}", std::to_string(status), path);
                co_return {std::pair{response, *jsonResp}, Error::Ok};
            }
        }

        logger.trace("Unexpected api response: ({}) {}", std::to_string(status), response->getBody());
        co_return {std::nullopt, api::v1::mapStatusCode(status)};
    } catch (std::exception &e) {
        logger.error("Error sending HTTP request: {}", e.what());
        co_return {std::nullopt, Error::ErrInternal};
    }
}

nlohmann::json parkourJson(const Json::Value &json) {
    // Jump from one json library to the other. Parkour!
    const auto ser = serializeJsonString(json);
    return nlohmann::json::parse(ser);
}

Json::Value unparkourJson(const nlohmann::json &json) {
    const auto parsed = parseJsonString(json.dump());
    if (!parsed) {
        throw std::runtime_error("Failed to convert JSON: " + json.dump());
    }
    return *parsed;
}

std::optional<JsonValidationError> validateJson(const nlohmann::json &schema, const Json::Value &json) {
    const auto newJson = parkourJson(json);
    return validateJson(schema, newJson);
}

std::optional<JsonValidationError> validateJson(const nlohmann::json &schema, const nlohmann::json &json) {
    class CustomJsonErrorHandler : public nlohmann::json_schema::basic_error_handler {
    public:
        void error(const nlohmann::json_pointer<std::basic_string<char>> &pointer, const nlohmann::json &json1,
                   const std::string &string1) override {
            error_ = std::make_unique<JsonValidationError>(json1, pointer, string1);
        }

        const std::unique_ptr<JsonValidationError> &getError() { return error_; }

    private:
        std::unique_ptr<JsonValidationError> error_;
    };

    nlohmann::json_schema::json_validator validator;
    validator.set_root_schema(schema);
    try {
        CustomJsonErrorHandler err;
        validator.validate(json, err);
        if (const auto &error = err.getError()) {
            return *error;
        }
        return std::nullopt;
    } catch ([[maybe_unused]] const std::exception &e) {
        return JsonValidationError{nlohmann::json(), nlohmann::json::json_pointer{"/"}, e.what()};
    }
}

Json::Value projectToJson(const drogon_model::postgres::Project &project, const bool verbose) {
    Json::Value json = project.toJson();
    json["platforms"] = parseJsonString(project.getValueOfPlatforms()).value_or(Json::Value());
    json.removeMember("search_vector");
    if (!verbose) {
        if (!project.getValueOfIsPublic()) {
            json.removeMember("source_repo");
        }
        json.removeMember("source_path");
        json.removeMember("source_branch");
    }
    return json;
}

std::string strToLower(std::string copy) {
    std::ranges::transform(copy, copy.begin(), [](const unsigned char c) { return std::tolower(c); });
    return copy;
}

bool isSubpath(const fs::path &path, const fs::path &base) {
    const auto [fst, snd] = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
    return snd == base.end();
}

std::string formatDateTime(const std::string &databaseData) {
    return trantor::Date::fromDbString(databaseData).toCustomFormattedString("%Y-%m-%dT%H:%M:%SZ");
}