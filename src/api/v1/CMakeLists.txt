add_library(api STATIC
        browse.cc
        docs.cc
        error.cc
)

target_link_libraries(api PRIVATE
        service
        log
        drogon
)
