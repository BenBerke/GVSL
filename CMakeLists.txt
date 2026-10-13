cmake_minimum_required(VERSION 4.1)
project(GVSL)

set(CMAKE_CXX_STANDARD 20)

add_library(GVSL STATIC
        lib/Objects.hpp)
target_include_directories(GVSL PUBLIC lib)

option(GVSL_BUILD_SANDBOX "Build the SDL3 + imgui-node-editor test app" ${PROJECT_IS_TOP_LEVEL})
if(GVSL_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()
