#pragma once

#include <json/json.h>
#include <optional>
#include <string>

struct ResourceLocation {
    const std::string namespace_;
    const std::string path_;

    static std::optional<ResourceLocation> parse(const std::string &str);
};

void replace_all(std::string &s, std::string const &toReplace, std::string const &replaceWith);

std::string decodeBase64(std::string encoded);

std::optional<Json::Value> parseJsonString(const std::string &str);

std::string serializeJsonString(const Json::Value &value);

std::string toCamelCase(std::string s);

std::string removeLeadingSlash(const std::string &s);
std::string removeTrailingSlash(const std::string &s);
