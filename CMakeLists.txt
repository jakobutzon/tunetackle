cmake_minimum_required(VERSION 3.22)
project(TuneTackle VERSION 1.0.0)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

add_subdirectory(JUCE)

juce_add_plugin(TuneTackle
    COMPANY_NAME "TuneTackle Audio"
    BUNDLE_ID com.tunetackle.tunetackle
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Tntk
    PLUGIN_CODE Tntl
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "TuneTackle"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "TuneTackle needs microphone access to process your voice.")

target_sources(TuneTackle PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(TuneTackle PRIVATE cxx_std_17)

target_compile_definitions(TuneTackle PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(TuneTackle
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags)
