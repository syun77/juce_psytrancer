juce_add_plugin(Psytrancer
    VERSION 0.2.0
    COMPANY_NAME "Ozeki"
    BUNDLE_ID "com.ozeki.psytrancer"
    PLUGIN_MANUFACTURER_CODE Ozek
    PLUGIN_CODE PsGn
    FORMATS VST3
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
