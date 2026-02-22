#include "project.h"

namespace service {
    ENUM_FROM_TO_STR(ProjectVisibility,
        {ProjectVisibility::PUBLIC, "public"},
        {ProjectVisibility::UNLISTED, "unlisted"},
        {ProjectVisibility::PRIVATE, "private"}
    )
    void to_json(nlohmann::json &j, const ProjectVisibility &obj) { j = enumToStr(obj); }
    void from_json(const nlohmann::json &j, ProjectVisibility &obj) { obj = parseProjectVisibility(j); }
}
