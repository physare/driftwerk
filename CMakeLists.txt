cmake_minimum_required(VERSION 3.22)
project(Timewind VERSION 0.6.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static MSVC runtime so the plugin works without installing VC++ redistributables.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise fetch it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
    set(TW_JUCE_SOURCE "${JUCE_DIR}")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
    set(TW_JUCE_SOURCE "${juce_SOURCE_DIR}")
endif()

# Timewind never draws with the GPU on Windows. Some Intel graphics drivers crash
# inside Direct2D (igd10umt64xe.dll), especially when the webcam thread creates
# frames, so every window uses JUCE's software renderer and every image lives in
# ordinary memory. JUCE's sources compile after this runs, so patching here works.
# The build fails loudly if a JUCE update moves these lines.
function(tw_patch_juce relpath from to)
    set(f "${TW_JUCE_SOURCE}/${relpath}")
    file(READ "${f}" txt)
    string(FIND "${txt}" "${to}" already)
    if(NOT already EQUAL -1)
        return()
    endif()
    string(FIND "${txt}" "${from}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "Timewind: could not apply the CPU-drawing patch to ${f}")
    endif()
    string(REPLACE "${from}" "${to}" txt "${txt}")
    file(WRITE "${f}" "${txt}")
endfunction()
tw_patch_juce(modules/juce_gui_basics/native/juce_Windowing_windows.cpp
    "return new HWNDComponentPeer { *this, styleFlags, (HWND) parentHWND, false, 1 };"
    "return new HWNDComponentPeer { *this, styleFlags, (HWND) parentHWND, false, 0 }; /* timewind: software renderer */")
tw_patch_juce(modules/juce_graphics/native/juce_Direct2DImage_windows.cpp
    "return new Direct2DPixelData (format, width, height, clearImage);"
    "return new SoftwarePixelData { format, width, height, clearImage }; /* timewind: CPU images */")

juce_add_plugin(Timewind
    COMPANY_NAME "Physare"
    BUNDLE_ID "com.physare.timewind"
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
    PRODUCT_NAME "Timewind")

target_sources(Timewind PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Timewind PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_CAMERA=$<IF:$<PLATFORM_ID:Linux>,0,1>)

target_link_libraries(Timewind
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

    juce_add_console_app(TimewindHandTest PRODUCT_NAME "TimewindHandTest")
    target_sources(TimewindHandTest PRIVATE tools/HandTest.cpp)
    target_compile_definitions(TimewindHandTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(TimewindHandTest PRIVATE juce::juce_gui_basics PUBLIC juce::juce_recommended_config_flags)
endif()
