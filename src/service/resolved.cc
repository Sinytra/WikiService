#include "resolved.h"
#include "schemas.h"
#include "util.h"

#include <filesystem>
#include <fstream>
#include <unordered_map>

#include <fmt/args.h>
#include <git2.h>
#include <include/uri.h>

#define DOCS_META_FILE "sinytra-wiki.json"
#define FOLDER_META_FILE "_meta.json"
#define DOCS_FILE_EXT ".mdx"
#define I18N_DIR_PATH ".translated"
#define NO_ICON "_none"

using namespace logging;
using namespace drogon;
namespace fs = std::filesystem;

std::unordered_map<std::string, std::string> GIT_PROVIDERS = {
    { "github.com", "blob/{branch}/{base}/{path}" }
};

std::string formatISOTime(git_time_t time, const int offset) {
    // time += offset * 60;

    const std::time_t utc_time = time;
    const std::tm *tm_ptr = std::gmtime(&utc_time);

    std::ostringstream oss;
    oss << std::put_time(tm_ptr, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static int matchWithParent(const git_commit *commit, const size_t i, const git_diff_options *opts) {
    git_commit *parent;
    git_tree *a, *b;
    git_diff *diff;

    if (git_commit_parent(&parent, commit, i)) {
        return false;
    }
    if (git_commit_tree(&a, parent)) {
        return false;
    }
    if (git_commit_tree(&b, commit)) {
        return false;
    }
    if (git_diff_tree_to_tree(&diff, git_commit_owner(commit), a, b, opts)) {
        return false;
    }

    const int ndeltas = static_cast<int>(git_diff_num_deltas(diff));

    git_diff_free(diff);
    git_tree_free(a);
    git_tree_free(b);
    git_commit_free(parent);

    return ndeltas > 0;
}

std::optional<std::string> getLastCommitDate(const std::string &repoPath, std::string path) {
    git_repository *repo = nullptr;
    if (git_repository_open(&repo, repoPath.c_str()) != 0) {
        logger.error("Failed to open repository: {}", repoPath);
        return std::nullopt;
    }

    constexpr int limit = 1;
    int printed = 0;

    git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;
    git_oid oid;
    git_commit *commit = nullptr;
    git_pathspec *ps = nullptr;
    git_revwalk *walker = nullptr;

    if (git_revwalk_new(&walker, repo)) {
        logger.error("Failed to create rev walker");
        git_repository_free(repo);
        return std::nullopt;
    }
    git_revwalk_sorting(walker, GIT_SORT_TIME);
    if (git_revwalk_push_head(walker)) {
        logger.error("Could not find repository HEAD");
        git_revwalk_free(walker);
        git_repository_free(repo);
        return std::nullopt;
    }

    char *data = path.data();
    diffopts.pathspec.strings = &data;
    diffopts.pathspec.count = 1;
    if (git_pathspec_new(&ps, &diffopts.pathspec)) {
        logger.error("Error building pathspec");
        git_pathspec_free(ps);
        git_revwalk_free(walker);
        git_repository_free(repo);
        return std::nullopt;
    }

    std::string result = "";

    for (; !git_revwalk_next(&oid, walker); git_commit_free(commit)) {
        if (git_commit_lookup(&commit, repo, &oid)) {
            logger.error("Failed to look up commit");
            git_pathspec_free(ps);
            git_revwalk_free(walker);
            git_repository_free(repo);
            return std::nullopt;
        }

        const int parents = static_cast<int>(git_commit_parentcount(commit));

        int unmatched = parents;

        if (parents == 0) {
            git_tree *tree;
            if (git_commit_tree(&tree, commit)) {
                logger.error("Failed to get commit tree");
                git_pathspec_free(ps);
                git_revwalk_free(walker);
                git_repository_free(repo);
                git_commit_free(commit);
                return std::nullopt;
            }
            if (git_pathspec_match_tree(nullptr, tree, GIT_PATHSPEC_NO_MATCH_ERROR, ps) != 0) {
                unmatched = 1;
            }
            git_tree_free(tree);
        } else if (parents == 1) {
            unmatched = matchWithParent(commit, 0, &diffopts) ? 0 : 1;
        } else {
            for (int i = 0; i < parents; ++i) {
                if (matchWithParent(commit, i, &diffopts))
                    unmatched--;
            }
        }

        if (unmatched > 0)
            continue;

        if (printed++ >= limit) {
            git_commit_free(commit);
            break;
        }

        const git_time_t commit_time = git_commit_time(commit);
        const int offset = git_commit_time_offset(commit);

        result = formatISOTime(commit_time, offset);
    }

    git_pathspec_free(ps);
    git_revwalk_free(walker);
    git_repository_free(repo);

    return result.empty() ? std::nullopt : std::optional(result);
}

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

std::string getDocsTreeEntryPath(const std::string &s) { return s.ends_with(DOCS_FILE_EXT) ? s.substr(0, s.size() - 4) : s; }

fs::path getFolderMetaFilePath(const fs::path &rootDir, const fs::path &dir, const std::string &locale) {
    if (!locale.empty()) {
        const auto relativePath = relative(dir, rootDir);

        if (const auto localeFile = rootDir / I18N_DIR_PATH / locale / relativePath / FOLDER_META_FILE; exists(localeFile)) {
            return localeFile;
        }
    }

    return dir / FOLDER_META_FILE;
}

FolderMetadata getFolderMetadata(const Project &project, const fs::path &path) {
    FolderMetadata metadata;
    if (!exists(path)) {
        return metadata;
    }

    std::ifstream ifs(path);
    try {
        nlohmann::ordered_json parsed = nlohmann::ordered_json::parse(ifs);
        if (const auto error = validateJson(schemas::folderMetadata, parsed)) {
            logger.error("Invalid folder metadata: Project: {} Path: {} Error: {}", project.getValueOfId(), path.string(), error->msg);
        } else {
            for (auto &[key, val]: parsed.items()) {
                metadata.keys.push_back(key);
                if (val.is_string()) {
                    metadata.entries.try_emplace(key, val, "");
                } else if (val.is_object()) {
                    metadata.entries.try_emplace(key, val.contains("name") ? val["name"].get<std::string>() : getDocsTreeEntryName(key),
                                                 val.contains("icon") ? (val["icon"].is_null() ? NO_ICON : val.value("icon", "")) : "");
                }
            }
        }
        ifs.close();
    } catch (const nlohmann::json::parse_error &e) {
        ifs.close();
        logger.error("Error parsing folder metadata: Project '{}', path {}", project.getValueOfId(), path.string());
        logger.error("JSON parse error (getFolderMetadata): {}", e.what());
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

nlohmann::ordered_json getDirTreeJson(const Project &project, const fs::path &rootDir, const fs::path &dir, const std::string &locale) {
    nlohmann::ordered_json root(nlohmann::json::value_t::array);

    const auto metaFilePath = getFolderMetaFilePath(rootDir, dir, locale);
    auto [keys, entries] = getFolderMetadata(project, metaFilePath);

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
        const auto relativePath = relative(entry.path(), rootDir);

        const auto [name, icon] = entries.contains(fileName) ? entries[fileName] : FolderMetadataEntry{getDocsTreeEntryName(fileName), ""};
        nlohmann::ordered_json obj;
        obj["name"] = name;
        if (!icon.empty()) {
            obj["icon"] = icon;
        }
        obj["path"] = getDocsTreeEntryPath(relativePath);
        obj["type"] = entry.is_directory() ? "dir" : "file";
        if (entry.is_directory()) {
            nlohmann::ordered_json children = getDirTreeJson(project, rootDir, entry.path(), locale);
            obj["children"] = children;
        }
        root.push_back(obj);
    }

    return root;
}

namespace service {
    std::string projectErrorToString(const ProjectError status) {
        switch (status) {
            case ProjectError::OK:
                return "ok";
            case ProjectError::REQUIRES_AUTH:
                return "requires_auth";
            case ProjectError::NO_REPOSITORY:
                return "no_repository";
            case ProjectError::REPO_TOO_LARGE:
                return "repo_too_large";
            case ProjectError::NO_BRANCH:
                return "no_branch";
            case ProjectError::NO_PATH:
                return "no_path";
            case ProjectError::INVALID_META:
                return "invalid_meta";
            default:
                return "unknown";
        }
    }

    std::string formatEditUrl(const Project &project, const std::string &filePath) {
        const uri parsed{project.getValueOfSourceRepo()};
        const auto domain = parsed.get_host();

        const auto provider = GIT_PROVIDERS.find(domain);
        if (provider == GIT_PROVIDERS.end()) {
            return "";
        }

        fmt::dynamic_format_arg_store<fmt::format_context> store;
        store.push_back(fmt::arg("branch", project.getValueOfSourceBranch()));
        store.push_back(fmt::arg("base", removeLeadingSlash(project.getValueOfSourcePath())));
        store.push_back(fmt::arg("path", removeTrailingSlash(filePath)));
        const std::string result = fmt::vformat(provider->second, store);

        return removeTrailingSlash(project.getValueOfSourceRepo()) + "/" + result;
    }

    ResolvedProject::ResolvedProject(const Project &p, const std::filesystem::path &r, const std::filesystem::path &d) :
        project_(p), defaultVersion_(nullptr), rootDir_(r), docsDir_(d) {}

    void ResolvedProject::setDefaultVersion(const ResolvedProject &defaultVersion) {
        defaultVersion_ = std::make_shared<ResolvedProject>(defaultVersion);
    }

    bool ResolvedProject::setLocale(const std::optional<std::string> &locale) {
        if (!locale || hasLocale(*locale)) {
            locale_ = locale.value_or("");
            return true;
        }
        return false;
    }

    bool ResolvedProject::hasLocale(const std::string &locale) const {
        const auto locales = getLocales();
        return locales.contains(locale);
    }

    std::set<std::string> ResolvedProject::getLocales() const {
        std::set<std::string> locales;

        if (const auto localesPath = docsDir_ / I18N_DIR_PATH; exists(localesPath) && is_directory(localesPath)) {
            for (const auto &entry: fs::directory_iterator(localesPath)) {
                if (entry.is_directory()) {
                    locales.insert(entry.path().filename().string());
                }
            }
        }

        return locales;
    }

    std::optional<std::unordered_map<std::string, std::string>> ResolvedProject::getAvailableVersionsFiltered() const {
        const auto versions = getAvailableVersions();

        return versions.empty() ? std::nullopt : std::make_optional(versions);
    }

    std::unordered_map<std::string, std::string> ResolvedProject::getAvailableVersions() const {
        if (defaultVersion_) {
            return defaultVersion_->getAvailableVersions();
        }

        const auto defaultBranch = project_.getValueOfSourceBranch();
        const auto metaFilePath = rootDir_ / removeLeadingSlash(project_.getValueOfSourcePath()) / DOCS_META_FILE;

        std::unordered_map<std::string, std::string> versions;

        if (const auto jf = parseJsonFile(metaFilePath)) {
            if (jf->contains("versions") && (*jf)["versions"].is_object()) {
                for (const auto &[key, val]: (*jf)["versions"].items()) {
                    if (val.is_string() && val.get<std::string>() != defaultBranch) {
                        versions[key] = val;
                    }
                }
            }
        }

        return versions;
    }

    fs::path getFilePath(const fs::path &rootDir, const std::string &path, const std::string &locale) {
        if (!locale.empty()) {
            const auto relativePath = relative(path, rootDir);

            if (const auto localeFile = rootDir / I18N_DIR_PATH / locale / removeLeadingSlash(path); exists(localeFile)) {
                return localeFile;
            }
        }
        return rootDir / removeLeadingSlash(path);
    }

    std::tuple<ProjectPage, Error> ResolvedProject::readFile(std::string path) const {
        const auto filePath = getFilePath(docsDir_, removeLeadingSlash(path), locale_);

        std::ifstream file(filePath);

        if (!file) {
            return {{"", "", ""}, Error::ErrInternal};
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        file.close();

        const auto updatedAt = getLastCommitDate(rootDir_, removeLeadingSlash(project_.getValueOfSourcePath()) + '/' + path).value_or("");
        const auto editUrl = formatEditUrl(project_, path);

        return {{buffer.str(), editUrl, updatedAt}, Error::Ok};
    }

    std::tuple<nlohmann::ordered_json, Error> ResolvedProject::getDirectoryTree() const {
        const auto tree = getDirTreeJson(project_, docsDir_, docsDir_, locale_);
        return {tree, Error::Ok};
    }

    std::string getAssetPath(const ResourceLocation &location) {
        return ".assets/" + location.namespace_ + "/" + location.path_ + (location.path_.find('.') != std::string::npos ? "" : ".png");
    }

    std::optional<std::filesystem::path> ResolvedProject::getAsset(const ResourceLocation &location) const {
        if (const auto filePath = docsDir_ / getAssetPath(location); exists(filePath)) {
            return filePath;
        }

        // Legacy asset path fallback
        if (const auto legacyFilePath = docsDir_ / getAssetPath({.namespace_ = "item", .path_ = location.namespace_ + '/' + location.path_});
            exists(legacyFilePath))
        {
            return legacyFilePath;
        }

        return std::nullopt;
    }

    const Project &ResolvedProject::getProject() const { return project_; }

    const std::filesystem::path &ResolvedProject::getDocsDirectoryPath() const { return docsDir_; }

    Json::Value ResolvedProject::toJson() const {
        const auto versions = getAvailableVersions();
        const auto locales = getLocales();

        Json::Value projectJson = projectToJson(project_);

        if (!versions.empty()) {
            Json::Value versionsJson;
            for (const auto &[key, val]: versions) {
                versionsJson[key] = val;
            }
            projectJson["versions"] = versionsJson;
        }

        if (!locales.empty()) {
            Json::Value localesJson(Json::arrayValue);
            for (const auto &item: locales) {
                localesJson.append(item);
            }
            projectJson["locales"] = localesJson;
        }

        return projectJson;
    }

    std::tuple<std::optional<nlohmann::json>, ProjectError, std::string> ResolvedProject::validateProjectMetadata() const {
        const auto path = docsDir_ / DOCS_META_FILE;
        if (!exists(path)) {
            return {std::nullopt, ProjectError::NO_PATH, ""};
        }

        const auto meta = parseJsonFile(path);
        if (!meta) {
            return {std::nullopt, ProjectError::INVALID_META, ""};
        }

        if (const auto error = validateJson(schemas::projectMetadata, *meta)) {
            auto field = error->pointer.to_string();
            if (field.starts_with('/')) {
                field = field.substr(1);
            }
            const auto detail = field + ": " + error->msg;
            return {std::nullopt, ProjectError::INVALID_META, detail};
        }

        return {*meta, ProjectError::OK, ""};
    }
}
