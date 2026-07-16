#pragma once

#include "PluginProcessor.h"

class PsytrancerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit PsytrancerAudioProcessorEditor (PsytrancerAudioProcessor&);
    ~PsytrancerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void configureControls();
    void drawStepGrid (juce::Graphics&, juce::Rectangle<int>);
    void editStepAt (juce::Point<int> position, bool drag);
    int getStepAtX (int x) const;
    void setSelectedStep (int step);
    void updatePageForSelection();

    PsytrancerAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;

    juce::ComboBox resolutionBox;
    juce::ComboBox rootBox;
    juce::ComboBox scaleBox;
    juce::Slider lengthSlider;
    juce::Slider octaveSlider;
    juce::TextButton initButton { "Init" };
    juce::TextButton repeatButton { "Repeat" };
    juce::TextButton shiftLeftButton { "<" };
    juce::TextButton shiftRightButton { ">" };
    juce::TextButton panicButton { "Panic" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> resolutionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lengthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment;

    int selectedStep = 0;
    int page = 0;
    int visibleSteps = 16;
    juce::Rectangle<int> gridBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsytrancerAudioProcessorEditor)
};
