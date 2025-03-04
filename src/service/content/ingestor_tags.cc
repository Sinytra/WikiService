#include <database.h>


#include "game_content.h"

#include <drogon/drogon.h>
#include <resolved_db.h>
#include <schemas.h>

#define EXT_JSON ".json"
#define CONTENT_DIR ".content/"

using namespace drogon;
using namespace drogon::orm;
using namespace logging;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    TagsSubIngestor::TagsSubIngestor(const ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log) :
       SubIngestor(proj, log) {}

    Task<PreparationResult> TagsSubIngestor::prepare() {
        PreparationResult result;

        static const std::set<std::string> allowedTypes{"item"};

        const auto docsRoot = project_.getDocsDirectoryPath();
        const auto dataRoot = docsRoot / ".data";
        if (!exists(dataRoot)) {
            co_return result;
        }

        const auto projectModid = project_.getProject().getValueOfModid();
        auto projectLog = *logger_;

        const std::set allowedNamespaces{ResourceLocation::DEFAULT_NAMESPACE, ResourceLocation::COMMON_NAMESPACE, projectModid};

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

                    tagIds_.insert(id);

                    for (const auto values = (*json)["values"]; const auto &value: values) {
                        if (!value.is_string()) {
                            continue; // TODO optional values
                        }
                        if (const auto str = value.get<std::string>(); !str.starts_with("#")) {
                            if (const auto resloc = ResourceLocation::parse(str); resloc && resloc->namespace_ == projectModid) {
                                result.items.insert(str);
                            }
                        }

                        tagEntries_[id].insert(value);
                    }
                }
            }
        }

        co_return result;
    }

    Task<Error> TagsSubIngestor::execute() {
        const auto projectModid = project_.getProject().getValueOfModid();
        auto projectLog = *logger_;

        projectLog.debug("Registering {} tags", tagIds_.size());

        for (const auto &tag: tagIds_) {
            const auto resloc = ResourceLocation::parse(tag);
            if (!resloc) {
                continue;
            }

            std::optional<std::string> project = resloc->namespace_ == projectModid ? std::make_optional(projectModid) : std::nullopt;

            co_await project_.getProjectDatabase().addTag(tag);
        }

        projectLog.debug("Registering tag entries");

        for (const auto &[key, val]: tagEntries_) {
            for (auto &entry: val) {
                const auto isTag = entry.starts_with("#");
                const auto entryId = isTag ? entry.substr(1) : entry;

                if (isTag) {
                    const auto tagId = entry.substr(1);
                    co_await project_.getProjectDatabase().addTagTagEntry(key, entry);
                } else {
                    co_await project_.getProjectDatabase().addTagItemEntry(key, entry);
                }
            }
        }

        co_return Error::Ok;
    }

    Task<Error> TagsSubIngestor::finish() {
        if (tagIds_.size() > 0) {
            logger.debug("Refreshing flat tag->item view after data ingestion");
            co_await global::database->refreshFlatTagItemView();
        }
        co_return Error::Ok;
    }

}