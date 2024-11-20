#include "util.h"
#include <log/log.h>

#include <base64.hpp>

using namespace drogon;
using namespace logging;

std::string removeTrailingSlash(const std::string &s) { return s.ends_with('/') ? s.substr(0, s.size() - 1) : s; }

std::string removeLeadingSlash(const std::string &s) { return s.starts_with('/') ? s.substr(1) : s; }

std::optional<ResourceLocation> ResourceLocation::parse(const std::string &str) {
    const auto delimeter = str.find(':');
    if (delimeter == std::string::npos || delimeter == str.size() - 1) {
        return std::nullopt;
    }
    const auto namespace_ = str.substr(0, delimeter);
    const auto path_ = str.substr(delimeter + 1);
    return ResourceLocation{namespace_, path_};
}

void replace_all(std::string &s, std::string const &toReplace, std::string const &replaceWith) {
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

std::string decodeBase64(std::string encoded) {
    replace_all(encoded, "\n", "");
    return base64::from_base64(encoded);
}

std::optional<Json::Value> parseJsonString(const std::string &str) {
    Json::Value root;
    JSONCPP_STRING err;
    Json::CharReaderBuilder builder;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(str.c_str(), str.c_str() + str.size(), &root, &err)) {
        return std::nullopt;
    }
    return root;
}

std::string toCamelCase(std::string s) {
    char previous = ' ';
    auto f = [&](char current) {
        char result = (std::isblank(previous) && std::isalpha(current)) ? std::toupper(current) : std::tolower(current);
        previous = current;
        return result;
    };
    std::transform(s.begin(), s.end(), s.begin(), f);
    return s;
}

std::string serializeJsonString(const Json::Value &value) {
    Json::FastWriter fastWriter;
    fastWriter.omitEndingLineFeed();
    return fastWriter.write(value);
}

bool isSuccess(const HttpStatusCode &code) { return code == k200OK || code == k201Created || code == k202Accepted; }

HttpClientPtr createHttpClient(const std::string &url) {
    auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
    auto client = HttpClient::newHttpClient(url, currentLoop);
    return client;
}

Task<std::optional<Json::Value>> sendAuthenticatedRequest(HttpClientPtr client, HttpMethod method, std::string path, std::string token,
                                                          std::function<void(HttpRequestPtr &)> callback) {
    co_return co_await sendApiRequest(client, method, path, [&](HttpRequestPtr &req) {
        req->addHeader("Authorization", "Bearer " + token);
        callback(req);
    });
}

Task<std::optional<Json::Value>> sendApiRequest(HttpClientPtr client, HttpMethod method, std::string path,
                                                std::function<void(HttpRequestPtr &)> callback) {
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
                co_return std::make_optional(*jsonResp);
            }
        }

        logger.trace("Unexpected api response: ({}) {}", std::to_string(status), response->getBody());
        co_return std::nullopt;
    } catch (std::exception &e) {
        logger.error(e.what());
        co_return std::nullopt;
    }
}

std::optional<JsonValidationError> validateJson(const nlohmann::json &schema, const Json::Value &json) {
    // Jump from one json library to the other. Parkour!
    const auto ser = serializeJsonString(json);
    const auto nlJson = nlohmann::json::parse(ser);

    class CustomJsonErrorHandler : public nlohmann::json_schema::basic_error_handler {
    public:
        void error(const nlohmann::json_pointer<std::basic_string<char>> &pointer, const nlohmann::json &json1,
                   const std::string &string1) override {
            error_ = std::make_unique<JsonValidationError>(json1, string1);
        }

        const std::unique_ptr<JsonValidationError>& getError() {
            return error_;
        }
    private:
        std::unique_ptr<JsonValidationError> error_;
    };

    nlohmann::json_schema::json_validator validator;
    validator.set_root_schema(schema);
    try {
        CustomJsonErrorHandler err;
        validator.validate(nlJson, err);
        if (const auto &error = err.getError()) {
            return *error;
        }
        return std::nullopt;
    } catch ([[maybe_unused]] const std::exception &e) {
        return JsonValidationError{nlJson, e.what()};
    }
}
