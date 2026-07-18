#pragma once

#include "SequencerModel.h"

class PsytrancerAudioProcessor final : public juce::AudioProcessor
{
public:
    PsytrancerAudioProcessor();
    ~PsytrancerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

    StepData getStep (int index) const;
    void setStep (int index, StepData step);
    void clearPattern();
    void resetToInitialPattern();
    void repeatFirstPageToLength();
    void shiftPattern (int amount);
    void panic();

    int getCurrentStep() const { return currentStep.load(); }
    int getBaseRootMidiNote() const;
    bool isMidiKeyEnabled() const;
    ScaleType getScaleType() const;
    StepResolution getResolution() const;
    int getStepsPerPage() const;
    int getPageCount() const;
    int getSequenceLength() const;
    float getGateMultiplier() const;

    juce::File getPresetDirectory() const;
    juce::StringArray getPresetNames() const;
    bool savePreset (const juce::String& name);
    bool loadPreset (const juce::String& name);
    juce::String getCurrentPresetName() const { return currentPresetName; }
    juce::String getLastPresetError() const { return lastPresetError; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    template <typename FloatType>
    void processAudioBlock (juce::AudioBuffer<FloatType>&, juce::MidiBuffer&);

    juce::ValueTree stepsToValueTree() const;
    void loadStepsFromValueTree (const juce::ValueTree&);
    juce::ValueTree createStateValueTree();
    void loadStateValueTree (const juce::ValueTree&);
    void renderSequenceSegment (juce::MidiBuffer&, int segmentStart, int segmentLength,
                                double ppq, double bpm, int baseNote);
    void sendActiveNoteOff (juce::MidiBuffer&, int sampleOffset);
    void resetTransportState();

    juce::AudioProcessorValueTreeState parameters;
    mutable juce::CriticalSection stepLock;
    std::array<StepData, maxSequenceSteps> steps {};

    double currentSampleRate = 44100.0;
    double previousPpq = 0.0;
    double triggerPpq = 0.0;
    double pendingNoteOffPpq = -1.0;
    int activeNote = -1;
    int activeChannel = 1;
    int triggerNote = -1;
    int triggerChannel = 1;
    std::atomic<int> currentStep { -1 };
    std::atomic<int> midiKeyNote { -1 };
    bool midiTriggerActive = false;
    bool wasPlaying = false;
    std::atomic<bool> panicRequested { false };
    juce::String lastPresetError;
    juce::String currentPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsytrancerAudioProcessor)
};
