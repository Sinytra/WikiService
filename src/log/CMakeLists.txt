add_library(log STATIC
        log.cc
)

target_link_libraries(log PUBLIC
        spdlog::spdlog
        Drogon::Drogon
)

target_include_directories(log PUBLIC
        ${PROJECT_SOURCE_DIR}
)