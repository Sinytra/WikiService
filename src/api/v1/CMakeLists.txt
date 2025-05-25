add_library(api STATIC
        auth.cc
        base.cc
        browse.cc
        docs.cc
        game.cc
        error.cc
        projects.cc
        websocket.cc
        system.cc
)

target_link_libraries(api PRIVATE
        service
        log
        Drogon::Drogon
        nlohmann_json_schema_validator
        libgit2
)
