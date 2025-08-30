#include "format.h"

#include <service/system/lang.h>
#include <service/resolved.h>

#define ASSETS_DIR_PATH ".assets"
#define ASSETS_LANG_DIR_PATH "lang"
#define I18N_DIR_PATH ".translated"
#define CONTENT_DIR_PATH ".content"
#define FOLDER_META_FILE "_meta.json"
#define PROPERTIES_PATH ".data/properties.json"
#define WORKBENCHES_PATH ".data/workbenches.json"
#define WIKI_META_FILE "sinytra-wiki.json"

namespace fs = std::filesystem;

namespace service {
    ProjectFormat::ProjectFormat(const fs::path &root, const std::string &locale) : root_(root), locale_(locale) {}

    fs::path ProjectFormat::getRoot() const {
        return root_;
    }

    void ProjectFormat::setLocale(const std::optional<std::string> &locale) {
        locale_ = locale.value_or("");
    }

    std::string ProjectFormat::getLocale() const {
        return locale_.empty() ? DEFAULT_LOCALE : locale_;
    }

    fs::path ProjectFormat::getWikiMetadataPath() const {
        return root_ / WIKI_META_FILE;
    }

    fs::path ProjectFormat::getLocalizedFilePath(const std::string &path) const {
        if (!locale_.empty()) {
            const auto relativePath = relative(path, root_);

            if (const auto localeFile = root_ / I18N_DIR_PATH / locale_ / removeLeadingSlash(path); exists(localeFile)) {
                return localeFile;
            }
        }
        return root_ / removeLeadingSlash(path);
    }

    fs::path ProjectFormat::getLocalesPath() const {
        return root_ / I18N_DIR_PATH;
    }

    fs::path ProjectFormat::getContentDirectoryPath() const {
        return root_ / CONTENT_DIR_PATH;
    }

    fs::path ProjectFormat::getFolderMetaFilePath(const fs::path &dir) const {
        if (!locale_.empty()) {
            const auto relativePath = relative(dir, root_);

            if (const auto localeFile = root_ / I18N_DIR_PATH / locale_ / relativePath / FOLDER_META_FILE; exists(localeFile)) {
                return localeFile;
            }
        }

        return dir / FOLDER_META_FILE;
    }

    fs::path ProjectFormat::getItemPropertiesPath() const {
        return getLocalizedFilePath(PROPERTIES_PATH);
    }

    fs::path ProjectFormat::getWorkbenchesPath() const {
        return root_ / WORKBENCHES_PATH;
    }

    fs::path ProjectFormat::getAssetPath(const ResourceLocation &location) const {
        const auto ext = location.path_.find('.') != std::string::npos ? "" : ".png";
        const auto path = ASSETS_DIR_PATH "/" + location.namespace_ + "/" + location.path_ + ext;
        return root_ / path;
    }

    fs::path ProjectFormat::getLanguageFilePath(const std::string &namespace_) const {
        return root_ / ASSETS_DIR_PATH / namespace_ / ASSETS_LANG_DIR_PATH / (getLocale() + ".json");
    }
}
