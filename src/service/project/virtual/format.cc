#include "format.h"

namespace fs = std::filesystem;

namespace service {
    VirtualProjectFormat::VirtualProjectFormat(const fs::path &root) : root_(root) {}

    fs::path VirtualProjectFormat::getRoot() const { return root_; }

    std::filesystem::path VirtualProjectFormat::getAssetsRoot() const {
        return root_ / "assets";
    }

    std::filesystem::path VirtualProjectFormat::getDataRoot() const {
        return root_ / "data";
    }

    fs::path VirtualProjectFormat::getLocalesPath() const {
        throw std::runtime_error("getLocalesPath not implemented in VirtualProjectFormat");
    }
    fs::path VirtualProjectFormat::getContentDirectoryPath() const {
        throw std::runtime_error("getContentDirectoryPath not implemented in VirtualProjectFormat");
    }
    fs::path VirtualProjectFormat::getWikiMetadataPath() const {
        throw std::runtime_error("getWikiMetadataPath not implemented in VirtualProjectFormat");
    }
    fs::path VirtualProjectFormat::getLocalizedFilePath(const std::string &path) const {
        throw std::runtime_error("getLocalizedFilePath not implemented in VirtualProjectFormat");
    }
    fs::path VirtualProjectFormat::getFolderMetaFilePath(const fs::path &dir) const {
        throw std::runtime_error("getFolderMetaFilePath not implemented in VirtualProjectFormat");
    }
    fs::path VirtualProjectFormat::getItemPropertiesPath() const {
        throw std::runtime_error("getItemPropertiesPath not implemented in VirtualProjectFormat");
    }
    fs::path VirtualProjectFormat::getWorkbenchesPath() const {
        throw std::runtime_error("getWorkbenchesPath not implemented in VirtualProjectFormat");
    }
    fs::path VirtualProjectFormat::getAssetsPath(const ResourceLocation &location) const {
        throw std::runtime_error("getAssetPath not implemented in VirtualProjectFormat");
    }
    fs::path VirtualProjectFormat::getLanguageFilePath(const std::string &namespace_) const {
        throw std::runtime_error("getLanguageFilePath not implemented in VirtualProjectFormat");
    }
}
