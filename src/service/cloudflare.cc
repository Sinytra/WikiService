#define CLOUDFLARE_API_URL "https://api.cloudflare.com/client/v4/graphql"

#include "cloudflare.h"
#include "util.h"

#include <chrono>
#include <global.h>

using namespace drogon;
using namespace std::chrono_literals;

namespace service {
    CloudFlare::CloudFlare(const config::CloudFlare &config) : config_(config) {}

    Task<std::vector<std::string>> CloudFlare::computeMostVisitedProjectIDs() const {
        const auto client = createHttpClient(CLOUDFLARE_API_URL);

        static constexpr int projectCount = 3;
        static constexpr int limit = 50;
        static constexpr auto query =
            R"({{
                viewer {{
                    accounts(filter: {{accountTag: "{}"}}) {{
                        topPaths: rumPageloadEventsAdaptiveGroups(
                            filter: {{
                                AND: [
                                    {{date_gt: "{}", requestPath_like: "/%/project/%/docs"}},
                                    {{siteTag: "{}"}}
                                ]
                            }},
                            limit: {},
                            orderBy: [sum_visits_DESC]
                        ) {{
                            count
                            sum {{
                                visits
                            }}
                            dimensions {{
                                metric: requestPath
                            }}
                        }}
                    }}
                }}
            }})";
        const auto today = std::chrono::system_clock::now();
        const auto thirtyDaysAgo = today - std::chrono::days(30);
        const auto date = std::format("{:%Y-%m-%d}", thirtyDaysAgo);
        const auto formatted = std::format(query, config_.accountTag, date, config_.siteTag, limit);
        Json::Value body;
        body["query"] = formatted;
        const auto bodyStr = serializeJsonString(body);

        std::vector<std::string> projects;

        if (const auto [data, err] = co_await sendAuthenticatedRequest(client, Post, "/client/v4/graphql", config_.token,
                                                                       [&bodyStr](const HttpRequestPtr &req) {
                                                                           req->setContentTypeCode(CT_APPLICATION_JSON);
                                                                           req->setBody(bodyStr);
                                                                       });
            data)
        {
            const auto accounts = (*data)["data"]["viewer"]["accounts"];
            const auto account = accounts.front();
            const auto topPaths = account["topPaths"];

            std::map<std::string, int> visits;
            for (const auto &top_path: topPaths) {
                const auto path = top_path["dimensions"]["metric"].asString();
                auto parts =
                    path.substr(1) | std::views::split('/') | std::views::transform([](auto r) { return std::string(r.data(), r.size()); });
                auto split = std::vector(parts.begin(), parts.end());

                auto project = split[2];
                auto visitCount = top_path["sum"]["visits"].asInt();
                visits[project] += visitCount;
            }

            std::vector<std::pair<std::string, int>> sortedAggregated(visits.begin(), visits.end());
            std::ranges::sort(sortedAggregated, [](const std::pair<std::string, int> &a, const std::pair<std::string, int> &b) {
                return a.second > b.second;
            });
            if (sortedAggregated.size() > projectCount) {
                sortedAggregated.resize(projectCount);
            }
            for (auto &key: sortedAggregated | std::views::keys) {
                projects.push_back(key);
            }
        }

        co_return projects;
    }

    Task<std::vector<std::string>> CloudFlare::getMostVisitedProjectIDs() {
        if (config_.token.empty()) {
            co_return std::vector<std::string>();
        }

        static const auto cacheKey = "popular_projects";

        if (const auto cached = co_await global::cache->getFromCache(cacheKey)) {
            if (const auto parsed = tryParseJson<nlohmann::ordered_json>(*cached)) {
                co_return parsed->get<std::vector<std::string>>();
            }
        }

        if (const auto pending = co_await getOrStartTask<std::vector<std::string>>(cacheKey)) {
            co_return co_await patientlyAwaitTaskResult(*pending);
        }

        auto result = co_await computeMostVisitedProjectIDs();
        const auto serialized = nlohmann::json(result).dump();

        co_await global::cache->updateCache(cacheKey, serialized, 7 * 24h);
        co_return co_await completeTask<std::vector<std::string>>(cacheKey, std::move(result));
    }
}
