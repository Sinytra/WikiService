#include "game_data.h"
#include <fstream>
#include <include/uri.h>
#include <nlohmann/json.hpp>
#include <service/util.h>
#include <zip.h>

#define LAUNCHER_MANIFEST_URL "https://launchermeta.mojang.com/mc/game/version_manifest_v2.json"
#define RESOURCES_URL "https://resources.download.minecraft.net"

using namespace drogon;
using namespace logging;
using namespace std::chrono;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace service {
    GameDataService::GameDataService(const std::string &storagePath) : baseDir_(fs::path(storagePath) / ".game") {
        if (!exists(baseDir_)) {
            fs::create_directories(baseDir_);
        }
    }

    Task<> GameDataService::setupGameData() const {
        const auto langDir = baseDir_ / "lang";
        if (exists(langDir)) {
            co_return;
        }

        try {
            co_await downloadAssets(langDir);
            logger.info("Game data setup complete");
        } catch (std::exception err) {
            logger.critical("Error downloading game assets: {}", err.what());
            remove_all(langDir);
            throw;
        }
    }

    std::optional<nlohmann::json> GameDataService::getLang(const std::string &name) const {
        const auto path = baseDir_ / "lang" / (name + ".json");
        return parseJsonFile(path);
    }

    Task<> GameDataService::downloadAssets(const fs::path langDir) const {
        static auto langFilePrefix = "minecraft/lang/";

        logger.info("Downloading game assets");

        if (exists(langDir)) {
            remove_all(langDir);
        }
        fs::create_directories(langDir);

        // Step 1: Download launcher manifest
        logger.debug("Fetching launcher manifest");
        const auto launcherManifest = co_await bufferJsonFile(LAUNCHER_MANIFEST_URL);
        // Step 2: Find and download version manifest
        const std::string latestRelease = launcherManifest["latest"]["release"];
        logger.debug("Found latest release {}", latestRelease);
        int count = 0;
        for (auto version: launcherManifest["versions"]) {
            if (version["id"] == latestRelease) {
                const auto versionUrl = version["url"];
                logger.debug("Fetching version manifest");
                const auto versionManifest = co_await bufferJsonFile(versionUrl);

                // Step 4: Download asset index
                const auto assetIndexUrl = versionManifest["assetIndex"]["url"];
                logger.debug("Fetching asset index");
                const auto assetIndex = co_await bufferJsonFile(assetIndexUrl);

                auto start = high_resolution_clock::now();
                for (const auto &[key, object]: assetIndex["objects"].items()) {
                    if (key.starts_with(langFilePrefix)) {
                        const auto fileName = key.substr(strlen(langFilePrefix));
                        const std::string hash = object["hash"];
                        const auto prefix = hash.substr(0, 2);
                        const auto resourceUrl = std::format("{}/{}/{}", RESOURCES_URL, prefix, hash);
                        const auto downloadPath = langDir / fileName;

                        co_await downloadFile(resourceUrl, downloadPath);
                        logger.debug("Downloaded language file {}", fileName);
                        count++;
                    }
                }

                auto stop = high_resolution_clock::now();
                auto duration = duration_cast<milliseconds>(stop - start);
                logger.info("Successfully downloaded {} game assets in {} ms.", count, duration.count());

                logger.info("Extracting client data");
                const auto clientUrl = versionManifest["downloads"]["client"]["url"];
                const auto clientDest = baseDir_ / "client.jar";
                remove(clientDest);
                co_await downloadFile(clientUrl, clientDest);
                extractClient(clientDest, langDir);
                remove(clientDest);

                logger.debug("Done extracting client data");

                co_return;
            }
        }
        throw std::runtime_error("Could not find version manifest for version " + latestRelease);
    }

    void GameDataService::extractClient(const std::filesystem::path &clientPath, const std::filesystem::path langDir) const {
        static const auto mainLangFileName = "assets/minecraft/lang/en_us.json";

        zip_t *archive;
        int err;
        if ((archive = zip_open(absolute(clientPath).c_str(), 0, &err)) == nullptr) {
            throw std::runtime_error("Cannot open zip file");
        }

        const auto num_entries = zip_get_num_entries(archive, 0);
        for (zip_uint64_t i = 0; i < num_entries; ++i) {
            if (const char *name = zip_get_name(archive, i, 0); name && std::string(name) == mainLangFileName) {
                struct zip_stat st;
                zip_stat_init(&st);
                zip_stat(archive, name, 0, &st);
                auto contents = new char[st.size];
                zip_file *f = zip_fopen(archive, name, 0);
                zip_fread(f, contents, st.size);
                zip_fclose(f);

                const auto dest = langDir / "en_us.json";
                std::ofstream file(dest);
                if (!file) {
                    throw std::runtime_error("Failed to open output stream to " + dest.string());
                }
                file << contents;
                file.close();

                break;
            }
        }

        zip_close(archive);
    }

    Task<nlohmann::json> GameDataService::bufferJsonFile(const std::string url) const {
        const auto client = createHttpClient(url);
        const uri parsed(url);

        const auto httpReq = HttpRequest::newHttpRequest();
        httpReq->setMethod(Get);
        httpReq->setPath("/" + parsed.get_path());

        logger.debug("Downloading JSON file: {}", url);

        const auto response = co_await client->sendRequestCoro(httpReq);

        if (response->getStatusCode() != k200OK) {
            throw std::runtime_error("Unexpected response code: " + std::to_string(response->getStatusCode()));
        }

        if (const auto json = response->getJsonObject()) {
            co_return parkourJson(*json);
        }

        throw std::runtime_error("Expected response body to be JSON");
    }

    Task<> GameDataService::downloadFile(const std::string url, const std::filesystem::path dest) const {
        const auto client = createHttpClient(url);
        const uri parsed(url);

        const auto httpReq = HttpRequest::newHttpRequest();
        httpReq->setMethod(Get);
        httpReq->setPath("/" + parsed.get_path());

        logger.debug("Downloading file: {}", url);

        const auto response = co_await client->sendRequestCoro(httpReq);
        const auto body = response->getBody();

        std::ofstream file(dest);
        if (!file) {
            throw std::runtime_error("Failed to open output stream to " + dest.string());
        }
        file << body;
        file.close();

        co_return;
    }
}
