cmake_minimum_required(VERSION 3.20)

foreach(required IN ITEMS LUNAX_EXECUTABLE LUNAX_TEST_BACKEND
        LUNAX_SOURCE_DIR LUNAX_BINARY_DIR LUNA_EXECUTABLE LUNA_RUNTIME_LIB)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "${required} must name an existing test input")
    endif()
endforeach()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(test_root "${LUNAX_BINARY_DIR}/Testing/lunax-integration-${run_id}")
set(home "${test_root}/home")
set(archive_root "${test_root}/archives")
file(MAKE_DIRECTORY "${archive_root}/compiler/luna/bin")
file(MAKE_DIRECTORY "${archive_root}/compiler/luna/lib")
file(COPY "${LUNA_EXECUTABLE}"
     DESTINATION "${archive_root}/compiler/luna/bin")
file(COPY "${LUNA_RUNTIME_LIB}"
     DESTINATION "${archive_root}/compiler/luna/lib")
get_filename_component(luna_name "${LUNA_EXECUTABLE}" NAME)

set(compiler_archive "${archive_root}/compiler.tar")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${compiler_archive}"
            --format=gnutar luna
    WORKING_DIRECTORY "${archive_root}/compiler"
    RESULT_VARIABLE archive_result
    ERROR_VARIABLE archive_error)
if(NOT archive_result EQUAL 0)
    message(FATAL_ERROR "cannot create compiler fixture: ${archive_error}")
endif()
file(SHA256 "${compiler_archive}" compiler_sha256)

function(run_lunax expected_result fixture_archive)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "LUNAX_HOME=${home}"
            "LUNAX_DOWNLOAD_BACKEND=${LUNAX_TEST_BACKEND}"
            "LUNAX_TEST_ARCHIVE=${fixture_archive}"
            "${LUNAX_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error)
    if(NOT command_result EQUAL expected_result)
        message(FATAL_ERROR
            "lunax command failed: ${ARGN}\n"
            "expected: ${expected_result}\nactual: ${command_result}\n"
            "stdout: ${command_output}\nstderr: ${command_error}")
    endif()
    set(LUNAX_LAST_OUTPUT "${command_output}" PARENT_SCOPE)
    set(LUNAX_LAST_ERROR "${command_error}" PARENT_SCOPE)
endfunction()

run_lunax(0 "${compiler_archive}" install compiler 0.3.0
          https://fixtures.invalid/luna.tar "${compiler_sha256}")
set(installed_luna "${home}/compilers/0.3.0/bin/${luna_name}")
if(NOT EXISTS "${installed_luna}")
    message(FATAL_ERROR "compiler was not atomically published")
endif()

run_lunax(0 "${compiler_archive}" list compiler)
if(NOT LUNAX_LAST_OUTPUT MATCHES "compiler 0\\.3\\.0")
    message(FATAL_ERROR "installed compiler was not listed: ${LUNAX_LAST_OUTPUT}")
endif()
run_lunax(0 "${compiler_archive}" use compiler 0.3.0)
run_lunax(0 "${compiler_archive}" which)
string(STRIP "${LUNAX_LAST_OUTPUT}" selected_compiler)
cmake_path(CONVERT "${selected_compiler}" TO_CMAKE_PATH_LIST
           selected_compiler_normalized NORMALIZE)
cmake_path(CONVERT "${installed_luna}" TO_CMAKE_PATH_LIST
           installed_luna_normalized NORMALIZE)
if(NOT selected_compiler_normalized STREQUAL installed_luna_normalized)
    message(FATAL_ERROR
        "managed compiler selection mismatch: ${selected_compiler}")
endif()
run_lunax(0 "${compiler_archive}" --version)
if(NOT LUNAX_LAST_OUTPUT MATCHES "Luna 0\\.3\\.0")
    message(FATAL_ERROR "managed compiler forwarding failed: ${LUNAX_LAST_OUTPUT}")
endif()

file(MAKE_DIRECTORY "${archive_root}/package/package")
file(COPY "${LUNAX_SOURCE_DIR}/luna.package"
     DESTINATION "${archive_root}/package/package")
set(package_archive "${archive_root}/package.tar")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${package_archive}"
            --format=gnutar package
    WORKING_DIRECTORY "${archive_root}/package"
    RESULT_VARIABLE package_archive_result
    ERROR_VARIABLE package_archive_error)
if(NOT package_archive_result EQUAL 0)
    message(FATAL_ERROR "cannot create package fixture: ${package_archive_error}")
endif()
file(SHA256 "${package_archive}" package_sha256)
run_lunax(0 "${package_archive}" package fetch org.luna.integration 1.0.0
          https://fixtures.invalid/package.tar "${package_sha256}")
run_lunax(0 "${package_archive}" package list org.luna.integration)
if(NOT LUNAX_LAST_OUTPUT MATCHES "package org\\.luna\\.integration 1\\.0\\.0")
    message(FATAL_ERROR "cached package was not listed: ${LUNAX_LAST_OUTPUT}")
endif()

string(REPEAT "0" 64 wrong_sha256)
run_lunax(4 "${compiler_archive}" install sdk checksum-negative
          https://fixtures.invalid/sdk.tar "${wrong_sha256}")
if(EXISTS "${home}/sdks/checksum-negative")
    message(FATAL_ERROR "checksum mismatch published an installation")
endif()

file(MAKE_DIRECTORY "${archive_root}/broken/broken")
file(COPY "${LUNAX_SOURCE_DIR}/README.md"
     DESTINATION "${archive_root}/broken/broken")
set(broken_archive "${archive_root}/broken.tar")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${broken_archive}"
            --format=gnutar broken
    WORKING_DIRECTORY "${archive_root}/broken"
    RESULT_VARIABLE broken_archive_result
    ERROR_VARIABLE broken_archive_error)
if(NOT broken_archive_result EQUAL 0)
    message(FATAL_ERROR "cannot create invalid fixture: ${broken_archive_error}")
endif()
file(SHA256 "${broken_archive}" broken_sha256)
run_lunax(5 "${broken_archive}" install compiler invalid-layout
          https://fixtures.invalid/broken.tar "${broken_sha256}")
if(EXISTS "${home}/compilers/invalid-layout")
    message(FATAL_ERROR "invalid compiler layout was published")
endif()
file(GLOB retained_staging
     "${home}/compilers/.invalid-layout.staging-*")
if(NOT retained_staging)
    message(FATAL_ERROR "invalid staged installation was not retained for audit")
endif()
