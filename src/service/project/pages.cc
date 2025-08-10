#include <fmt/args.h>
#include <fstream>
#include <regex>
#include <service/resolved.h>
#include <service/schemas.h>
#include <service/database/resolved_db.h>

#include "git_hosts.h"
#include "storage/storage.h"

#define NO_ICON "_none"

#define TITLE_ATTR "title"
#define H1_REGEX R"(^#\s+([^\n<>*_`]+)$)"

using namespace logging;
using namespace drogon;
using namespace drogon_model::postgres;
namespace fs = std::filesystem;

struct FolderMetadataEntry {
    const std::string name;
    const std::string icon;
};

struct FolderMetadata {
    std::vector<std::string> keys;
    std::map<std::string, FolderMetadataEntry> entries;
};

std::string getDocsTreeEntryName(std::string s) {
    if (s.ends_with(DOCS_FILE_EXT)) {
        s = s.substr(0, s.size() - 4);
    }
    return toCamelCase(s);
}

FolderMetadata getFolderMetadata(const service::ResolvedProject &resolved, const fs::path &path) {
    FolderMetadata metadata;
    if (!exists(path)) {
        return metadata;
    }

    std::ifstream ifs(path);
    const auto rel = relative(path, resolved.getRootDirectory());
    try {
        nlohmann::ordered_json parsed = nlohmann::ordered_json::parse(ifs);

        if (const auto error = validateJson(schemas::folderMetadata, parsed)) {
            logger.error("Invalid folder metadata: Project: {} Path: {} Error: {}", resolved.getProject().getValueOfId(), path.string(),
                         error->msg);

            global::storage->addProjectIssueAsync(resolved, service::ProjectIssueLevel::ERROR, service::ProjectIssueType::FILE,
                                                  service::ProjectError::INVALID_FORMAT, error->msg, rel);
        } else {
            for (auto &[key, val]: parsed.items()) {
                metadata.keys.push_back(key);
                if (val.is_string()) {
                    metadata.entries.try_emplace(key, val, "");
                } else if (val.is_object()) {
                    const auto name = val.contains("name") ? val["name"].get<std::string>() : getDocsTreeEntryName(key);
                    const auto icon = val.contains("icon") ? (val["icon"].is_null() ? NO_ICON : val.value("icon", "")) : "";
                    metadata.entries.try_emplace(key, name, icon);
                }
            }
        }

        ifs.close();
    } catch (const nlohmann::json::parse_error &error) {
        ifs.close();
        logger.error("Error parsing folder metadata: Project '{}', path {}", resolved.getProject().getValueOfId(), path.string());
        logger.error("JSON parse error (getFolderMetadata): {}", error.what());

        global::storage->addProjectIssueAsync(resolved, service::ProjectIssueLevel::ERROR, service::ProjectIssueType::FILE,
                                              service::ProjectError::INVALID_FILE, error.what(), rel);
    }
    return metadata;
}

auto createDirEntriesComparator(const std::vector<std::string> &keys) {
    return [&keys](const fs::directory_entry &a, const fs::directory_entry &b) {
        if (keys.empty()) {
            return a.path().filename().string() < b.path().filename().string();
        }
        const auto aPos = std::ranges::find(keys, a.path().filename().string());
        const auto bPos = std::ranges::find(keys, b.path().filename().string());
        if (aPos == keys.end() && bPos == keys.end()) {
            return false;
        }
        if (aPos == keys.end()) {
            return false;
        }
        if (bPos == keys.end()) {
            return true;
        }

        const auto aIdx = std::distance(keys.begin(), aPos);
        const auto bIdx = std::distance(keys.begin(), bPos);
        return aIdx < bIdx;
    };
}

// TODO Abolish JSON, use struct
nlohmann::ordered_json getDirTreeJson(const service::ResolvedProject &resolved, const fs::path &dir) {
    nlohmann::ordered_json root(nlohmann::json::value_t::array);

    const auto format = resolved.getFormat();
    const auto metaFilePath = format.getFolderMetaFilePath(dir);
    auto [keys, entries] = getFolderMetadata(resolved, metaFilePath);

    std::vector<fs::directory_entry> paths;
    for (const auto &entry: fs::directory_iterator(dir)) {
        if (const auto fileName = entry.path().filename().string();
            (entry.is_directory() || entry.is_regular_file() && fileName.ends_with(DOCS_FILE_EXT)) && !fileName.starts_with(".") &&
            !fileName.starts_with("_"))
        {
            paths.push_back(entry);
        }
    }
    std::ranges::sort(paths, createDirEntriesComparator(keys));

    for (const auto &entry: paths) {
        const auto fileName = entry.path().filename().string();
        const auto relativePath = relative(entry.path(), format.getRoot()).string();
        const auto displayPath = relativePath.substr(0, relativePath.size() - 4);

        const auto [name, icon] = entries.contains(fileName) ? entries[fileName] : FolderMetadataEntry{getDocsTreeEntryName(fileName), ""};
        nlohmann::ordered_json obj;
        obj["name"] = name;
        if (!icon.empty()) {
            obj["icon"] = icon;
        }
        obj["path"] = displayPath;
        obj["type"] = entry.is_directory() ? "dir" : "file";
        if (entry.is_directory()) {
            nlohmann::ordered_json children = getDirTreeJson(resolved, entry.path());
            obj["children"] = children;
        }
        root.push_back(obj);
    }

    return root;
}

std::optional<std::string> readPageHeading(const std::string &filePath) {
    static const std::regex headingRegex(H1_REGEX, std::regex_constants::ECMAScript);

    std::ifstream ifs(filePath);
    if (!ifs) {
        return std::nullopt;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.starts_with("# ")) {
            if (std::smatch match; std::regex_search(line, match, headingRegex)) {
                if (match.size() > 1) {
                    return match[1].str();
                }
            }
        } else if (line.starts_with("#")) {
            break;
        }
    }

    ifs.close();
    return std::nullopt;
}

std::string formatEditUrl(const Project &project, const std::string &filePath) {
    const auto provider = service::getGitProvider(project.getValueOfSourceRepo());
    if (!provider) {
        return "";
    }

    fmt::dynamic_format_arg_store<fmt::format_context> store;
    store.push_back(fmt::arg("branch", project.getValueOfSourceBranch()));
    store.push_back(fmt::arg("base", removeLeadingSlash(project.getValueOfSourcePath())));
    store.push_back(fmt::arg("path", removeTrailingSlash(filePath)));
    const auto result = vformat(provider->filePath, store);

    return removeTrailingSlash(project.getValueOfSourceRepo()) + "/" + result;
}

namespace service {
    std::optional<std::string> ResolvedProject::getPagePath(const std::string &path) const {
        const auto filePath = format_.getLocalizedFilePath(removeLeadingSlash(path) + DOCS_FILE_EXT);
        if (!exists(filePath))
            return std::nullopt;
        return relative(filePath, docsDir_).string();
    }

    std::optional<std::string> ResolvedProject::getPageAttribute(const std::string &path, const std::string &prop) const {
        const auto filePath = format_.getLocalizedFilePath(removeLeadingSlash(path));

        std::ifstream ifs(filePath);
        if (!ifs) {
            return std::nullopt;
        }

        std::string line;
        int frontMatterBorder = 0;
        while (std::getline(ifs, line)) {
            if (frontMatterBorder > 1) {
                break;
            }
            if (line.starts_with("---")) {
                frontMatterBorder++;
            }
            if (frontMatterBorder == 1 && line.starts_with(prop + ":")) {
                if (const auto pos = line.find(":"); pos != std::string::npos) {
                    auto sub = line.substr(pos + 1);
                    ifs.close();
                    ltrim(sub);
                    rtrim(sub);
                    return sub;
                }
            }
        }
        ifs.close();
        return std::nullopt;
    }

    std::optional<std::string> ResolvedProject::getPageTitle(const std::string &path) const {
        const auto title = getPageAttribute(path, TITLE_ATTR);
        return title ? title : readPageHeading(format_.getLocalizedFilePath(removeLeadingSlash(path)));
    }

    std::tuple<ProjectPage, Error> ResolvedProject::readPageFile(std::string path) const {
        const auto filePath = format_.getLocalizedFilePath(removeLeadingSlash(path));

        std::ifstream file(filePath);

        if (!file) {
            return {{"", ""}, Error::ErrNotFound};
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        file.close();

        const auto editUrl = formatEditUrl(project_, path);

        return {{.content = buffer.str(), .editUrl = editUrl}, Error::Ok};
    }

    Task<std::tuple<ProjectPage, Error>> ResolvedProject::readContentPage(const std::string id) const {
        const auto contentPath = co_await projectDb_->getProjectContentPath(id);
        if (!contentPath) {
            co_return {{"", ""}, Error::ErrNotFound};
        }
        co_return readPageFile(*contentPath);
    }

    std::tuple<nlohmann::ordered_json, Error> ResolvedProject::getDirectoryTree() const {
        const auto tree = getDirTreeJson(*this, docsDir_);
        return {tree, Error::Ok};
    }

    std::tuple<nlohmann::ordered_json, Error> ResolvedProject::getContentDirectoryTree() const {
        const auto contentPath = getFormat().getContentDirectoryPath();
        if (!exists(contentPath)) {
            return {nlohmann::ordered_json(), Error::ErrNotFound};
        }
        const auto tree = getDirTreeJson(*this, contentPath);
        return {tree, Error::Ok};
    }
}
