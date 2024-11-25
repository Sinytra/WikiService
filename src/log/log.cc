#include "log.h"

#include <drogon/drogon.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace logging {
    spdlog::logger &logger([]() -> spdlog::logger & {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        sinks.push_back(std::make_shared<spdlog::sinks::daily_file_sink_mt>("logs/logfile", 23, 59));
        const auto combined_logger = std::make_shared<spdlog::logger>("main", begin(sinks), end(sinks));
        combined_logger->set_pattern("[%^%L%$] [%H:%M:%S %z] [thread %t] %v");
        register_logger(combined_logger);
        return *combined_logger;
    }());

    void configureLoggingLevel() {
        switch (trantor::Logger::logLevel()) {
            case trantor::Logger::LogLevel::kTrace:
                spdlog::set_level(spdlog::level::trace);
                break;
            case trantor::Logger::LogLevel::kDebug:
                spdlog::set_level(spdlog::level::debug);
                break;
            case trantor::Logger::LogLevel::kInfo:
                spdlog::set_level(spdlog::level::info);
                break;
            case trantor::Logger::LogLevel::kWarn:
                spdlog::set_level(spdlog::level::warn);
                break;
            case trantor::Logger::LogLevel::kError:
                spdlog::set_level(spdlog::level::err);
                break;
            case trantor::Logger::LogLevel::kFatal:
                spdlog::set_level(spdlog::level::critical);
                break;
            default:;
        }
    }

}
