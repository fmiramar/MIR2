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
    plugins/MIR2Pitch.cpp
    plugins/MIR2Onsets.cpp
    plugins/MIR2Temporal.cpp
    plugins/MIR2Stereo.cpp
    plugins/MIR2FeatureDynamics.cpp
    plugins/MIR2SpectralAdvanced.cpp
    plugins/MIR2Auditory.cpp
    plugins/MIR2PitchAdvanced.cpp
    plugins/MIR2Wavelet.cpp
    plugins/MIR2ConstantQ.cpp
    plugins/MIR2PNCC.cpp
    plugins/MIR2Tonal.cpp
    plugins/MIR2Harmonic.cpp
    plugins/MIR2Rhythm.cpp
    plugins/MIR2RhythmModulation.cpp
)
file(GLOB mir2_sc_files CONFIGURE_DEPENDS Classes/*.sc)
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
install(DIRECTORY sounds/ DESTINATION MIR2/sounds)
install(DIRECTORY benchmarks/ DESTINATION MIR2/benchmarks)
install(DIRECTORY scripts/ DESTINATION MIR2/scripts)
install(DIRECTORY tests/listening/ DESTINATION MIR2/tests/listening)
install(DIRECTORY HelpSource/Guides/ DESTINATION MIR2/HelpSource/Guides)

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    file(GLOB mir2_test_files CONFIGURE_DEPENDS tests/cpp/*.cpp)
    foreach(test_file IN LISTS mir2_test_files)
        get_filename_component(test_name "${test_file}" NAME_WE)
        set(test_target "mir2_${test_name}")
        add_executable(${test_target} "${test_file}")
        target_include_directories(${test_target} PRIVATE include)
        target_compile_definitions(${test_target} PRIVATE MIR2_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        # Assertions are part of the numerical oracle, including Release builds.
        if(MSVC)
            target_compile_options(${test_target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${test_target} PRIVATE -UNDEBUG)
        endif()
        if(STRICT)
            if(MSVC)
                target_compile_options(${test_target} PRIVATE /W4)
            else()
                target_compile_options(${test_target} PRIVATE -Wall -Wextra -Wpedantic)
            endif()
        endif()
        if(MIR2_SANITIZE)
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
                target_compile_options(${test_target} PRIVATE
                    -fsanitize=address,undefined -fno-omit-frame-pointer)
                target_link_options(${test_target} PRIVATE -fsanitize=address,undefined)
            else()
                message(FATAL_ERROR "MIR2_SANITIZE requires Clang or GCC")
            endif()
        endif()
        add_test(NAME ${test_target} COMMAND ${test_target})
    endforeach()
endif()
