cmake_minimum_required(VERSION 3.10)

project(wiki CXX)

set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/modules)

include(Deps)

add_subdirectory(src)
