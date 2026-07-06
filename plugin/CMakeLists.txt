cmake_minimum_required(VERSION 3.22)
project(AstraTune VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# --- AAX (Pro Tools) ---
# The AAX SDK is distributed by Avid to registered developers only
# (https://developer.avid.com). Once you have it, configure with:
#   cmake -B build -DAAX_SDK_PATH=/path/to/aax-sdk-2-x-x
# and the AAX target is added automatically.
set(ASTRA_FORMATS AU VST3 Standalone)
if(DEFINED AAX_SDK_PATH)
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND ASTRA_FORMATS AAX)
    message(STATUS "AAX SDK found - AAX (Pro Tools) target enabled")
else()
    message(STATUS "AAX_SDK_PATH not set - building AU/VST3/Standalone only")
endif()

juce_add_plugin(AstraTune
    COMPANY_NAME "companyAIstack"
    BUNDLE_ID com.companyaistack.astratune
    PLUGIN_MANUFACTURER_CODE Cais
    PLUGIN_CODE Astn
    AAX_IDENTIFIER com.companyaistack.astratune
    FORMATS ${ASTRA_FORMATS}
    PRODUCT_NAME "ASTRA-TUNE"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(AstraTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(AstraTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AstraTune
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
