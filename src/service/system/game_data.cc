#include "game_data.h"
#include "lang.h"

#include <fstream>
#include <include/uri.h>
#include <nlohmann/json.hpp>
#include <pugixml.hpp>
#include <service/project/virtual/virtual.h>
#include <service/storage/ingestor/ingestor.h>
#include <service/util.h>
#include <storage/storage.h>
#include <zip.h>

#define LAUNCHER_MANIFEST_URL "https://launchermeta.mojang.com/mc/game/version_manifest_v2.json"
#define RESOURCES_URL "https://resources.download.minecraft.net"

#define NEOFORGE_MAVEN_METADATA "https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml"
#define NEOFORGE_URL "https://maven.neoforged.net/releases/net/neoforged/neoforge/{}/neoforge-{}-universal.jar"

using namespace drogon;
using namespace logging;
using namespace std::chrono;
namespace fs = std::filesystem;

namespace service {
    static const std::set<std::string> extractVanillaDirs = {"assets/minecraft/lang", "assets/minecraft/items", "data/minecraft/recipe",
                                                             "data/minecraft/tags/item"};
    static const std::set<std::string> extractNeoForgeDirs = {"data/c/recipe", "data/c/tags/item"};

    struct VersionManifest {
        std::string version;
        nlohmann::json data;
    };

    bool shouldExtract(const std::string &dir, const std::set<std::string> &filter) {
        if (dir.ends_with("/")) {
            return false;
        }
        if (filter.empty()) {
            return true;
        }
        for (const auto &item: filter) {
            if (dir.starts_with(item)) {
                return true;
            }
        }
        return false;
    }

    void extractZip(const std::filesystem::path &clientPath, const std::filesystem::path &gameDir, const std::set<std::string> &filter) {
        zip_t *archive;
        int err;
        if ((archive = zip_open(absolute(clientPath).c_str(), 0, &err)) == nullptr) {
            throw std::runtime_error("Cannot open zip file");
        }

        const auto num_entries = zip_get_num_entries(archive, 0);
        for (zip_uint64_t i = 0; i < num_entries; ++i) {
            if (const char *name = zip_get_name(archive, i, 0); name && shouldExtract(name, filter)) {
                struct zip_stat st;
                zip_stat_init(&st);
                zip_stat(archive, name, 0, &st);
                std::vector<char> contents(st.size);
                zip_file *f = zip_fopen(archive, name, 0);
                zip_fread(f, contents.data(), st.size);
                zip_fclose(f);

                const auto dest = gameDir / name;
                fs::create_directories(dest.parent_path());

                std::ofstream file(dest, std::ios::out | std::ios::binary | std::ios::trunc);
                if (!file) {
                    throw std::runtime_error("Failed to open output stream to " + dest.string());
                }
                file.write(contents.data(), st.size);
                file.close();
            }
        }

        zip_close(archive);
    }

    Task<TaskResult<HttpResponsePtr>> makeGetRequest(const std::string url) {
        const auto client = createHttpClient(url);
        const uri parsed(url);

        const auto httpReq = HttpRequest::newHttpRequest();
        httpReq->setMethod(Get);
        httpReq->setPath("/" + parsed.get_path());

        logger.debug("Sending GET request to: {}", url);

        const auto response = co_await client->sendRequestCoro(httpReq);
        if (response->getStatusCode() != k200OK) {
            logger.error("Failed to GET {}", url);
            co_return Error::ErrInternal;
        }

        co_return response;
    }

    Task<TaskResult<std::string>> getRemoteFileContents(const std::string url) {
        const auto response = co_await makeGetRequest(url);
        if (!response) {
            co_return response.error();
        }
        const auto body = (*response)->getBody();
        co_return std::string(body);
    }

    Task<> downloadFile(const std::string url, const std::filesystem::path dest) {
        const auto body = co_await getRemoteFileContents(url);
        if (!body) {
            throw std::runtime_error("Failed to download file");
        }

        remove(dest);
        std::ofstream file(dest);
        if (!file) {
            throw std::runtime_error("Failed to open output stream to " + dest.string());
        }
        file << *body;
        file.close();

        co_return;
    }

    Task<nlohmann::json> bufferJsonFile(const std::string url) {
        const auto response = co_await makeGetRequest(url);
        if (!response) {
            throw std::runtime_error("Failed to download file to buffer");
        }

        if (const auto json = (*response)->getJsonObject()) {
            co_return parkourJson(*json);
        }

        throw std::runtime_error("Expected response body to be JSON");
    }

    Task<TaskResult<std::string>> getLatestNeoForgeVersion() {
        const auto body = co_await getRemoteFileContents(NEOFORGE_MAVEN_METADATA);
        if (!body) {
            throw std::runtime_error("Failed to download neoforge maven metadata");
        }

        pugi::xml_document doc;
        if (const auto result = doc.load_string(body->data()); !result) {
            co_return Error::ErrInternal;
        }

        const auto metadata = doc.child("metadata");
        if (!metadata) {
            co_return Error::ErrInternal;
        }
        const auto versioning = metadata.child("versioning");
        if (!versioning) {
            co_return Error::ErrInternal;
        }
        const auto latest = versioning.child("latest");
        if (!latest) {
            co_return Error::ErrInternal;
        }

        co_return std::string(latest.text().get());
    }

    Task<> registerItems(const ProjectDatabaseAccess &db, const fs::path itemsRoot) {
        logger.debug("Registering all game items by asset files");
        int count = 0;
        for (const auto &entry: fs::directory_iterator(itemsRoot)) {
            const auto name = entry.path().filename().string();
            const auto id = "minecraft:" + name.substr(0, name.size() - 5);
            if (const auto res = co_await db.addProjectItem(id); !res) {
                logger.error("Failed to register game item: {}", id);
            }
            count++;
        }
        logger.debug("Registered {} items", count);
    }

    Task<TaskResult<VersionManifest>> resolveLatestGameVersionManifest() {
        // Download launcher manifest
        logger.debug("Fetching launcher manifest");
        const auto launcherManifest = co_await bufferJsonFile(LAUNCHER_MANIFEST_URL);

        // Find and download version manifest
        const std::string latestRelease = launcherManifest["latest"]["release"];
        logger.debug("Found latest release: {}", latestRelease);

        for (auto version: launcherManifest["versions"]) {
            if (version["id"] == latestRelease) {
                // Fetch version manifest
                const auto versionUrl = version["url"];
                logger.debug("Fetching version manifest");

                const auto versionManifest = co_await bufferJsonFile(versionUrl);
                co_return VersionManifest{.version = latestRelease, .data = versionManifest};
            }
        }

        logger.error("Version {} not found in launcher manifest", latestRelease);
        co_return Error::ErrNotFound;
    }

    std::optional<nlohmann::json> GameDataService::getLang(const std::string &name) const {
        const fs::path gameDir = global::virtualProject->getFormat().getRoot();
        const auto langDir = gameDir / "assets/minecraft/lang";
        const auto path = langDir / (name + ".json");
        return parseJsonFile(path);
    }

    Task<TaskResult<>> ingestGameData() {
        static const std::set<std::string> enabledModules{INGESTOR_TAGS};

        logger.info("Ingesting game data");
        ProjectBase &project = *global::virtualProject;
        const auto projectLogger = global::storage->getProjectLogger(project.getProject(), false);
        const auto issues = std::make_shared<ProjectIssueCallback>("", projectLogger);
        const content::Ingestor ingestor{project, projectLogger, issues, enabledModules, false};

        if (const auto result = co_await ingestor.runIngestor(); result != Error::Ok) {
            projectLogger->error("Error ingesting game data");
            co_return Error::ErrInternal;
        }

        if (issues->hasErrors()) {
            projectLogger->error("Encountered issues during game data ingestion");
            co_return Error::ErrInternal;
        }

        const auto itemsRoot = project.getFormat().getAssetsRoot() / "minecraft" / "items";
        co_await registerItems(project.getProjectDatabase(), itemsRoot);

        logger.info("Game data ingestion successful");
        co_return Error::Ok;
    }

    Task<> downloadAdditionalLanguageFiles(const nlohmann::json assetIndex, const fs::path langDir) {
        static constexpr auto langFilePrefix = "minecraft/lang/";

        int langFileCount = 0;
        auto start = high_resolution_clock::now();
        {
            for (const auto &[key, object]: assetIndex["objects"].items()) {
                if (key.starts_with(langFilePrefix)) {
                    const auto fileName = key.substr(strlen(langFilePrefix));
                    const std::string hash = object["hash"];
                    const auto prefix = hash.substr(0, 2);
                    const auto resourceUrl = std::format("{}/{}/{}", RESOURCES_URL, prefix, hash);
                    const auto downloadPath = langDir / fileName;

                    co_await downloadFile(resourceUrl, downloadPath);
                    logger.debug("Downloaded language file {}", fileName);
                    langFileCount++;
                }
            }
        }
        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(stop - start);
        logger.info("Successfully downloaded {} game assets in {} ms.", langFileCount, duration.count());
    }

    Task<> downloadGameFiles(const nlohmann::json versionManifest, const std::string neoForgeVersion) {
        const fs::path gameDir = global::virtualProject->getFormat().getRoot();
        if (exists(gameDir)) {
            remove_all(gameDir);
        }

        logger.info("Downloading game files");
        fs::create_directories(gameDir);

        // Download asset index
        logger.debug("Fetching asset index");
        const auto assetIndexUrl = versionManifest["assetIndex"]["url"];
        const auto assetIndex = co_await bufferJsonFile(assetIndexUrl);

        // Download additional language files
        logger.debug("Downloading additional language files");
        const auto langDir = gameDir / "assets/minecraft/lang";
        fs::create_directories(langDir);
        co_await downloadAdditionalLanguageFiles(assetIndex, langDir);

        // Download client
        logger.info("Downloading client");
        const auto clientUrl = versionManifest["downloads"]["client"]["url"];
        const auto clientDest = gameDir / "client.jar";
        co_await downloadFile(clientUrl, clientDest);
        // Extract client data
        logger.info("Extracting client data");
        extractZip(clientDest, gameDir, extractVanillaDirs);
        // Cleanup
        remove(clientDest);

        std::set filter{extractVanillaDirs};
        filter.insert(extractNeoForgeDirs.begin(), extractNeoForgeDirs.end());
        logger.info("Downloading neoforge jar");
        const auto neoForgeDest = gameDir / "neoforge.jar";
        const auto neoForgeUrl = std::format(NEOFORGE_URL, neoForgeVersion, neoForgeVersion);
        co_await downloadFile(neoForgeUrl, neoForgeDest);
        logger.info("Extracting neoforge jar");
        extractZip(neoForgeDest, gameDir, filter);
        remove(neoForgeDest);

        logger.debug("Game data download successful");
        co_return;
    }

    Task<TaskResult<>> GameDataService::importGameData(bool updateLoader) const {
        try {
            logger.debug("Checking game data status...");
            const auto versionManifest = co_await resolveLatestGameVersionManifest();
            if (!versionManifest) {
                throw std::runtime_error("Could not find version manifest");
            }

            const auto latest = co_await getLatestNeoForgeVersion();
            if (!latest) {
                throw std::runtime_error("Could not find latest neoforge version");
            }
            const auto neoForgeVersion = *latest;

            if (const auto existingImport = co_await global::database->getDataImport(versionManifest->version);
                existingImport && (!updateLoader || existingImport->getValueOfLoaderVersion() == neoForgeVersion))
            {
                logger.debug("Game data up to date, skipping");
                co_return Error::Ok;
            }

            logger.info("Setting up game data");
            co_await downloadGameFiles(versionManifest->data, *latest);
            co_await ingestGameData();
            logger.info("Game data setup complete");

            DataImport import;
            import.setGameVersion(versionManifest->version);
            import.setLoader("neoforge");
            import.setLoaderVersion(neoForgeVersion);
            import.setUserIdToNull();

            if (const auto res = co_await global::database->addModel(import); !res) {
                throw std::runtime_error("Error inserting data import to database");
            }

            co_return Error::Ok;
        } catch (std::exception &err) {
            logger.critical("Error downloading game files: {}", err.what());
            co_return Error::ErrInternal;
        }
    }
}
