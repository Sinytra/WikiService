#include "moderation.h"

#include <log/log.h>
#include <service/auth.h>
#include <service/serializers.h>
#include <service/storage/storage.h>
#include <service/util.h>

#include "base.h"
#include "reports.h"
#include "schemas.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    Task<> ModerationController::reports(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        co_await global::auth->ensurePrivilegedAccess(req);

        const auto params = getTableQueryParams(req);
        const auto reports(co_await global::database->getReports(params.page));
        callback(jsonResponse(reports));
    }

    Task<> ModerationController::submitReport(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session(co_await global::auth->getSession(req));
        const auto json(BaseProjectController::validatedBody(req, schemas::report));

        const auto projectId = json["project_id"];
        const auto [proj, projErr] = co_await global::database->getProjectSource(projectId);
        if (!proj) {
            throw ApiException(Error::ErrNotFound, "invalid_project");
        }

        const auto type = parseReportType(json["type"]);
        if (type == ReportType::UNKNOWN) {
            throw ApiException(Error::ErrBadRequest, "invalid_type");
        }

        const auto reason = parseReportReason(json["reason"]);
        if (reason == ReportReason::UNKNOWN) {
            throw ApiException(Error::ErrBadRequest, "invalid_reason");
        }

        Report report;

        if (json.contains("version") || json.contains("locale")) {
            const auto version = json.contains("version") ? std::make_optional(json["version"].get<std::string>()) : std::nullopt;
            const auto locale = json.contains("locale") ? std::make_optional(json["locale"].get<std::string>()) : std::nullopt;

            const auto [resolved, resErr](co_await global::storage->getProject(*proj, version, locale));
            if (!resolved) {
                throw ApiException(Error::ErrNotFound, "invalid_project");
            }

            report.setVersionId(resolved->getProjectVersion().getValueOfId());
            if (const auto projLocale = resolved->getLocale(); !projLocale.empty()) {
                report.setLocale(projLocale);
            }
        }

        const std::string body = json["body"];
        const std::string path = json["path"];

        report.setProjectId(projectId);
        report.setType(enumToStr(type));
        if (!path.empty())
            report.setPath(path);
        report.setSubmitterId(session.username);
        report.setStatus(enumToStr(ReportStatus::NEW));
        report.setReason(enumToStr(reason));
        report.setBody(body);

        if (const auto res = co_await global::database->addModel(report); !res) {
            logger.error("Failed to create report");
            throw ApiException(Error::ErrInternal, "internal");
        }

        callback(statusResponse(k201Created));
    }

    Task<> ModerationController::getReport(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                           const std::string id) const {
        co_await global::auth->ensurePrivilegedAccess(req);

        const auto report(co_await global::database->findByPrimaryKey<Report>(id));
        if (!report) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        nlohmann::json root(*report);
        if (const auto versionId = report->getVersionId()) {
            if (const auto version = co_await global::database->findByPrimaryKey<ProjectVersion>(*versionId); version && version->getName()) {
                root["version"] = version->getValueOfName();
            }
        }

        callback(jsonResponse(root));
    }

    Task<> ModerationController::ruleReport(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                            const std::string id) const {
        co_await global::auth->ensurePrivilegedAccess(req);
        const auto json(BaseProjectController::validatedBody(req, schemas::ruleReport));

        auto report(co_await global::database->findByPrimaryKey<Report>(id));
        if (!report) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        const auto resolution = json["resolution"];
        const auto status = resolution == "accept" ? ReportStatus::ACCEPTED : ReportStatus::DISMISSED;

        report->setStatus(enumToStr(status));
        if (const auto res = co_await global::database->updateModel(*report); !res) {
            throw ApiException(Error::ErrInternal, "internal");
        }

        callback(jsonResponse(*report));
    }
}
