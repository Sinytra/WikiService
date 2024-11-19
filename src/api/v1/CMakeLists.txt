add_library(api STATIC
        browse.cc
        docs.cc
        error.cc
        projects.cc
)

target_link_libraries(api PRIVATE
        service
        log
        drogon
        nlohmann_json_schema_validator
)
