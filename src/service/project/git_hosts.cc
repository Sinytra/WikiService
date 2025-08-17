#include "git_hosts.h"

#include <unordered_map>
#include <include/uri.h>

namespace service {
    std::unordered_map<std::string, GitProvider> GIT_PROVIDERS = {
        {"github.com", GitProvider{.filePath = "blob/{branch}/{base}/{path}", .commitPath = "commit/{hash}"}}};

    std::optional<GitProvider> getGitProvider(const std::string &url) {
        const uri parsed{url};
        const auto domain = parsed.get_host();

        const auto provider = GIT_PROVIDERS.find(domain);
        if (provider == GIT_PROVIDERS.end()) {
            return std::nullopt;
        }

        return provider->second;
    }
}