add_library(api STATIC
        authors/projects.cc
        authors/websocket.cc

        projects/docs.cc
        projects/game.cc

        auth.cc
        base.cc
        browse.cc
        error.cc
        moderation.cc
        system.cc
)

target_link_libraries(api PRIVATE
        service
        log
        Drogon::Drogon
        nlohmann_json_schema_validator
        libgit2
)
