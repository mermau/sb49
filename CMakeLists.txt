cmake_minimum_required(VERSION 3.22)
project(SB49 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# --- JUCE: cartella locale (JUCE_DIR) oppure download automatico ----------------------
set(JUCE_DIR "" CACHE PATH "Percorso di una copia locale di JUCE (vuoto = download)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- formati ------------------------------------------------------------------------------
set(SB49_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SB49_FORMATS AU)
endif()

# AAX: richiede l'AAX SDK di Avid (developer.avid.com). Passare -DAAX_SDK_PATH=/percorso/aax-sdk
set(AAX_SDK_PATH "" CACHE PATH "Percorso dell'Avid AAX SDK (vuoto = niente AAX)")
if(AAX_SDK_PATH)
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND SB49_FORMATS AAX)
endif()

juce_add_plugin(SB49
    COMPANY_NAME "MM Instruments"
    PRODUCT_NAME "SB-49 Ensemble"
    BUNDLE_ID "it.mminstruments.sb49"
    PLUGIN_MANUFACTURER_CODE Mmin
    PLUGIN_CODE Sb49
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    AAX_CATEGORY AAX_ePlugInCategory_SWGenerators
    FORMATS ${SB49_FORMATS})

target_sources(SB49 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/PerformerEngine.cpp
    Source/dsp/Circuits.cpp)

target_include_directories(SB49 PRIVATE Source)

target_compile_definitions(SB49 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SB49
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- test offline del motore (senza JUCE) ------------------------------------------------
option(SB49_BUILD_TESTS "Compila il test di rendering offline" ON)
if(SB49_BUILD_TESTS)
    add_executable(sb49_render_test tools/render_test.cpp Source/dsp/PerformerEngine.cpp Source/dsp/Circuits.cpp)
endif()
