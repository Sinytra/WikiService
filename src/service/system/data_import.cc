#include <drogon/HttpAppFramework.h>
#include <log/log.h>
#include "data_import.h"

#include <service/database/database.h>
#include <schemas/schemas.h>
#include <service/storage/storage.h>
#include <service/util.h>

using namespace drogon;
using namespace service;
using namespace logging;

struct GameDataItem {
    std::string id;

    friend void from_json(const nlohmann::json &j, GameDataItem &obj) { j.at("id").get_to(obj.id); }
};

struct GameDataTag {
    std::string id;
    std::vector<std::string> entries;

    friend void from_json(const nlohmann::json &j, GameDataTag &obj) {
        j.at("id").get_to(obj.id);
        j.at("entries").get_to(obj.entries);
    }
};

struct GameRecipeIngredient {
    std::string id;
    int count;
    int slot;
    bool input;

    friend void from_json(const nlohmann::json &j, GameRecipeIngredient &obj) {
        j.at("id").get_to(obj.id);
        j.at("count").get_to(obj.count);
        j.at("slot").get_to(obj.slot);
        j.at("input").get_to(obj.input);
    }
};

struct GameRecipe {
    std::string id;
    std::string type;
    std::vector<GameRecipeIngredient> ingredients;

    friend void from_json(const nlohmann::json &j, GameRecipe &obj) {
        j.at("id").get_to(obj.id);
        j.at("type").get_to(obj.type);
        j.at("ingredients").get_to(obj.ingredients);
    }
};

Task<Error> importSystemGameData(const Database &db, const std::vector<GameDataItem> &items, const std::vector<GameDataTag> &tags,
                                 const std::vector<GameRecipe> &recipes) {
    logger.debug("Importing {} items", items.size());
    for (auto &[id]: items) {
        co_await db.addItem(id);
    }

    logger.debug("Importing {} tags", tags.size());
    int entryCount = 0;
    for (auto &[tag, entries]: tags) {
        co_await db.addTag(tag);
        entryCount += entries.size();
    }

    logger.debug("Importing {} tag entries", entryCount);
    for (auto &[tag, entries]: tags) {
        for (auto &entry: entries) {
            if (entry.starts_with("#")) {
                co_await db.addTagTagEntry(tag, entry.substr(1));
            } else {
                co_await db.addTagItemEntry(tag, entry);
            }
        }
    }

    logger.debug("Importing {} recipes", recipes.size());
    for (auto &[id, type, ingredients]: recipes) {
        if (const auto recipe = co_await db.addRecipe(id, type); recipe != -1) {
            // Existing recipe
            if (recipe == 0) {
                logger.trace("Skipping duplicate recipe {}", id);
                continue;
            }

            for (const auto &[id, count, slot, input]: ingredients) {
                if (id.starts_with("#")) {
                    co_await db.addRecipeIngredientTag(recipe, id.substr(1), slot, count, input);
                } else {
                    co_await db.addRecipeIngredientItem(recipe, id, slot, count, input);
                }
            }
        } else {
            logger.error("Error adding recipe {}, aborting", id);
            co_return Error::ErrInternal;
        }
    }

    co_return Error::Ok;
}

Task<Error> executeImport(const DataImport &data, const std::vector<GameDataItem> &items, const std::vector<GameDataTag> &tags,
                          const std::vector<GameRecipe> &recipes) {
    const auto clientPtr = app().getFastDbClient();

    try {
        const auto transPtr = co_await clientPtr->newTransactionCoro();
        const Database transDb{transPtr};

        const auto res = co_await importSystemGameData(transDb, items, tags, recipes);
        if (res == Error::Ok) {
            co_await transDb.addDataImportRecord(data);
        }

        co_return res;
    } catch (std::exception e) {
        logger.error("Error ingesting system data: {}", e.what());

        co_return Error::ErrInternal;
    }
}

Task<Error> sys::SystemDataImporter::importData(nlohmann::json rawData) const {
    if (const auto error = validateJson(schemas::gameDataExport, rawData)) {
        logger.error("Invalid system data file supplied");
        co_return Error::ErrBadRequest;
    }

    std::string version = rawData["version"];
    std::string loader = rawData["loader"];
    std::string loaderVersion = rawData["loader_version"];
    std::string created_at = rawData["created_at"];

    logger.info("== Importing system data ==");
    logger.info("Game version: {}", version);
    logger.info("Loader: {} {}", loader, loaderVersion);
    logger.info("Exported at: {}", created_at);

    const std::vector<GameDataItem> items = rawData["items"];
    const std::vector<GameDataTag> tags = rawData["tags"];
    const std::vector<GameRecipe> recipes = rawData["recipes"];

    DataImport data;
    data.setUserIdToNull();
    data.setGameVersion(version);
    data.setLoader(loader);
    data.setLoaderVersion(loaderVersion);

    const auto res = co_await executeImport(data, items, tags, recipes);
    if (res == Error::Ok) {
        logger.info("System data ingestion successful");
    }
    co_return res;
}

Task<Error> sys::runInitialDeployments() {
    const auto candidateProjects = co_await global::database->getUndeployedProjects();
    int total = candidateProjects.size();
    logger.info("Running initial deployments on {} projects", total);

    int i = 0;
    for (const auto &id : candidateProjects) {
        logger.info("Deploying project {} [{}/{}]", id, ++i, total);

        const auto project = co_await global::database->getProjectSource(id);
        if (!project) {
            logger.error("Project {} not found, skipping", id);
            continue;
        }

        if (const auto deployment = co_await global::storage->deployProject(*project, ""); !deployment) {
            logger.error("Failed to deploy project {}", id);
        } else {
            logger.info("Project {} deployment successful", id);
        }
    }

    co_return Error::Ok;
}