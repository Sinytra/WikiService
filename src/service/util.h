#pragma once

#include <string>
#include <optional>
#include <json/json.h>

struct ResourceLocation {
    const std::string namespace_;
    const std::string path_;

    static std::optional<ResourceLocation> parse(const std::string& str);
};

void replace_all(std::string& s,std::string const& toReplace,std::string const& replaceWith);

std::string decodeBase64(std::string encoded);

std::optional<Json::Value> parseJsonString(const std::string& str);

std::string serializeJsonString(const Json::Value& value);

std::string toCamelCase(std::string s);