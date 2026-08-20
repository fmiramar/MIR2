cmake_minimum_required(VERSION 3.12)

project(MIR2 VERSION 0.1.0 LANGUAGES CXX)

option(SCSYNTH "Build the scsynth plug-in" ON)
option(SUPERNOVA "Build the supernova plug-in" OFF)
option(STRICT "Enable strict compiler warnings" OFF)
option(MIR2_SANITIZE "Enable address and undefined-behavior sanitizers for tests" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/stage"
        CACHE PATH "Install prefix" FORCE)
endif()

if(NOT SC_PATH)
    message(FATAL_ERROR "SC_PATH must point to a matching SuperCollider source tree")
endif()

get_filename_component(SC_PATH_FOR_MODULES "${SC_PATH}" ABSOLUTE
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${SC_PATH_FOR_MODULES}/tools/cmake_gen")
include(SuperColliderServerPlugin)
sc_do_initial_compiler_config()
sc_check_sc_path("${SC_PATH}")

set(mir2_cpp_files
    plugins/MIR2.cpp
    plugins/MIR2Spectral.cpp
    plugins/MIR2Pitch.cpp)
set(mir2_sc_files Classes/MIR2.sc)
file(GLOB mir2_schelp_files CONFIGURE_DEPENDS HelpSource/Classes/*.schelp)

sc_add_server_plugin(
    "MIR2"
    "MIR2"
    "${mir2_cpp_files}"
    "${mir2_sc_files}"
    "${mir2_schelp_files}"
)

foreach(target MIR2_scsynth MIR2_supernova)
    if(TARGET ${target})
        target_include_directories(${target} SYSTEM PRIVATE
            "${SC_PATH_FOR_MODULES}/include/plugin_interface"
            "${SC_PATH_FOR_MODULES}/include/common"
            "${SC_PATH_FOR_MODULES}/common")
        target_include_directories(${target} PRIVATE include plugins)
        if(STRICT)
            if(MSVC)
                target_compile_options(${target} PRIVATE /W4)
            else()
                target_compile_options(${target} PRIVATE
                    -Wall -Wextra -Wpedantic -Wconversion
                    -Wno-unused-parameter)
            endif()
        endif()
    endif()
endforeach()

if(TARGET MIR2_scsynth)
    set_target_properties(MIR2_scsynth PROPERTIES OUTPUT_NAME "MIR2")
    if(MINGW)
        target_link_options(MIR2_scsynth PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

if(TARGET MIR2_supernova)
    set_target_properties(MIR2_supernova PROPERTIES OUTPUT_NAME "MIR2_supernova")
endif()

install(FILES README.md CHANGELOG.md LICENSE ORIGINS.md THIRD_PARTY_NOTICES.md
    DESTINATION MIR2)
install(DIRECTORY docs/ DESTINATION MIR2/docs)
install(DIRECTORY benchmarks/ DESTINATION MIR2/benchmarks)
install(DIRECTORY scripts/ DESTINATION MIR2/scripts)
install(DIRECTORY tests/listening/ DESTINATION MIR2/tests/listening)
install(DIRECTORY HelpSource/Guides/ DESTINATION MIR2/HelpSource/Guides)

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(mir2_reference_tests tests/cpp/MIR2ReferenceTests.cpp)
    target_include_directories(mir2_reference_tests PRIVATE include)
    if(STRICT)
        if(MSVC)
            target_compile_options(mir2_reference_tests PRIVATE /W4)
        else()
            target_compile_options(mir2_reference_tests PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion)
        endif()
    endif()
    if(MIR2_SANITIZE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(mir2_reference_tests PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(mir2_reference_tests PRIVATE
                -fsanitize=address,undefined)
        else()
            message(FATAL_ERROR "MIR2_SANITIZE requires Clang or GCC")
        endif()
    endif()
    add_test(NAME mir2_reference_tests COMMAND mir2_reference_tests)
endif()

