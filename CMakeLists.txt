cmake_minimum_required(VERSION 3.20)
project(Lunax VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

set(LUNA_COMPILER "" CACHE FILEPATH "Luna compiler used to build Lunax")
if(NOT LUNA_COMPILER)
    find_program(LUNA_COMPILER
        NAMES luna
        HINTS
            "${CMAKE_CURRENT_SOURCE_DIR}/../build-stage-a-strict"
            "${CMAKE_CURRENT_SOURCE_DIR}/../build")
endif()
if(NOT LUNA_COMPILER)
    message(FATAL_ERROR
        "Luna compiler not found; pass -DLUNA_COMPILER=/path/to/luna")
endif()

set(LUNA_RUNTIME_LIB "" CACHE FILEPATH
    "Luna Runtime static library used for AOT linking")
if(NOT LUNA_RUNTIME_LIB)
    get_filename_component(LUNA_COMPILER_DIR "${LUNA_COMPILER}" DIRECTORY)
    set(LUNA_RUNTIME_LIB "${LUNA_COMPILER_DIR}/libruntime.a")
endif()
if(NOT EXISTS "${LUNA_RUNTIME_LIB}")
    message(FATAL_ERROR
        "Luna Runtime library not found; pass -DLUNA_RUNTIME_LIB=/path/to/libruntime.a")
endif()

add_library(lunax-host STATIC native/src/Host.cpp)
target_include_directories(lunax-host
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/native/include")
target_link_libraries(lunax-host PUBLIC CURL::libcurl ${CMAKE_DL_LIBS})

if(MSVC)
    target_compile_options(lunax-host PRIVATE /W4 /WX)
else()
    target_compile_options(lunax-host PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()

set(LUNAX_LUNA_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/luna-src")
set(LUNAX_EXECUTABLE
    "${LUNAX_LUNA_SOURCE_DIR}/org.luna.lunax${CMAKE_EXECUTABLE_SUFFIX}")
set(LUNAX_LUNA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/commands.luna"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/host.luna"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.luna")

add_custom_command(
    OUTPUT "${LUNAX_EXECUTABLE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${LUNAX_LUNA_SOURCE_DIR}/src"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/luna.package"
            "${LUNAX_LUNA_SOURCE_DIR}/luna.package"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            ${LUNAX_LUNA_SOURCES}
            "${LUNAX_LUNA_SOURCE_DIR}/src"
    COMMAND "${LUNA_COMPILER}" build "${LUNAX_LUNA_SOURCE_DIR}" -O2
            --runtime-lib "${LUNA_RUNTIME_LIB}"
            --link "$<TARGET_FILE:lunax-host>"
            --link curl
    DEPENDS lunax-host "${CMAKE_CURRENT_SOURCE_DIR}/luna.package"
            ${LUNAX_LUNA_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Building Lunax with Luna"
    VERBATIM)

add_custom_target(lunax ALL DEPENDS "${LUNAX_EXECUTABLE}")

include(GNUInstallDirs)
install(PROGRAMS "${LUNAX_EXECUTABLE}"
        DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME lunax)
install(FILES include/lunax/backend_abi.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/lunax")

include(CTest)
if(BUILD_TESTING)
    add_library(lunax-test-backend MODULE tests/FakeBackend.cpp)
    target_include_directories(lunax-test-backend PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    if(MSVC)
        target_compile_options(lunax-test-backend PRIVATE /W4 /WX)
    else()
        target_compile_options(lunax-test-backend PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME lunax.help COMMAND "${LUNAX_EXECUTABLE}" help)
    set_tests_properties(lunax.help PROPERTIES
        PASS_REGULAR_EXPRESSION "Luna ecosystem manager")
    add_test(NAME lunax.version COMMAND "${LUNAX_EXECUTABLE}" version)
    set_tests_properties(lunax.version PROPERTIES
        PASS_REGULAR_EXPRESSION "lunax 0.1.0")
    add_test(NAME lunax.backend COMMAND "${LUNAX_EXECUTABLE}" backend)
    set_tests_properties(lunax.backend PROPERTIES
        PASS_REGULAR_EXPRESSION "download backend: curl")
    add_test(NAME lunax.forward-version COMMAND "${LUNAX_EXECUTABLE}" --version)
    set_tests_properties(lunax.forward-version PROPERTIES
        ENVIRONMENT "LUNAX_LUNA=${LUNA_COMPILER}"
        PASS_REGULAR_EXPRESSION "Luna 0.2.1")
    add_test(
        NAME lunax.dynamic-backend
        COMMAND "${CMAKE_COMMAND}" -E env
            "LUNAX_DOWNLOAD_BACKEND=$<TARGET_FILE:lunax-test-backend>"
            "${LUNAX_EXECUTABLE}" backend)
    set_tests_properties(lunax.dynamic-backend PROPERTIES
        PASS_REGULAR_EXPRESSION "download backend: fake-dynamic")
    add_test(
        NAME lunax.install-integration
        COMMAND "${CMAKE_COMMAND}"
            -DLUNAX_EXECUTABLE=${LUNAX_EXECUTABLE}
            -DLUNAX_TEST_BACKEND=$<TARGET_FILE:lunax-test-backend>
            -DLUNAX_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DLUNAX_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -DLUNA_EXECUTABLE=${LUNA_COMPILER}
            -DLUNA_RUNTIME_LIB=${LUNA_RUNTIME_LIB}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/install_integration.cmake)
    set_tests_properties(lunax.install-integration PROPERTIES
        LABELS "integration;release"
        TIMEOUT 60)
endif()
