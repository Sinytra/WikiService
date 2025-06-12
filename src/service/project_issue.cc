#include "project_issue.h"

namespace service {
    std::string enumToStr(const ProjectIssueLevel status) {
        switch (status) {
            case ProjectIssueLevel::WARNING:
                return "warning";
            case ProjectIssueLevel::ERROR:
                return "error";
            default:
                return "unknown";
        }
    }
}
