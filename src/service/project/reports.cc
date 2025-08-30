#include "reports.h"

namespace service {
    // clang-format off
    ENUM_FROM_TO_STR(ReportType,
        {ReportType::PROJECT, "project"},
        {ReportType::DOCS, "docs"},
        {ReportType::CONTENT, "content"}
    )

    ENUM_FROM_TO_STR(ReportReason,
        {ReportReason::SPAM, "spam"},
        {ReportReason::COPYRIGHT, "copyright"},
        {ReportReason::CONTENT_RULES, "content_rules"},
        {ReportReason::TOS, "tos"}
    )

    ENUM_TO_STR(ReportStatus,
        {ReportStatus::NEW, "new"},
        {ReportStatus::DISMISSED, "dismissed"},
        {ReportStatus::ACCEPTED, "accepted"}
    )
    // clang-format on
}