#include "reports.h"

namespace service {
    // clang-format off
    DEFINE_ENUM(ReportType,
        {ReportType::PROJECT, "project"},
        {ReportType::DOCS, "docs"},
        {ReportType::CONTENT, "content"}
    )

    DEFINE_ENUM(ReportReason,
        {ReportReason::SPAM, "spam"},
        {ReportReason::COPYRIGHT, "copyright"},
        {ReportReason::CONTENT_RULES, "content_rules"},
        {ReportReason::TOS, "tos"}
    )

    DEFINE_ENUM(ReportStatus,
        {ReportStatus::NEW, "new"},
        {ReportStatus::DISMISSED, "dismissed"},
        {ReportStatus::ACCEPTED, "accepted"}
    )
    // clang-format on
}