#include "project.h"

using namespace drogon;

namespace service {
    // clang-format off
    DEFINE_ENUM(ProjectVisibility,
        {ProjectVisibility::PUBLIC, "public"},
        {ProjectVisibility::UNLISTED, "unlisted"},
        {ProjectVisibility::PRIVATE, "private"}
    );

    DEFINE_ENUM(ProjectFlag,
        {ProjectFlag::UNPUBLISHED, "unpublished"}
    );
    // clang-format on

    nlohmann::json parseFlags(const Project& project) {
        const auto flags(project.getFlags());
        if (!flags) {
            return {};
        }
        return tryParseJson(*flags).value_or(nlohmann::json{});
    }

    Task<> setFlag(Project project, const ProjectFlag flag) {
        auto flags(parseFlags(project));
        const auto str(enumToStr(flag));

        if (!flags.contains(str)) {
            flags += str;
            if (flags.empty()) {
                project.setFlagsToNull();
            } else {
                project.setFlags(flags.dump());
            }
            co_await global::database->updateModel(project);
        }
    }

    Task<TaskResult<>> removeFlag(Project project, const ProjectFlag flag) {
        auto flags(parseFlags(project));

        const auto it = std::find(flags.begin(), flags.end(), enumToStr(flag));
        if (it != flags.end()) {
            flags.erase(it);
            if (flags.empty()) {
                project.setFlagsToNull();
            } else {
                project.setFlags(flags.dump());
            }
            co_return co_await global::database->updateModel(project);
        }

        co_return Error::Ok;
    }

    bool hasFlag(const Project &project, const ProjectFlag flag) {
        const auto flags(parseFlags(project));
        return flags.contains(enumToStr(flag));
    }
}
