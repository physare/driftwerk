cmake_minimum_required(VERSION 3.22)
project(Driftwerk VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static MSVC runtime so the plugin works without installing VC++ redistributables.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise fetch it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Driftwerk
    COMPANY_NAME "Driftwerk"
    BUNDLE_ID "com.driftwerk.driftwerk"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Drfw
    PLUGIN_CODE Dw94
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 Standalone
    PRODUCT_NAME "Driftwerk")

target_sources(Driftwerk PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Driftwerk PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_CAMERA=$<IF:$<PLATFORM_ID:Linux>,0,1>)

target_link_libraries(Driftwerk
    PRIVATE
        juce::juce_audio_utils
        juce::juce_video
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional offline render tool for testing (not part of the plugin).
option(DRIFTWERK_BUILD_TOOLS "Build the offline render test tool" OFF)
if(DRIFTWERK_BUILD_TOOLS)
    juce_add_console_app(DriftwerkRender PRODUCT_NAME "DriftwerkRender")
    target_sources(DriftwerkRender PRIVATE tools/Render.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(DriftwerkRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_CAMERA=0)
    target_link_libraries(DriftwerkRender PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_video juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
