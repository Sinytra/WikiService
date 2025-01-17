add_library(api STATIC
        browse.cc
        docs.cc
        error.cc
        projects.cc
        websocket.cc
)

target_link_libraries(api PRIVATE
        service
        log
        Drogon::Drogon
        nlohmann_json_schema_validator
        libgit2
)
