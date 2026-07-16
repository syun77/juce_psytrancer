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
    bool isMidiEffect() const override { return true; }
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
    void repeatFirstPageToLength();
    void shiftPattern (int amount);
    void panic();

    int getCurrentStep() const { return currentStep.load(); }
    int getBaseRootMidiNote() const;
    ScaleType getScaleType() const;
    StepResolution getResolution() const;
    int getSequenceLength() const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    template <typename FloatType>
    void processAudioBlock (juce::AudioBuffer<FloatType>&, juce::MidiBuffer&);

    juce::ValueTree stepsToValueTree() const;
    void loadStepsFromValueTree (const juce::ValueTree&);
    void sendActiveNoteOff (juce::MidiBuffer&, int sampleOffset);
    void resetTransportState();

    juce::AudioProcessorValueTreeState parameters;
    mutable juce::CriticalSection stepLock;
    std::array<StepData, 128> steps {};

    double currentSampleRate = 44100.0;
    double previousPpq = 0.0;
    double pendingNoteOffPpq = -1.0;
    int activeNote = -1;
    int activeChannel = 1;
    std::atomic<int> currentStep { -1 };
    bool wasPlaying = false;
    std::atomic<bool> panicRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsytrancerAudioProcessor)
};
