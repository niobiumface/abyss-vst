cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

project(Abyss VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ABYSS_STANDALONE "Build Standalone" ON)
option(ABYSS_AVX2 "AVX2" OFF)

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

include(cmake/JuceFetch.cmake)

set(ABYSS_FORMATS VST3)
if(ABYSS_STANDALONE)
    list(APPEND ABYSS_FORMATS Standalone)
endif()

juce_add_plugin(Abyss
    PLUGIN_NAME "Abyss"
    PRODUCT_NAME "Abyss"
    COMPANY_NAME "n-f.pro"
    BUNDLE_ID "pro.n-f.abyss"
    PLUGIN_MANUFACTURER_CODE Nfpr
    PLUGIN_CODE Abys
    FORMATS ${ABYSS_FORMATS}
    VST3_CATEGORIES Fx Filter Distortion
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

set(ABYSS_ASSETS "")

foreach(level 000 010 020 030 040 050 060 070 080 090 100)
    foreach(frame 0 1 2)
        set(_asset "${CMAKE_CURRENT_SOURCE_DIR}/anim/depth_${level}_f${frame}.png")
        if(NOT EXISTS "${_asset}")
            message(FATAL_ERROR "Missing asset: ${_asset}")
        endif()
        list(APPEND ABYSS_ASSETS "${_asset}")
    endforeach()
endforeach()

foreach(_rel
        ui/ui_frame.png
        ui/knob_depth_strip.png
        ui/knob_pressure_strip.png
        ui/knob_small_strip.png
        ui/bypass_off.png
        ui/bypass_on.png
        ui/screen_off.png
        palette.json)
    set(_asset "${CMAKE_CURRENT_SOURCE_DIR}/${_rel}")
    if(NOT EXISTS "${_asset}")
        message(FATAL_ERROR "Missing asset: ${_asset}")
    endif()
    list(APPEND ABYSS_ASSETS "${_asset}")
endforeach()

set(ABYSS_FACTORY_XML
    Source/presets/factory/insert_coin.xml
    Source/presets/factory/first_step.xml
    Source/presets/factory/freefall.xml
    Source/presets/factory/black_shaft.xml
    Source/presets/factory/low_light.xml
    Source/presets/factory/red_eyes.xml
    Source/presets/factory/bottomless.xml
    Source/presets/factory/crushing_dark.xml
    Source/presets/factory/game_over.xml
    Source/presets/factory/the_abyss.xml
)

foreach(_rel ${ABYSS_FACTORY_XML})
    set(_asset "${CMAKE_CURRENT_SOURCE_DIR}/${_rel}")
    if(NOT EXISTS "${_asset}")
        message(FATAL_ERROR "Missing factory preset: ${_asset}")
    endif()
    list(APPEND ABYSS_ASSETS "${_asset}")
endforeach()

juce_add_binary_data(AbyssAssets
    NAMESPACE AbyssData
    SOURCES ${ABYSS_ASSETS}
)

set(ABYSS_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/presets/PresetManager.cpp
    Source/dsp/PitchTracker.cpp
    Source/dsp/SubOscillator.cpp
    Source/dsp/Darkener.cpp
    Source/dsp/PressureComp.cpp
    Source/dsp/PressureDrive.cpp
    Source/dsp/AutoGain.cpp
    Source/dsp/AbyssEngine.cpp
    Source/ui/IndexedImage.cpp
    Source/ui/PixelScaler.cpp
    Source/ui/PixelFont.cpp
    Source/ui/PressureFx.cpp
    Source/ui/SpriteBank.cpp
    Source/ui/ScreenComponent.cpp
    Source/ui/AbyssLookAndFeel.cpp
    Source/ui/KnobStrip.cpp
    Source/ui/BypassButton.cpp
)

target_sources(Abyss PRIVATE ${ABYSS_SOURCES})
target_include_directories(Abyss PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(Abyss
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Abyss
    PRIVATE
        AbyssAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

function(abyss_apply_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8 /MP>
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /fp:fast>
    )
    if(ABYSS_AVX2)
        target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>)
    endif()
endfunction()

abyss_apply_warnings(Abyss)

juce_add_console_app(AbyssTests PRODUCT_NAME "AbyssTests")
target_sources(AbyssTests PRIVATE ${ABYSS_SOURCES} tests/AbyssTests.cpp)
target_include_directories(AbyssTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_definitions(AbyssTests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=1
)
target_link_libraries(AbyssTests
    PRIVATE
        AbyssAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
if(WIN32)
    target_link_libraries(AbyssTests PRIVATE Psapi)
endif()
abyss_apply_warnings(AbyssTests)

juce_add_console_app(AbyssBench PRODUCT_NAME "AbyssBench")
target_sources(AbyssBench PRIVATE ${ABYSS_SOURCES} bench/AbyssBench.cpp)
target_include_directories(AbyssBench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_definitions(AbyssBench
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)
target_link_libraries(AbyssBench
    PRIVATE
        AbyssAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
abyss_apply_warnings(AbyssBench)

enable_testing()
add_test(NAME AbyssTests COMMAND AbyssTests)
