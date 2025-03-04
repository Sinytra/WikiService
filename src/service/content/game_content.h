#pragma once

#include "ingestor_recipes.h"
#include <drogon/utils/coroutine.h>
#include <service/error.h>
#include <service/resolved.h>

namespace content {
    struct PreparationResult {
        std::set<std::string> items;
    };

    class SubIngestor {
    public:
        explicit SubIngestor(const service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &);
        virtual ~SubIngestor() = default;

        virtual drogon::Task<PreparationResult> prepare() = 0;
        virtual drogon::Task<service::Error> execute() = 0;
        virtual drogon::Task<service::Error> finish();

    protected:
        const service::ResolvedProject &project_;
        const std::shared_ptr<spdlog::logger> &logger_;
    };

    class Ingestor {
    public:
        explicit Ingestor(service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &);

        drogon::Task<service::Error> ingestGameContentData() const;
    private:
        const service::ResolvedProject &project_;
        const std::shared_ptr<spdlog::logger> &logger_;
    };

    class ContentPathsSubIngestor final : public SubIngestor {
    public:
        explicit ContentPathsSubIngestor(const service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &);

        drogon::Task<PreparationResult> prepare() override;
        drogon::Task<service::Error> execute() override;
    private:
        std::unordered_map<std::string, std::string> pagePaths_;
    };

    class TagsSubIngestor final : public SubIngestor {
    public:
        explicit TagsSubIngestor(const service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &);

        drogon::Task<PreparationResult> prepare() override;
        drogon::Task<service::Error> execute() override;
        drogon::Task<service::Error> finish() override;
    private:
        std::set<std::string> tagIds_;
        std::unordered_map<std::string, std::set<std::string>> tagEntries_;
    };

    class RecipesSubIngestor final : public SubIngestor {
    public:
        explicit RecipesSubIngestor(const service::ResolvedProject &, const std::shared_ptr<spdlog::logger> &);

        drogon::Task<PreparationResult> prepare() override;
        drogon::Task<service::Error> execute() override;
    private:
        drogon::Task<service::Error> addRecipe(ModRecipe recipe) const;

        std::vector<ModRecipe> recipes_;
    };
}
