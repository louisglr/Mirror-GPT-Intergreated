cmake_minimum_required(VERSION 3.22)

project(MirrorPlugin VERSION 1.5.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Mirror
    COMPANY_NAME "Louis Gabriel"
    BUNDLE_ID "com.louisgabriel.mirror"
    PLUGIN_MANUFACTURER_CODE LGRL
    PLUGIN_CODE MIRR
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "MIRROR"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "MIRROR needs microphone access to process vocal input."
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(Mirror PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(Mirror PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Mirror PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

enable_testing()

juce_add_console_app(MirrorDspSmokeTests
    PRODUCT_NAME "MIRROR DSP Smoke Tests"
)

target_sources(MirrorDspSmokeTests PRIVATE
    Tests/DspSmokeTests.cpp
)

target_include_directories(MirrorDspSmokeTests PRIVATE
    Source
)

target_compile_definitions(MirrorDspSmokeTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(MirrorDspSmokeTests PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_test(NAME MirrorDspSmoke COMMAND MirrorDspSmokeTests)
