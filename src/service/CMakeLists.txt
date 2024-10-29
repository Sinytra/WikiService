add_library(service STATIC
    service_impl.cc
)

target_link_libraries(service PRIVATE
    log
)

target_include_directories(service PUBLIC
        ${PROJECT_SOURCE_DIR}
)
