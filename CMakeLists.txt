cmake_minimum_required(VERSION 3.5)

project(recipes CXX)

set(CMAKE_POLICY_DEFAULT_CMP0169 OLD)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/modules)

include(deps)

add_subdirectory(src)
