#pragma once

#include <string>

namespace service {
    enum class ProjectIssueLevel { WARNING, ERROR, UNKNOWN };
    std::string enumToStr(ProjectIssueLevel status);
}
