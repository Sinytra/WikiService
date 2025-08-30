#pragma once

#include <service/util.h>

namespace service {
    // clang-format off
    enum class ReportType {
        PROJECT,
        DOCS,
        CONTENT,
        UNKNOWN
    };
    // clang-format on
    std::string enumToStr(ReportType type);
    ReportType parseReportType(const std::string &type);

    // clang-format off
    enum class ReportReason {
        SPAM,
        COPYRIGHT,
        CONTENT_RULES,
        TOS,
        UNKNOWN
    };
    // clang-format on
    std::string enumToStr(ReportReason reason);
    ReportReason parseReportReason(const std::string &reason);

    enum class ReportStatus { NEW, DISMISSED, ACCEPTED, UNKNOWN };
    std::string enumToStr(ReportStatus status);
}
