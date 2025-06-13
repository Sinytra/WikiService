#pragma once

#include <drogon/utils/coroutine.h>
#include <service/error.h>
#include <service/resolved.h>
#include <service/project_issue.h>
#include "ingestor_recipes.h"

namespace content {
    struct PreparationResult {
        std::set<std::string> items;
    };

    class SubIngestor {
    public:
        explicit SubIngestor(const service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &,
                             service::ProjectIssueCallback &);
        virtual ~SubIngestor() = default;

        virtual drogon::Task<PreparationResult> prepare() = 0;
        virtual drogon::Task<service::Error> execute() = 0;
        virtual drogon::Task<service::Error> finish();

    protected:
        drogon::Task<> addIssue(service::ProjectIssueLevel level, service::ProjectIssueType type, service::ProjectError subject,
                                const std::string &details = "", const std::string &file = "") const;

        void addIssueAsync(service::ProjectIssueLevel level, service::ProjectIssueType type, service::ProjectError subject,
                           const std::string &details = "", const std::string &file = "") const;

        const service::ResolvedProject &project_;
        const std::shared_ptr<spdlog::logger> &logger_;
        service::ProjectIssueCallback &issues_;
    };

    class Ingestor {
    public:
        explicit Ingestor(service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &, service::ProjectIssueCallback &);

        drogon::Task<service::Error> runIngestor() const;

    private:
        drogon::Task<service::Error> ingestGameContentData() const;

        const service::ResolvedProject &project_;
        const std::shared_ptr<spdlog::logger> &logger_;
        service::ProjectIssueCallback &issues_;
    };

    class ContentPathsSubIngestor final : public SubIngestor {
    public:
        explicit ContentPathsSubIngestor(const service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &,
                                         service::ProjectIssueCallback &);

        drogon::Task<PreparationResult> prepare() override;
        drogon::Task<service::Error> execute() override;

    private:
        std::unordered_map<std::string, std::string> pagePaths_;
    };

    class TagsSubIngestor final : public SubIngestor {
    public:
        explicit TagsSubIngestor(const service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &,
                                 service::ProjectIssueCallback &);

        drogon::Task<PreparationResult> prepare() override;
        drogon::Task<service::Error> execute() override;
        drogon::Task<service::Error> finish() override;

    private:
        std::set<std::string> tagIds_;
        std::unordered_map<std::string, std::set<std::string>> tagEntries_;
    };

    class RecipesSubIngestor final : public SubIngestor {
    public:
        explicit RecipesSubIngestor(const service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &,
                                    service::ProjectIssueCallback &);

        drogon::Task<PreparationResult> prepare() override;
        drogon::Task<service::Error> execute() override;

    private:
        std::optional<ModRecipe> readRecipe(const std::string &namespace_, const std::filesystem::path &root,
                                            const std::filesystem::path &path) const;
        drogon::Task<service::Error> addRecipe(ModRecipe recipe) const;

        std::vector<ModRecipe> recipes_;
    };
}
