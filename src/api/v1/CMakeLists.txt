add_library(api STATIC
    controller.cc
    docs.cc
    error.cc
)

target_link_libraries(api PRIVATE
    service
    log
    drogon
)
