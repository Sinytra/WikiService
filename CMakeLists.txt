cmake_minimum_required(VERSION 3.10)

project(wiki CXX)

set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)

option(USE_LOCAL_DEPS "Build dependencies locally" OFF)

if (MSVC)
    set(USE_LOCAL_DEPS ON)
    add_compile_definitions(NOMINMAX)
endif ()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/modules)

include(Deps)

add_subdirectory(src)
