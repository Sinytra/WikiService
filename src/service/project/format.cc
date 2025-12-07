#include "format.h"

#include <service/project/resolved.h>
#include <service/system/lang.h>

#define ASSETS_DIR_PATH ".assets"
#define DATA_DIR_PATH ".data"
#define ASSETS_LANG_DIR_PATH "lang"
#define I18N_DIR_PATH ".translated"
#define CONTENT_DIR_PATH ".content"
#define FOLDER_META_FILE "_meta.json"
#define PROPERTIES_PATH ".data/properties.json"
#define WORKBENCHES_PATH ".data/workbenches.json"
#define WIKI_META_FILE "sinytra-wiki.json"

namespace fs = std::filesystem;

namespace service {
    V0ProjectFormat::V0ProjectFormat(const fs::path &root, const std::string &locale) : root_(root), locale_(locale) {}

    fs::path V0ProjectFormat::getRoot() const {
        return root_;
    }

    void V0ProjectFormat::setLocale(const std::optional<std::string> &locale) {
        locale_ = locale.value_or("");
    }

    std::string V0ProjectFormat::getLocale() const {
        return locale_.empty() ? DEFAULT_LOCALE : locale_;
    }

    fs::path V0ProjectFormat::getWikiMetadataPath() const {
        return root_ / WIKI_META_FILE;
    }

    fs::path V0ProjectFormat::getLocalizedFilePath(const std::string &path) const {
        if (!locale_.empty()) {
            const auto relativePath = relative(path, root_);

            if (const auto localeFile = root_ / I18N_DIR_PATH / locale_ / removeLeadingSlash(path); exists(localeFile)) {
                return localeFile;
            }
        }
        return root_ / removeLeadingSlash(path);
    }

    fs::path V0ProjectFormat::getLocalesPath() const {
        return root_ / I18N_DIR_PATH;
    }

    fs::path V0ProjectFormat::getContentDirectoryPath() const {
        return root_ / CONTENT_DIR_PATH;
    }

    std::filesystem::path V0ProjectFormat::getAssetsRoot() const {
        return root_ / ASSETS_DIR_PATH;
    }

    std::filesystem::path V0ProjectFormat::getDataRoot() const {
        return root_ / DATA_DIR_PATH;
    }

    fs::path V0ProjectFormat::getFolderMetaFilePath(const fs::path &dir) const {
        if (!locale_.empty()) {
            const auto relativePath = relative(dir, root_);

            if (const auto localeFile = root_ / I18N_DIR_PATH / locale_ / relativePath / FOLDER_META_FILE; exists(localeFile)) {
                return localeFile;
            }
        }

        return dir / FOLDER_META_FILE;
    }

    fs::path V0ProjectFormat::getItemPropertiesPath() const {
        return getLocalizedFilePath(PROPERTIES_PATH);
    }

    fs::path V0ProjectFormat::getWorkbenchesPath() const {
        return root_ / WORKBENCHES_PATH;
    }

    fs::path V0ProjectFormat::getAssetsPath(const ResourceLocation &location) const {
        const auto ext = location.path_.find('.') != std::string::npos ? "" : ".png";
        const auto path = ASSETS_DIR_PATH "/" + location.namespace_ + "/" + location.path_ + ext;
        return root_ / path;
    }

    fs::path V0ProjectFormat::getLanguageFilePath(const std::string &namespace_) const {
        return root_ / ASSETS_DIR_PATH / namespace_ / ASSETS_LANG_DIR_PATH / (getLocale() + ".json");
    }
}
