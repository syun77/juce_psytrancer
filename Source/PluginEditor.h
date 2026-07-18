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
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void configureControls();
    void drawStepGrid (juce::Graphics&, juce::Rectangle<int>);
    void drawPageOverview (juce::Graphics&, juce::Rectangle<int>);
    int getPageAtOverviewPosition (juce::Point<int>) const;
    void editStepAt (juce::Point<int> position, bool drag);
    int getStepAtX (int x) const;
    int getGridRowAtY (int y) const;
    bool isStepToggleRow (int row) const;
    bool isPitchEditRow (int row) const;
    bool isTypeEditRow (int row) const;
    bool isGateEditRow (int row) const;
    bool isVelocityEditRow (int row) const;
    bool isEditableValueRow (int row) const;
    bool isStepParameterRow (int row) const;
    bool resetStepValueAt (int step, int row);
    int getPageCount() const;
    void setPage (int newPage);
    void setSelectedStep (int step);
    void updatePageForSelection();
    void changeLengthBy (int amount);
    void updateRootOctaveControls();
    void refreshPresetList (const juce::String& selectedName = {});
    void saveCurrentPreset();
    void loadSelectedPreset();
    void copySelectedStep();
    void pasteSelectedStep();
    void copyCurrentPage();
    void pasteCurrentPage();
    void appendLogMessage (const juce::String& message);
    void showLogWindow();

    PsytrancerAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;

    MouseWheelComboBox resolutionBox;
    MouseWheelComboBox rootBox;
    MouseWheelComboBox scaleBox;
    juce::Slider lengthSlider;
    juce::Slider octaveSlider;
    juce::Slider gateMultiplierSlider;
    juce::ToggleButton midiKeyToggle { "MIDI Key" };
    juce::TextButton lengthDownButton { "-16" };
    juce::TextButton lengthUpButton { "+16" };
    juce::ToggleButton followPlaybackToggle { "Follow" };
    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "Save" };
    juce::TextButton logButton { "Log" };
    juce::TextButton copyStepButton { "Copy Step" };
    juce::TextButton pasteStepButton { "Paste Step" };
    juce::TextButton copyPageButton { "Copy Page" };
    juce::TextButton pastePageButton { "Paste Page" };
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
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiKeyAttachment;

    int selectedStep = 0;
    int page = 0;
    int visibleSteps = 16;
    int dragStep = -1;
    int dragRow = -1;
    int dragStartY = 0;
    int dragStartPitch = 0;
    float dragStartGate = 1.0f;
    juce::uint8 dragStartVelocity = 100;
    int hoverStep = -1;
    int hoverRow = -1;
    bool refreshingPresetList = false;
    StepData copiedStep;
    std::array<StepData, 16> copiedPage {};
    bool hasCopiedStep = false;
    bool hasCopiedPage = false;
    juce::String logMessages;
    juce::Rectangle<int> gridBounds;
    juce::Rectangle<int> pageOverviewBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsytrancerAudioProcessorEditor)
};
