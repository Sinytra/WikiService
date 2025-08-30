#pragma once

#include "ingestor_metadata.h"

#include <drogon/utils/coroutine.h>
#include <service/error.h>
#include <service/resolved.h>
#include <service/storage/issues.h>
#include <service/content/recipe/recipe_parser.h>

namespace content {
    struct PreparationResult {
        std::set<std::string> items;
    };

    template<typename T>
    struct PreparedData {
        T data;
        service::ProjectFileIssueCallback issues;
    };

    class SubIngestor {
    public:
        explicit SubIngestor(const service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &, service::ProjectFileIssueCallback &);
        virtual ~SubIngestor() = default;

        virtual drogon::Task<PreparationResult> prepare() = 0;
        virtual drogon::Task<service::Error> execute() = 0;
        virtual drogon::Task<service::Error> finish();

    protected:
        const service::ResolvedProject &project_;
        const std::shared_ptr<spdlog::logger> &logger_;
        service::ProjectFileIssueCallback &issues_;
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
                                         service::ProjectFileIssueCallback &);

        drogon::Task<PreparationResult> prepare() override;
        drogon::Task<service::Error> execute() override;

    private:
        std::unordered_map<std::string, std::string> pagePaths_;
    };

    class TagsSubIngestor final : public SubIngestor {
    public:
        explicit TagsSubIngestor(const service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &,
                                 service::ProjectFileIssueCallback &);

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
                                    service::ProjectFileIssueCallback &);

        drogon::Task<PreparationResult> prepare() override;
        drogon::Task<service::Error> execute() override;

    private:
        std::optional<PreparedData<StubRecipeType>> readRecipeType(const std::string &namespace_, const std::filesystem::path &root,
                                                                   const std::filesystem::path &path) const;
        drogon::Task<service::Error> addRecipeType(PreparedData<StubRecipeType> type) const;

        std::optional<PreparedData<StubRecipe>> readRecipe(const std::string &namespace_, const std::filesystem::path &root,
                                                           const std::filesystem::path &path) const;
        drogon::Task<service::Error> addRecipe(PreparedData<StubRecipe> recipe) const;

        std::vector<PreparedData<StubRecipeType>> recipeTypes_;
        std::vector<PreparedData<StubRecipe>> recipes_;
    };

    class MetadataSubIngestor final : public SubIngestor {
    public:
        explicit MetadataSubIngestor(const service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &,
                                     service::ProjectFileIssueCallback &);

        drogon::Task<PreparationResult> prepare() override;
        drogon::Task<service::Error> execute() override;
    private:
        drogon::Task<service::Error> addWorkbenches(PreparedData<StubWorkbenches> workbenches) const;

        std::vector<PreparedData<StubWorkbenches>> workbenches_;
    };
}
