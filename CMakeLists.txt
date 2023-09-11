cmake_minimum_required(VERSION 3.3.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(SimplePatchLoader VERSION 1.0.0)

file(GLOB SOURCES
    src/main.cpp
    src/sigscan.cpp
    src/dirs.cpp
    src/util.cpp
    src/patches.cpp
)

if (WIN32)
    file(GLOB PLATFORM_SOURCES
        src/mh.cpp
        src/pp.cpp # TODO: mac users dont have pp rn
    )
else ()
    file(GLOB PLATFORM_SOURCES
        src/mh_stub.cpp
    )
endif ()

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${PLATFORM_SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

CPMAddPackage("gh:matcool/gd-imgui-cocos#cb7290b")
target_link_libraries(${PROJECT_NAME} imgui-cocos)

if (WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE mh)
    target_link_libraries(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/mh/absolllute.megahack.lib)
    target_link_options(${PROJECT_NAME} PRIVATE /DELAYLOAD:absolllute.megahack.dll)
endif ()

setup_geode_mod(${PROJECT_NAME})
