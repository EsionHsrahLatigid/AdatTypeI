cmake_minimum_required(VERSION 3.22)

project(AdatTypeI VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ADATTYPEI_BUILD_PLUGIN "Build the JUCE plug-in targets" ON)
option(ADATTYPEI_BUILD_TESTS "Build AdatTypeI tests" ON)
set(ADATTYPEI_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout. If empty, FetchContent downloads JUCE.")
set(ADATTYPEI_ARTIFACT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/artifacts" CACHE PATH
    "Directory for final VST3, AU, and Standalone products")

add_library(AdatTypeICore INTERFACE)
target_include_directories(AdatTypeICore INTERFACE Source)
target_compile_features(AdatTypeICore INTERFACE cxx_std_17)

if(ADATTYPEI_BUILD_TESTS)
    enable_testing()

    add_executable(AdatTypeIDspTests Tests/DspTests.cpp)
    target_link_libraries(AdatTypeIDspTests PRIVATE AdatTypeICore)
    add_test(NAME AdatTypeIDspTests COMMAND AdatTypeIDspTests)
endif()

if(ADATTYPEI_BUILD_PLUGIN)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    if(ADATTYPEI_JUCE_PATH)
        add_subdirectory("${ADATTYPEI_JUCE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.15
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(ADATTYPEI_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND ADATTYPEI_FORMATS AU)
    endif()

    juce_add_plugin(AdatTypeI
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "EsionHsrahLatigid"
        BUNDLE_ID "jp.ehl.adattypei"
        PRODUCT_NAME "AdatTypeI"
        DESCRIPTION "ADAT Type I transfer fault audio effect"
        PLUGIN_MANUFACTURER_CODE EHL_
        PLUGIN_CODE AdtI
        FORMATS ${ADATTYPEI_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(AdatTypeI PRIVATE
        Source/PluginEntry.cpp
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h)

    target_include_directories(AdatTypeI PRIVATE Source)
    target_link_libraries(AdatTypeI
        PRIVATE
            AdatTypeICore
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(AdatTypeI PUBLIC
        JUCE_REPORT_APP_USAGE=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0)

    juce_generate_juce_header(AdatTypeI)

    set(adattypei_juce_products "${CMAKE_CURRENT_BINARY_DIR}/AdatTypeI_artefacts/$<CONFIG>")
    set(adattypei_staged_products "${ADATTYPEI_ARTIFACT_DIR}/$<CONFIG>")
    set(adattypei_artifact_dependencies AdatTypeI_VST3 AdatTypeI_Standalone)
    set(adattypei_artifact_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${adattypei_staged_products}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${adattypei_staged_products}/VST3"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${adattypei_staged_products}/Standalone"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${adattypei_juce_products}/VST3" "${adattypei_staged_products}/VST3"
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NormalizeVst3ModuleInfo.py"
                "${adattypei_staged_products}/VST3/AdatTypeI.vst3/Contents/Resources/moduleinfo.json"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${adattypei_juce_products}/Standalone" "${adattypei_staged_products}/Standalone")

    if(TARGET AdatTypeI_AU)
        list(APPEND adattypei_artifact_dependencies AdatTypeI_AU)
        list(APPEND adattypei_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${adattypei_staged_products}/AU"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${adattypei_juce_products}/AU" "${adattypei_staged_products}/AU")
    endif()

    if(APPLE)
        list(APPEND adattypei_artifact_commands
            COMMAND codesign --force --deep --sign -
                    "${adattypei_staged_products}/VST3/AdatTypeI.vst3"
            COMMAND codesign --force --deep --sign -
                    "${adattypei_staged_products}/Standalone/AdatTypeI.app"
            COMMAND codesign --force --deep --sign -
                    "${adattypei_staged_products}/AU/AdatTypeI.component")
    endif()

    add_custom_target(AdatTypeI_Artifacts
        ${adattypei_artifact_commands}
        DEPENDS ${adattypei_artifact_dependencies}
        COMMENT "Staging AdatTypeI products in ${ADATTYPEI_ARTIFACT_DIR}/$<CONFIG>")

    if(ADATTYPEI_BUILD_TESTS)
        add_executable(AdatTypeIPluginSmokeTests Tests/PluginSmokeTests.cpp)
        target_include_directories(AdatTypeIPluginSmokeTests PRIVATE
            Source
            "${CMAKE_CURRENT_BINARY_DIR}/AdatTypeI_artefacts/JuceLibraryCode")
        target_link_libraries(AdatTypeIPluginSmokeTests PRIVATE AdatTypeI juce::juce_audio_utils)
        add_dependencies(AdatTypeIPluginSmokeTests AdatTypeI_Artifacts)
        add_test(NAME AdatTypeIPluginSmokeTests COMMAND AdatTypeIPluginSmokeTests)
        add_test(
            NAME AdatTypeIArtifactChecks
            COMMAND ${CMAKE_COMMAND}
                    -DADATTYPEI_STAGED_DIR=${ADATTYPEI_ARTIFACT_DIR}/$<CONFIG>
                    -DADATTYPEI_EXPECT_AU=$<BOOL:${APPLE}>
                    -DPython3_EXECUTABLE=${Python3_EXECUTABLE}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/VerifyArtifacts.cmake)
    endif()
endif()
