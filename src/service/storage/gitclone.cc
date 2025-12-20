#include "gitops.h"

#include <git2.h>
#include <include/uri.h>
#include <process.hpp>

// 500 MB
#define MAX_REPO_SIZE_BYTES 500 * 1024 * 1024
#define CODE_SIZE_EXCEEDED -99

using namespace logging;
using namespace service;
using namespace drogon;
using namespace TinyProcessLib;
namespace fs = std::filesystem;

namespace git {
    bool is_local_url(const std::string &str) {
        try {
            const uri url{str};
            const auto scheme = url.get_scheme();
            return scheme == "file";
        } catch (...) {
            return false;
        }
    }

    uintmax_t getDirSize(const fs::path &dir) {
        if (!fs::exists(dir)) {
            return 0;
        }

        uintmax_t size = 0;
        try {
            for (const auto &entry: fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
                if (fs::is_regular_file(entry)) {
                    size += entry.file_size();
                }
            }
        } catch (...) {
            // Ignore error
        }

        return size;
    }

    void processLogLines(const std::string &chunk, const std::string &logProgram, const std::shared_ptr<spdlog::logger> &logger) {
        size_t pos;
        std::string logBuffer = chunk;
        // Find the next newline
        while ((pos = logBuffer.find_first_of("\r\n")) != std::string::npos) {
            // Extract line
            std::string line = logBuffer.substr(0, pos);

            // Check if it's a CRLF (\r\n) so we don't leave a dangling \n
            size_t eraseLength = 1;
            if (logBuffer[pos] == '\r' && pos + 1 < logBuffer.size() && logBuffer[pos + 1] == '\n') {
                eraseLength = 2;
            }

            // Log the clean line
            if (!line.empty()) {
                logger->debug("{}: {}", logProgram, line);
            }

            // Remove line from buffer
            logBuffer.erase(0, pos + eraseLength);
        }
    }

    // TODO Dedicated thread pool for cloning
    std::pair<int, std::string> executeCloneCmd(const std::string &command, const fs::path &destPath, const std::string &logProgram,
                                                const std::shared_ptr<spdlog::logger> &logger) {
        std::string result;
        std::mutex mutex;
        std::atomic finished{false};
        std::atomic sizeLimitExceeded{false};

        auto capture = [&](const char *bytes, const size_t n) {
            std::lock_guard lock(mutex);

            const std::string chunk(bytes, n);
            result += chunk;

            processLogLines(chunk, logProgram, logger);
        };

        Process::environment_type env{
            {"LC_ALL", "C"}, // Output messages in english
            {"GIT_TERMINAL_PROMPT", "0"} // Dont ask for credentials
        };
        Process process(command, "", env, capture, capture);

        std::thread watchdog([&]() {
            while (!finished) {
                const auto currentSize = getDirSize(destPath);

                if (currentSize > MAX_REPO_SIZE_BYTES) {
                    sizeLimitExceeded = true;
                    process.kill();
                    break;
                }

                // Sleep for 500ms before checking again
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });

        int status = process.get_exit_status();

        finished = true;
        if (watchdog.joinable()) {
            watchdog.join();
        }

        if (sizeLimitExceeded) {
            return {CODE_SIZE_EXCEEDED, result};
        }

        return {status, result};
    }

    ProjectErrorInstance runGitClone(const std::string &url, const fs::path &path, const std::string &branch, const bool shallow,
                                     const std::shared_ptr<spdlog::logger> &logger) {
        std::string cmd = "git clone --progress ";
        if (shallow) {
            cmd += "--depth 1 --no-single-branch ";
        }
        if (!branch.empty()) {
            cmd += "--branch " + branch + " ";
        }
        cmd += "\"" + url + "\" ";
        cmd += "\"" + path.string() + "\"";

        try {
            auto [exitCode, output] = executeCloneCmd(cmd, path, "git", logger);
            if (exitCode == 0) {
                return {ProjectError::OK};
            }

            if (exitCode == CODE_SIZE_EXCEEDED) {
                return {ProjectError::REPO_TOO_LARGE, "Repository exceeded size limit (500MB)."};
            }

            // Invalid URL / private repo
            if (contains(output, "Repository not found")) {
                return {ProjectError::NO_REPOSITORY};
            }

            // Authentication
            if (contains(output, "Authentication failed") || contains(output, "could not read Username") || contains(output, "HTTP 401") ||
                contains(output, "HTTP 403") || contains(output, "terminal prompts disabled"))
            {
                return {ProjectError::REQUIRES_AUTH, "Authentication required."};
            }

            // Branch
            if (contains(output, "Remote branch") || contains(output, "pathspec") || contains(output, "not found in upstream")) {
                return {ProjectError::NO_BRANCH, "Requested branch not found."};
            }

            // Size / Network
            if (contains(output, "RPC failed") || contains(output, "pack exceeds maximum") || contains(output, "early EOF") ||
                contains(output, "out of memory"))
            {
                return {ProjectError::REPO_TOO_LARGE, "Repository clone failed (network)."};
            }

            // Unknown
            std::ranges::replace(output, '\n', ' ');
            return {ProjectError::UNKNOWN};
        } catch (const std::exception &err) {
            logger->error("Error cloning repository: {}", err.what());
            return {ProjectError::UNKNOWN};
        }
    }

    Task<std::tuple<git_repository *, ProjectErrorInstance>> cloneRepository(const std::string url, const fs::path projectPath,
                                                                             const std::string branch,
                                                                             const std::shared_ptr<spdlog::logger> logger) {
        logger->info("Cloning git repository at {}", url);

        const auto path = absolute(projectPath);
        const auto shallow = !is_local_url(url);
        const auto result = runGitClone(url, path, branch, shallow, logger);

        if (result.error != ProjectError::OK) {
            logger->info("Git clone failed with error {}", result.message);
            co_return {nullptr, result};
        }

        logger->info("Git clone successful");

        git_repository *repo = nullptr;
        if (git_repository_open(&repo, absolute(path).c_str()) != 0) {
            logger->error("Failed to open repository: {}", path.string());
            co_return {nullptr, {ProjectError::UNKNOWN, "Failed to open repository"}};
        }

        co_return {repo, {ProjectError::OK}};
    }
}
