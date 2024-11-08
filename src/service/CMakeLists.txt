add_library(service STATIC
    service_impl.cc
    github.cc
    util.cc
    cache.cc
    database.cc
    documentation.cc
)

target_link_libraries(service PRIVATE
    log
    drogon
    jwt-cpp
    base64
)

target_include_directories(service PUBLIC
        ${PROJECT_SOURCE_DIR}
)
