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

        drogon::Task<std::tuple<std::optional<Project>, Error>> getProjectSource(std::string id);

        drogon::Task<std::tuple<ProjectSearchResponse, Error>> findProjects(std::string query, int page);
    };
}
