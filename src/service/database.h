#pragma once

#include "error.h"
#include "models/Project.h"

#include <drogon/utils/coroutine.h>
#include <optional>
#include <string>

using namespace drogon_model::postgres;

namespace service {
    struct ProjectSearchResponse {
        int pages;
        int total;
        std::vector<Project> data;
    };

    class Database {
    public:
        explicit Database();

        drogon::Task<std::vector<std::string>> getProjectIDs() const;

        drogon::Task<std::tuple<std::optional<Project>, Error>> createProject(const Project &project) const;

        drogon::Task<Error> updateProject(const Project &project) const;

        drogon::Task<Error> removeProject(const std::string &id) const;

        drogon::Task<Error> updateRepository(const std::string &repo, const std::string &newRepo) const;

        drogon::Task<std::tuple<std::optional<Project>, Error>> getProjectSource(std::string id) const;

        drogon::Task<std::vector<Project>> getProjectsForRepos(std::vector<std::string> repos) const;

        drogon::Task<std::tuple<ProjectSearchResponse, Error>> findProjects(std::string query, std::string types, int page) const;

        drogon::Task<bool> existsForRepo(std::string repo, std::string branch, std::string path) const;

        drogon::Task<bool> existsForData(std::string id, std::string platform, std::string slug) const;
    };
}
