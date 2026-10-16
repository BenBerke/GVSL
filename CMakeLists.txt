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

# Console example: basic addition / multiplication through GVSL (no SDL/imgui needed)
add_executable(GVSL_MathExample sandbox/math_main.cpp)
target_link_libraries(GVSL_MathExample PRIVATE GVSL)
set_target_properties(GVSL PROPERTIES LINKER_LANGUAGE CXX)
