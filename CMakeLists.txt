cmake_minimum_required(VERSION 3.15)

project(FuzzCola VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE should exist as a submodule/folder at ./JUCE
add_subdirectory(JUCE)

juce_add_plugin(FuzzCola
    COMPANY_NAME "Silver DSP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE SlvD
    PLUGIN_CODE FzCl
    FORMATS VST3 AU
    PRODUCT_NAME "Fuzz Cola"
)

juce_generate_juce_header(FuzzCola)

target_include_directories(FuzzCola PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/FuzzCola_artefacts/JuceLibraryCode
)

juce_add_binary_data(FuzzColaData
    SOURCES
        Assets/HiResBackground0001.png
        Assets/HiResBypassSwitch0001.png
        Assets/HiResBypassSwitch0002.png
        Assets/HiResLED0001.png
        Assets/HiResLED0038.png
        Assets/HiResOnOff0001.png
        Assets/HiResOnOff0002.png
        Assets/HiResSustainKnob_filmstrip.png
        Assets/HiResToneKnob_filmstrip.png
        Assets/HiResVolumeKnob_filmstrip.png
        Assets/LoResBackground0001.png
        Assets/LoResBypassSwitch0001.png
        Assets/LoResBypassSwitch0002.png
        Assets/LoResLED0001.png
        Assets/LoResLED0038.png
        Assets/LoResOnOff0001.png
        Assets/LoResOnOff0002.png
        Assets/LoResSustainKnob_filmstrip.png
        Assets/LoResToneKnob_filmstrip.png
        Assets/LoResVolumeKnob_filmstrip.png
)

# Your plugin sources (adjust if your filenames differ)
target_sources(FuzzCola PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(FuzzCola PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)


# Core JUCE modules most plugins need
target_link_libraries(FuzzCola PRIVATE
    FuzzColaData
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_audio_basics
    juce::juce_core
)
