cmake_minimum_required(VERSION 3.28...3.30)

# Development note:
# This project intentionally follows the official OBS plugin template layout.
# The helper CMake modules are obtained by scripts/setup-dev.ps1 from the
# official obs-plugintemplate repository before configuring the project.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(DeltaForceClipper VERSION 0.1.1)

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

find_package(Qt6 COMPONENTS Widgets Core REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    src/plugin-main.cpp
    src/clip_window.hpp
    src/clip_library.cpp
    src/clip_library.hpp
    src/settings_dialog.cpp
    src/settings_dialog.hpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Install rules for the OBS plugin
install(TARGETS ${CMAKE_PROJECT_NAME}
    LIBRARY DESTINATION "obs-plugins/64bit"
    RUNTIME DESTINATION "obs-plugins/64bit"
)

# Install data/locale
install(DIRECTORY data/ DESTINATION "data/obs-plugins/${_name}")

# Install Python detector tools
install(DIRECTORY tools/ DESTINATION "data/obs-plugins/${_name}/tools"
    PATTERN "__pycache__" EXCLUDE
    PATTERN "tests" EXCLUDE
)

# Offline unit tests for the clip library. Not part of the install tree;
# run with: ctest -C RelWithDebInfo
include(CTest)
if(BUILD_TESTING)
    add_executable(event_coalescer_tests src/test_event_coalescer.cpp)
    target_include_directories(event_coalescer_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(event_coalescer_tests PRIVATE cxx_std_17)
    add_test(NAME event_coalescer_tests COMMAND event_coalescer_tests)
endif()

find_package(Qt6 COMPONENTS Test QUIET)
if(Qt6Test_FOUND)
    add_executable(${CMAKE_PROJECT_NAME}_tests EXCLUDE_FROM_ALL
        src/test_clip_library.cpp
        src/clip_library.cpp
    )
    target_link_libraries(${CMAKE_PROJECT_NAME}_tests PRIVATE Qt6::Core Qt6::Test)
    target_compile_features(${CMAKE_PROJECT_NAME}_tests PRIVATE cxx_std_17)
    set_target_properties(${CMAKE_PROJECT_NAME}_tests PROPERTIES AUTOMOC ON)

    if(BUILD_TESTING)
        add_test(NAME ${CMAKE_PROJECT_NAME}_tests COMMAND ${CMAKE_PROJECT_NAME}_tests)
    endif()
endif()

# Package configuration (cpack)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "${_name}")
set(CPACK_PACKAGE_VERSION "${_version}")
set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)
