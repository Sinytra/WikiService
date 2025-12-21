#pragma once

#include <nlohmann/json_fwd.hpp>

namespace schemas {
    extern nlohmann::json getBulkProjects;

    extern nlohmann::json systemConfig;
    extern nlohmann::json projectRegister;
    extern nlohmann::json projectUpdate;
    extern nlohmann::json projectMetadata;
    extern nlohmann::json folderMetadata;
    extern nlohmann::json projectIssue;
    extern nlohmann::json report;
    extern nlohmann::json ruleReport;
    extern nlohmann::json accessKey;

    extern nlohmann::json addProjectMember;
    extern nlohmann::json removeProjectMember;

    extern nlohmann::json gameRecipe;
    extern nlohmann::json gameRecipeType;
    extern nlohmann::json gameRecipeBase;
    extern nlohmann::json gameRecipeCustom;
    extern nlohmann::json gameTag;
    extern nlohmann::json recipeWorkbenches;
    extern nlohmann::json properties;
}
