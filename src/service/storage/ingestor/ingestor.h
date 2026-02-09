#pragma once

#include "ingestor_metadata.h"

#include <drogon/utils/coroutine.h>
#include <service/error.h>
#include <service/project/resolved.h>
#include <service/storage/ingestor/recipe/recipe_parser.h>
#include <service/storage/issues/issue_callback.h>

#define INGESTOR_CONTENT_PATHS "Content paths"
#define INGESTOR_TAGS "Tags"
#define INGESTOR_RECIPES "Recipes"
#define INGESTOR_METADATA "Metadata"

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
        explicit SubIngestor(const service::ProjectBase &, const std::shared_ptr<spdlog::logger> &, service::ProjectFileIssueCallback &);
        virtual ~SubIngestor() = default;

        virtual drogon::Task<PreparationResult> prepare() = 0;
        virtual drogon::Task<service::Error> execute() = 0;
        virtual drogon::Task<service::Error> finish();

    protected:
        const service::ProjectBase &project_;
        const std::shared_ptr<spdlog::logger> &logger_;
        service::ProjectFileIssueCallback &issues_;
    };

    class Ingestor {
    public:
        explicit Ingestor(service::ProjectBase &, const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<service::ProjectIssueCallback> &,
                          const std::set<std::string> &enableModules, bool deleteExisting);

        drogon::Task<service::Error> runIngestor() const;

    private:
        drogon::Task<service::Error> ingestGameContentData() const;
        bool enableModule(const std::string &name) const;

        template<typename T>
        void addSubModule(std::vector<std::pair<std::string, std::unique_ptr<SubIngestor>>> &ingestors, const std::string &name,
                          service::ProjectFileIssueCallback &issues) const {
            if (enableModule(name)) {
                ingestors.emplace_back(name, std::make_unique<T>(project_, logger_, issues));
            }
        }

        const service::ProjectBase &project_;
        const std::shared_ptr<spdlog::logger> &logger_;
        std::shared_ptr<service::ProjectIssueCallback> issues_;
        const std::set<std::string> enableModules_;
        const bool deleteExisting_;
    };

    class ContentPathsSubIngestor final : public SubIngestor {
    public:
        explicit ContentPathsSubIngestor(const service::ProjectBase &, const std::shared_ptr<spdlog::logger> &,
                                         service::ProjectFileIssueCallback &);

        drogon::Task<PreparationResult> prepare() override;
        drogon::Task<service::Error> execute() override;

    private:
        std::unordered_map<std::string, std::string> pagePaths_;
    };

    class TagsSubIngestor final : public SubIngestor {
    public:
        explicit TagsSubIngestor(const service::ProjectBase &, const std::shared_ptr<spdlog::logger> &,
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
        explicit RecipesSubIngestor(const service::ProjectBase &, const std::shared_ptr<spdlog::logger> &,
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
        explicit MetadataSubIngestor(const service::ProjectBase &, const std::shared_ptr<spdlog::logger> &,
                                     service::ProjectFileIssueCallback &);

        drogon::Task<PreparationResult> prepare() override;
        drogon::Task<service::Error> execute() override;

    private:
        drogon::Task<service::Error> addWorkbenches(PreparedData<StubWorkbenches> workbenches) const;

        std::vector<PreparedData<StubWorkbenches>> workbenches_;
    };
}
