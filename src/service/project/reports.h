#pragma once

#include <service/util.h>

namespace service {
    enum class ReportType { PROJECT, DOCS, CONTENT, UNKNOWN };
    DECLARE_ENUM(ReportType);

    enum class ReportReason { SPAM, COPYRIGHT, CONTENT_RULES, TOS, UNKNOWN };
    DECLARE_ENUM(ReportReason);

    enum class ReportStatus { NEW, DISMISSED, ACCEPTED, UNKNOWN };
    DECLARE_ENUM(ReportStatus);
}
