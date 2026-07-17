#pragma once

#include "PluginProcessor.h"

class MouseWheelComboBox final : public juce::ComboBox
{
public:
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
};

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
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void configureControls();
    void drawStepGrid (juce::Graphics&, juce::Rectangle<int>);
    void drawPageOverview (juce::Graphics&, juce::Rectangle<int>);
    void editStepAt (juce::Point<int> position, bool drag);
    int getStepAtX (int x) const;
    int getGridRowAtY (int y) const;
    bool isPitchEditRow (int row) const;
    int getPageCount() const;
    void setPage (int newPage);
    void setSelectedStep (int step);
    void updatePageForSelection();
    void changeLengthBy (int amount);
    void refreshPresetList (const juce::String& selectedName = {});
    void saveCurrentPreset();
    void loadSelectedPreset();

    PsytrancerAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;

    MouseWheelComboBox resolutionBox;
    MouseWheelComboBox rootBox;
    MouseWheelComboBox scaleBox;
    juce::Slider lengthSlider;
    juce::Slider octaveSlider;
    juce::Slider gateMultiplierSlider;
    juce::TextButton lengthDownButton { "-16" };
    juce::TextButton lengthUpButton { "+16" };
    juce::TextButton prevPageButton { "<<" };
    juce::TextButton nextPageButton { ">>" };
    juce::ToggleButton pageMapToggle { "Map" };
    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "Save" };
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
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateMultiplierAttachment;

    int selectedStep = 0;
    int page = 0;
    int visibleSteps = 16;
    int dragStep = -1;
    int dragRow = -1;
    int dragStartY = 0;
    int dragStartPitch = 0;
    bool refreshingPresetList = false;
    juce::Rectangle<int> gridBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsytrancerAudioProcessorEditor)
};
