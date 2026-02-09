#pragma once

#include "issues.h"
#include <cache.h>
#include <service/project/project.h>

namespace service {
    class IssueService : public CacheableServiceBase {
    public:
        explicit IssueService();

        ComputableTask<> addProjectIssueExternal(ProjectBasePtr project, ProjectIssueLevel level, ProjectIssueType type,
                                                           ProjectError subject, std::string details, std::string path);

        ComputableTask<> addProjectIssueInternal(std::string deploymentId, ProjectIssueLevel level, ProjectIssueType type,
                                                           ProjectError subject, std::string details, std::string file) const;
    };
}

namespace global {
    extern std::shared_ptr<service::IssueService> issues;
}