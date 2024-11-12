add_library(api STATIC
        docs.cc
        error.cc
)

target_link_libraries(api PRIVATE
        service
        log
        drogon
)
