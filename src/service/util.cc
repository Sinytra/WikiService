#include "util.h"

#include <base64.hpp>

void replace_all(
std::string& s,
std::string const& toReplace,
std::string const& replaceWith
) {
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

std::optional<Json::Value> parseJsonString(const std::string& str) {
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
        char result =
                (std::isblank(previous) && std::isalpha(current)) ? std::toupper(current) : std::tolower(current);
        previous = current;
        return result;
    };
    std::transform(s.begin(), s.end(), s.begin(), f);
    return s;
}

std::string serializeJsonString(const Json::Value& value) {
    Json::FastWriter fastWriter;
    return fastWriter.write(value);
}