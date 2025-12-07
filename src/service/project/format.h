#pragma once

#include <filesystem>
#include <string>
#include <optional>
#include <service/util.h>

namespace service {
    class ProjectFormat {
    public:
        virtual std::filesystem::path getRoot() const = 0;

        virtual std::filesystem::path getLocalesPath() const = 0;
        virtual std::filesystem::path getContentDirectoryPath() const = 0;
        virtual std::filesystem::path getAssetsRoot() const = 0;
        virtual std::filesystem::path getDataRoot() const = 0;

        virtual std::filesystem::path getWikiMetadataPath() const = 0;
        virtual std::filesystem::path getLocalizedFilePath(const std::string &path) const = 0;
        virtual std::filesystem::path getFolderMetaFilePath(const std::filesystem::path &dir) const = 0;
        virtual std::filesystem::path getItemPropertiesPath() const = 0;
        virtual std::filesystem::path getWorkbenchesPath() const = 0;
        virtual std::filesystem::path getAssetsPath(const ResourceLocation &location) const = 0;
        virtual std::filesystem::path getLanguageFilePath(const std::string &namespace_) const = 0;
    };

    class V0ProjectFormat : public ProjectFormat {
    public:
        explicit V0ProjectFormat(const std::filesystem::path &, const std::string &);

        std::filesystem::path getRoot() const override;

        void setLocale(const std::optional<std::string> &locale);
        std::string getLocale() const;

        std::filesystem::path getLocalesPath() const override;
        std::filesystem::path getContentDirectoryPath() const override;
        std::filesystem::path getAssetsRoot() const override;
        std::filesystem::path getDataRoot() const override;

        std::filesystem::path getWikiMetadataPath() const override;
        std::filesystem::path getLocalizedFilePath(const std::string &path) const override;
        std::filesystem::path getFolderMetaFilePath(const std::filesystem::path &dir) const override;
        std::filesystem::path getItemPropertiesPath() const override;
        std::filesystem::path getWorkbenchesPath() const override;
        std::filesystem::path getAssetsPath(const ResourceLocation &location) const override;
        std::filesystem::path getLanguageFilePath(const std::string &namespace_) const override;
    private:
        std::filesystem::path root_;

        std::string locale_;
    };
}
