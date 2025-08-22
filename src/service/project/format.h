#pragma once

#include <filesystem>
#include <string>
#include <optional>
#include <service/util.h>

namespace service {
    class ProjectFormat {
    public:
        explicit ProjectFormat(const std::filesystem::path &, const std::string &);

        std::filesystem::path getRoot() const;

        void setLocale(const std::optional<std::string> &locale);
        std::string getLocale() const;

        std::filesystem::path getLocalesPath() const;
        std::filesystem::path getContentDirectoryPath() const;

        std::filesystem::path getWikiMetadataPath() const;
        std::filesystem::path getLocalizedFilePath(const std::string &path) const;
        std::filesystem::path getFolderMetaFilePath(const std::filesystem::path &dir) const;
        std::filesystem::path getItemPropertiesPath() const;
        std::filesystem::path getWorkbenchesPath() const;
        std::filesystem::path getAssetPath(const ResourceLocation &location) const;
        std::filesystem::path getLanguageFilePath(const std::string &namespace_) const;
    private:
        std::filesystem::path root_;

        std::string locale_;
    };
}
