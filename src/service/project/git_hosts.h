#pragma once

#include <string>
#include <optional>

namespace service {
    struct GitProvider {
        std::string filePath;
        std::string commitPath;
    };

    std::optional<GitProvider> getGitProvider(const std::string &url);
}