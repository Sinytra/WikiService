#pragma once

#include <service/project/format.h>

namespace service {
    class VirtualProjectFormat : public ProjectFormat {
    public:
        explicit VirtualProjectFormat(const std::filesystem::path &);

        std::filesystem::path getRoot() const override;

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
    };
}