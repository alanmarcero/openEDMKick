cmake_minimum_required(VERSION 3.22)

project(OpenEDMKick VERSION 1.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

option(OEK_BUILD_PLUGIN "Build the plugin" ON)
option(OEK_BUILD_TESTS "Build the unit tests" ON)

include(FetchContent)

add_library(openedmkick_core STATIC
    Source/Core/KickSynth.cpp
    Source/Core/AutoSequencer.cpp
    Source/Core/Effects.cpp
    Source/Core/KickEngine.cpp)
target_include_directories(openedmkick_core PUBLIC Source/Core)
set_target_properties(openedmkick_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(OEK_BUILD_PLUGIN)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)

    juce_add_binary_data(KickAssets SOURCES
        assets/kicking-mule.png
        assets/analog-cyber-faceplate.png)
    set_target_properties(KickAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    juce_add_plugin(OpenEDMKick
        COMPANY_NAME "Alan Marcero"
        BUNDLE_ID "com.alanmarcero.openedmkick"
        PRODUCT_NAME "openEDMKick"
        PLUGIN_MANUFACTURER_CODE Amrc
        PLUGIN_CODE Oedk
        FORMATS VST3 AU LV2 Standalone
        LV2URI "https://github.com/alanmarcero/openEDMKick"
        VST3_CATEGORIES Instrument Drum Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(OpenEDMKick PRIVATE
        Source/Plugin/Parameters.cpp
        Source/Plugin/Presets.cpp
        Source/Plugin/EdmLookAndFeel.cpp
        Source/Plugin/Controls.cpp
        Source/Plugin/Panels.cpp
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp)

    target_include_directories(OpenEDMKick PRIVATE
        Source/Core
        Source/Plugin)

    target_compile_definitions(OpenEDMKick PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(OpenEDMKick
        PRIVATE
            openedmkick_core
            KickAssets
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

if(OEK_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.9.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)

    add_executable(openedmkick_tests
        Tests/KickSynthTests.cpp
        Tests/AutoSequencerTests.cpp
        Tests/EffectsTests.cpp
        Tests/KickEngineTests.cpp)

    target_include_directories(openedmkick_tests PRIVATE Tests)
    target_link_libraries(openedmkick_tests PRIVATE openedmkick_core Catch2::Catch2WithMain)

    enable_testing()
    include(Catch)
    catch_discover_tests(openedmkick_tests)

    if(OEK_BUILD_PLUGIN)
        add_executable(openedmkick_plugin_tests Tests/PluginTests.cpp)
        target_include_directories(openedmkick_plugin_tests PRIVATE Source/Plugin Source/Core)
        target_compile_definitions(openedmkick_plugin_tests PRIVATE $<TARGET_PROPERTY:OpenEDMKick,COMPILE_DEFINITIONS>)
        target_link_libraries(openedmkick_plugin_tests PRIVATE OpenEDMKick KickAssets juce::juce_audio_utils Catch2::Catch2WithMain)
        catch_discover_tests(openedmkick_plugin_tests)
    endif()
endif()
