#include "game_content.h"

#include <database.h>
#include <drogon/drogon.h>
#include <fstream>
#include <models/Recipe.h>
#include <schemas.h>

#define EXT_MDX ".mdx"
#define EXT_JSON ".json"
#define CONTENT_DIR ".content/"

using namespace drogon;
using namespace drogon::orm;
using namespace logging;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    Ingestor::Ingestor(ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log) : project_(proj), logger_(log) {}

    Task<Error> wipeExistingData(const DbClientPtr clientPtr, const std::string id) {
        logger.info("Wiping existing data for project {}", id);
        // language=postgresql
        co_await clientPtr->execSqlCoro("DELETE FROM recipe WHERE project_id = $1", id);
        // language=postgresql
        co_await clientPtr->execSqlCoro("DELETE FROM project_item WHERE project_id = $1", id);
        // language=postgresql
        co_await clientPtr->execSqlCoro("DELETE FROM project_tag WHERE project_id = $1", id);
        co_return Error::Ok;
    }

    Task<> Ingestor::addProjectItem(const DbClientPtr clientPtr, const std::string project, const std::string item) const {
        // language=postgresql
        co_await clientPtr->execSqlCoro("INSERT INTO item VALUES (DEFAULT, $1) ON CONFLICT DO NOTHING", item);
        // language=postgresql
        static constexpr auto query = "INSERT INTO project_item (item_id, project_id) \
                                       SELECT id, $2 FROM item WHERE loc = $1 \
                                       ON CONFLICT DO NOTHING";
        co_await clientPtr->execSqlCoro(query, item, project);
    }

    Task<> Ingestor::addTag(const DbClientPtr clientPtr, const std::string tag, const std::optional<std::string> project) const {
        // language=postgresql
        co_await clientPtr->execSqlCoro("INSERT INTO tag VALUES (DEFAULT, $1) ON CONFLICT DO NOTHING", tag);
        // language=postgresql
        static constexpr auto query = "INSERT INTO project_tag (tag_id, project_id) \
                                       SELECT id, $2 FROM tag WHERE loc = $1 \
                                       ON CONFLICT DO NOTHING";
        if (project) {
            co_await clientPtr->execSqlCoro(query, tag, project);
        } else {
            co_await clientPtr->execSqlCoro(query, tag, nullptr);
        }
    }

    Task<Error> Ingestor::ingestContentPaths() const {
        const auto clientPtr = app().getFastDbClient();

        const auto docsRoot = project_.getDocsDirectoryPath();
        const auto projectId = project_.getProject().getValueOfId();
        std::set<std::string> items;

        for (const auto &entry: fs::recursive_directory_iterator(docsRoot)) {
            const auto fileName = entry.path().filename().string();
            if (!entry.is_regular_file() || entry.path().extension() != EXT_MDX ||
                fileName.starts_with(".") && !fileName.starts_with(CONTENT_DIR))
            {
                continue;
            }
            fs::path relative_path = relative(entry.path(), docsRoot);
            if (const auto id = project_.readPageAttribute(relative_path.string(), "id")) {
                if (items.contains(*id)) {
                    logger_->warn("Skipping duplicate item {} path {}", *id, relative_path.string());
                    continue;
                }

                logger_->debug("Found entry '{}' at '{}'", *id, relative_path.string());

                items.insert(*id);
                co_await addProjectItem(clientPtr, projectId, *id);

                // language=postgresql
                static constexpr auto pageQuery = "INSERT INTO project_item_page (item_id, path) \
                                                   SELECT pitem.id as item_id, $1 as path \
                                                   FROM project_item pitem \
                                                   JOIN item i ON pitem.item_id = i.id \
                                                   WHERE i.loc = $2 AND pitem.project_id = $3";
                co_await clientPtr->execSqlCoro(pageQuery, relative_path.string(), *id, projectId);
            }
        }

        logger_->info("Added {} entries", items.size());

        co_return Error::Ok;
    }

    Task<Error> Ingestor::ingestTags() const {
        static const std::set<std::string> allowedTypes{"item"};

        const auto docsRoot = project_.getDocsDirectoryPath();
        const auto dataRoot = docsRoot / ".data";
        if (!exists(dataRoot)) {
            co_return Error::Ok;
        }

        const auto clientPtr = app().getFastDbClient();
        const auto projectId = project_.getProject().getValueOfId();
        const auto projectModid = project_.getProject().getValueOfModid();
        auto projectLog = *logger_;

        const std::set allowedNamespaces{ResourceLocation::DEFAULT_NAMESPACE, ResourceLocation::COMMON_NAMESPACE, projectModid};

        std::set<std::string> candidateTags;
        std::unordered_map<std::string, std::set<std::string>> candidateTagEntries;

        projectLog.info("======= Ingesting item tags =======");

        // [namespace]
        for (const auto &namespaceDir: fs::directory_iterator(dataRoot)) {
            if (!namespaceDir.is_directory()) {
                continue;
            }

            // [namespace]/tags/
            const auto tagsRoot = namespaceDir.path() / "tags";
            if (!exists(tagsRoot)) {
                continue;
            }

            const auto nmspace = namespaceDir.path().filename().string();

            if (!allowedNamespaces.contains(nmspace)) {
                projectLog.warn("Skipping ignored tag namespace {}", nmspace);
                continue;
            }

            // [namespace]/tags/[type]
            for (const auto &tagTypeDir: fs::directory_iterator(tagsRoot)) {
                const auto type = tagTypeDir.path().filename().string();

                if (!allowedTypes.contains(type)) {
                    projectLog.warn("Skipping ignored tag type {}", type);
                    continue;
                }

                // [namespace]/tags/[type]/[..path]
                for (const auto &tagFile: fs::recursive_directory_iterator(tagTypeDir)) {
                    if (!tagFile.is_regular_file() || tagFile.path().extension() != EXT_JSON) {
                        continue;
                    }

                    const auto relativePath = relative(tagFile, tagTypeDir);
                    const auto fileName = relativePath.string();
                    const auto id = nmspace + ":" + fileName.substr(0, fileName.find_last_of('.'));

                    const auto json = parseJsonFile(tagFile);
                    if (!json) {
                        logger.warn("Failed to read tag {}", id);
                        continue;
                    }
                    if (const auto error = validateJson(schemas::gameTag, *json)) {
                        logger.warn("Skipping invalid tag {}", id);
                        continue;
                    }

                    candidateTags.insert(id);

                    for (const auto values = (*json)["values"]; const auto &value: values) {
                        if (!value.is_string()) {
                            continue; // TODO optional values
                        }

                        candidateTagEntries[id].insert(value);
                    }
                }
            }
        }

        projectLog.debug("Adding {} tags", candidateTags.size());

        for (auto &tag: candidateTags) {
            const auto resloc = ResourceLocation::parse(tag);
            if (!resloc) {
                continue;
            }

            std::optional<std::string> project = resloc->namespace_ == projectModid ? std::make_optional(projectModid) : std::nullopt;

            co_await addTag(clientPtr, tag, project);
        }

        projectLog.debug("Adding tag entries");

        for (auto &[key, val]: candidateTagEntries) {
            for (auto &entry: val) {
                const auto isTag = entry.starts_with("#");
                const auto entryId = isTag ? entry.substr(1) : entry;

                const auto resloc = ResourceLocation::parse(entry);
                if (!resloc) {
                    continue;
                }

                if (isTag) {
                    const auto tagId = entry.substr(1);
                    co_await global::database->addTagTagEntry(projectId, key, entry);
                } else {
                    if (resloc->namespace_ == projectModid) {
                        co_await addProjectItem(clientPtr, projectId, entry);
                    }

                    co_await global::database->addTagItemEntry(projectId, key, entry);
                }
            }
        }

        co_return Error::Ok;
    }

    Task<Error> Ingestor::ingestGameContentData() const {
        auto projectLog = *logger_;

        const auto projectId = project_.getProject().getValueOfId();
        projectLog.info("====================================");
        projectLog.info("Ingesting game data for project {}", projectId);

        const auto clientPtr = app().getFastDbClient();

        co_await wipeExistingData(clientPtr, projectId);

        co_await ingestTags();

        co_await ingestContentPaths();

        co_await ingestRecipes();

        // TODO Refresh flat tag item view

        co_return Error::Ok;
    }
}
