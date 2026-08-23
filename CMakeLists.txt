cmake_minimum_required(VERSION 3.16)
project(cjong4-opponent VERSION 1.0.0 LANGUAGES C)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CJ4_OPPONENT_BUILD_EXAMPLES "Build example programs" OFF)
option(CJ4_OPPONENT_BUILD_TESTS "Build tests" OFF)
option(CJ4_OPPONENT_USE_SYSTEM_CJONG4
    "Use an installed cjong4 package instead of the bundled submodule" OFF)

# Preserve the pre-release option names for top-level builds only. Prefixed
# options avoid collisions when this project is embedded with add_subdirectory.
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND DEFINED BUILD_EXAMPLES)
    message(DEPRECATION
        "BUILD_EXAMPLES is deprecated; use CJ4_OPPONENT_BUILD_EXAMPLES")
    set(CJ4_OPPONENT_BUILD_EXAMPLES "${BUILD_EXAMPLES}")
endif()
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND DEFINED BUILD_TESTS)
    message(DEPRECATION
        "BUILD_TESTS is deprecated; use CJ4_OPPONENT_BUILD_TESTS")
    set(CJ4_OPPONENT_BUILD_TESTS "${BUILD_TESTS}")
endif()

if (CJ4_OPPONENT_USE_SYSTEM_CJONG4)
    find_package(cjong4 3.1 CONFIG REQUIRED)
else()
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cjong4/CMakeLists.txt")
        message(FATAL_ERROR
            "The cjong4 submodule is missing. Run 'git submodule update --init "
            "--recursive' or configure with CJ4_OPPONENT_USE_SYSTEM_CJONG4=ON.")
    endif()

    # cjong4 has its own BUILD_TESTS option. Keep its tests out of this
    # project's build so CJ4_OPPONENT_BUILD_TESTS controls only this repo.
    set(_CJ4_OPPONENT_SAVED_BUILD_TESTS "${BUILD_TESTS}")
    set(BUILD_TESTS OFF)
    add_subdirectory(external/cjong4)
    set(BUILD_TESTS "${_CJ4_OPPONENT_SAVED_BUILD_TESTS}")
    unset(_CJ4_OPPONENT_SAVED_BUILD_TESTS)
endif()

add_library(cj4_opponents STATIC
    src/opponent_betaori.c
    src/opponent_chanta.c
    src/opponent_chiitoi.c
    src/opponent_kokushi.c
    src/opponent_pinfu.c
    src/opponent_somete.c
    src/opponent_tanyao.c
    src/opponent_toitoi.c
)
target_include_directories(cj4_opponents
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(cj4_opponents PUBLIC c_std_11)
target_link_libraries(cj4_opponents PUBLIC cjong4::cj4)
set_target_properties(cj4_opponents PROPERTIES EXPORT_NAME opponents)
add_library(cjong4::opponents ALIAS cj4_opponents)

if (MSVC)
    target_compile_options(cj4_opponents PRIVATE /W4)
else()
    target_compile_options(cj4_opponents PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (CJ4_OPPONENT_BUILD_EXAMPLES)
    add_executable(cj4_cli
        examples/cli/main.c
    )
    target_include_directories(cj4_cli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/cli
    )
    target_link_libraries(cj4_cli PRIVATE cj4_opponents)
endif()

if (CJ4_OPPONENT_BUILD_TESTS)
    enable_testing()

    add_executable(cj4_opponent_tests
        tests/test_opponents.c
    )
    target_link_libraries(cj4_opponent_tests PRIVATE cj4_opponents)

    add_executable(cj4_winning_results_tests
        tests/test_winning_results.c
        examples/cli/main.c
    )
    target_include_directories(cj4_winning_results_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/cli
    )
    target_compile_definitions(cj4_winning_results_tests PRIVATE
        CJ4_EXAMPLES_CLI_NO_MAIN
    )
    target_link_libraries(cj4_winning_results_tests PRIVATE cj4_opponents)

    # These tests use the standard assert macro as their test framework.
    # Keep assertions active in Release and other NDEBUG configurations.
    if (MSVC)
        target_compile_options(cj4_opponent_tests PRIVATE /UNDEBUG)
        target_compile_options(cj4_winning_results_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(cj4_opponent_tests PRIVATE -UNDEBUG)
        target_compile_options(cj4_winning_results_tests PRIVATE -UNDEBUG)
    endif()

    add_test(NAME cj4_opponent_tests COMMAND cj4_opponent_tests)
    add_test(NAME cj4_winning_results_tests COMMAND cj4_winning_results_tests)
endif()

install(TARGETS cj4_opponents
    EXPORT cjong4-opponentTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/cjong4-opponent
)
if (NOT CJ4_OPPONENT_USE_SYSTEM_CJONG4)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/external/cjong4/LICENSE
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/cjong4-opponent
        RENAME LICENSE-cjong4
    )
endif()

set(CJ4_OPPONENT_CMAKE_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/cjong4-opponent")

install(EXPORT cjong4-opponentTargets
    FILE cjong4-opponentTargets.cmake
    NAMESPACE cjong4::
    DESTINATION ${CJ4_OPPONENT_CMAKE_INSTALL_DIR}
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cjong4-opponentConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cjong4-opponentConfig.cmake"
    INSTALL_DESTINATION ${CJ4_OPPONENT_CMAKE_INSTALL_DIR}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cjong4-opponentConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cjong4-opponentConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cjong4-opponentConfigVersion.cmake"
    DESTINATION ${CJ4_OPPONENT_CMAKE_INSTALL_DIR}
)
