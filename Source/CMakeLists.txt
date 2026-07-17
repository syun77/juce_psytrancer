juce_add_plugin(Psytrancer
    VERSION 0.2.0
    COMPANY_NAME "Ozeki"
    BUNDLE_ID "com.ozeki.psytrancer"
    PLUGIN_MANUFACTURER_CODE Ozek
    PLUGIN_CODE PsGn
    FORMATS VST3
    VST3_CATEGORIES Instrument Synth
    PRODUCT_NAME "Psytrancer"
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_SYNTH TRUE
    NEEDS_CURL FALSE
    COPY_PLUGIN_AFTER_BUILD ${JUCE_COPY_PLUGIN_AFTER_BUILD})

target_sources(Psytrancer
    PRIVATE
        PluginEditor.cpp
        PluginProcessor.cpp)

target_compile_features(Psytrancer PRIVATE cxx_std_17)

target_compile_definitions(Psytrancer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Psytrancer
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

set(PSYTRANCER_VST3_COPY_DIR "/Users/syun77/Library/Audio/Plug-Ins/VST3"
    CACHE PATH "Directory where Psytrancer.vst3 is copied after building")

option(PSYTRANCER_COPY_VST3_AFTER_BUILD
    "Copy Psytrancer.vst3 to the configured VST3 directory after building" ON)

if(APPLE AND PSYTRANCER_COPY_VST3_AFTER_BUILD)
    add_custom_command(TARGET Psytrancer_VST3 POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${PSYTRANCER_VST3_COPY_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "$<TARGET_BUNDLE_DIR:Psytrancer_VST3>"
            "${PSYTRANCER_VST3_COPY_DIR}/Psytrancer.vst3"
        COMMAND codesign --force --sign - --deep
            "${PSYTRANCER_VST3_COPY_DIR}/Psytrancer.vst3"
        VERBATIM)
endif()
