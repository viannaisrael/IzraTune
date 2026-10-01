cmake_minimum_required(VERSION 3.22)
project(IzraTune VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 20)

# JUCE is downloaded automatically (needs internet on first configure).
include(FetchContent)
FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

# JUCE's own JS bridge (getSliderState / getToggleState / events) is shipped with the module.
set(JUCE_JS ${juce_SOURCE_DIR}/modules/juce_gui_extra/native/javascript/index.js)
configure_file(${JUCE_JS} ${CMAKE_BINARY_DIR}/ui_gen/juce_frontend.js COPYONLY)

juce_add_binary_data(IzraTuneUI SOURCES
    ui/index.html
    ui/app.js
    ${CMAKE_BINARY_DIR}/ui_gen/juce_frontend.js)

juce_add_plugin(IzraTune
    COMPANY_NAME "IzraAudio"
    PLUGIN_MANUFACTURER_CODE Izra
    PLUGIN_CODE Iztn
    FORMATS VST3 Standalone
    PRODUCT_NAME "IzraTune"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(IzraTune PRIVATE
    Source/PluginProcessor.cpp Source/PluginEditor.cpp)

target_compile_definitions(IzraTune PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)

target_link_libraries(IzraTune
    PRIVATE IzraTuneUI juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
