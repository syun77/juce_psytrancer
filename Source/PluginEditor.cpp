#include "PluginEditor.h"

namespace
{
constexpr auto headerHeight = 96;
constexpr auto footerHeight = 44;
constexpr auto stepHeaderHeight = 30;
constexpr auto rowHeight = 31;

juce::Colour background() { return juce::Colour (0xff17191d); }
juce::Colour panel() { return juce::Colour (0xff22262c); }
juce::Colour cell() { return juce::Colour (0xff2b3037); }
juce::Colour selected() { return juce::Colour (0xffe6c15a); }
juce::Colour playhead() { return juce::Colour (0xff59c0d8); }
juce::Colour text() { return juce::Colour (0xffe8ecef); }
juce::Colour dimText() { return juce::Colour (0xff9aa4ad); }

void drawFieldLabel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label, bool enabled)
{
    g.setColour (enabled ? dimText() : dimText().withAlpha (0.42f));
    g.setFont (13.0f);
    g.drawFittedText (label, area, juce::Justification::centredLeft, 1);
}

void setComboItems (juce::ComboBox& box, const juce::StringArray& items)
{
    box.clear (juce::dontSendNotification);

    for (auto i = 0; i < items.size(); ++i)
        box.addItem (items[i], i + 1);
}
}

void MouseWheelComboBox::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (getNumItems() <= 1 || juce::approximatelyEqual (wheel.deltaY, 0.0f))
        return;

    const auto direction = wheel.deltaY > 0.0f ? -1 : 1;
    const auto currentIndex = juce::jmax (0, getSelectedItemIndex());
    setSelectedItemIndex (juce::jlimit (0, getNumItems() - 1, currentIndex + direction),
                          juce::sendNotificationSync);
}

PsytrancerAudioProcessorEditor::PsytrancerAudioProcessorEditor (PsytrancerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), parameters (p.getParameters())
{
    setSize (1080, 440);
    setResizable (true, true);
    setWantsKeyboardFocus (true);
    configureControls();
    startTimerHz (20);
}

void PsytrancerAudioProcessorEditor::configureControls()
{
    auto addCombo = [this] (juce::ComboBox& box)
    {
        addAndMakeVisible (box);
        box.setColour (juce::ComboBox::backgroundColourId, cell());
        box.setColour (juce::ComboBox::textColourId, text());
        box.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3b424c));
        box.setScrollWheelEnabled (true);
    };

    addCombo (lengthBox);
    addCombo (resolutionBox);
    addCombo (rootBox);
    addCombo (scaleBox);
    addCombo (presetBox);
    presetBox.setEditableText (true);
    presetBox.setTextWhenNothingSelected ("Preset");

    setComboItems (lengthBox, { "1", "2", "3", "4", "5", "6", "7", "8" });
    setComboItems (resolutionBox, { "1/8", "1/16", "1/32", "1/8T", "1/16T", "1/32T", "1/8D", "1/16D" });
    setComboItems (rootBox, { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" });

    juce::StringArray scaleNames;
    for (const auto& scale : getScaleDefinitions())
        scaleNames.add (scale.name);

    setComboItems (scaleBox, scaleNames);

    auto addSlider = [this] (juce::Slider& slider)
    {
        addAndMakeVisible (slider);
        slider.setSliderStyle (juce::Slider::IncDecButtons);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 24);
        slider.setColour (juce::Slider::textBoxTextColourId, text());
        slider.setColour (juce::Slider::textBoxBackgroundColourId, cell());
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3b424c));
    };

    addSlider (octaveSlider);

    addAndMakeVisible (gateMultiplierSlider);
    gateMultiplierSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gateMultiplierSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 20);
    gateMultiplierSlider.setColour (juce::Slider::rotarySliderFillColourId, selected());
    gateMultiplierSlider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3b424c));
    gateMultiplierSlider.setColour (juce::Slider::thumbColourId, text());
    gateMultiplierSlider.setColour (juce::Slider::textBoxTextColourId, text());
    gateMultiplierSlider.setColour (juce::Slider::textBoxBackgroundColourId, background());
    gateMultiplierSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3b424c));

    for (auto* button : { &savePresetButton, &openPresetFolderButton,
                          &logButton, &copyStepButton, &pasteStepButton, &copyPageButton, &pastePageButton,
                          &initButton, &repeatButton, &shiftLeftButton, &shiftRightButton, &panicButton })
    {
        addAndMakeVisible (*button);
        button->setColour (juce::TextButton::buttonColourId, cell());
        button->setColour (juce::TextButton::textColourOffId, text());
    }

    addAndMakeVisible (followPlaybackToggle);
    followPlaybackToggle.setToggleState (true, juce::dontSendNotification);
    followPlaybackToggle.setColour (juce::ToggleButton::textColourId, text());
    followPlaybackToggle.setColour (juce::ToggleButton::tickColourId, selected());
    followPlaybackToggle.setColour (juce::ToggleButton::tickDisabledColourId, dimText());

    addAndMakeVisible (midiKeyToggle);
    midiKeyToggle.setColour (juce::ToggleButton::textColourId, text());
    midiKeyToggle.setColour (juce::ToggleButton::tickColourId, selected());
    midiKeyToggle.setColour (juce::ToggleButton::tickDisabledColourId, dimText());

    midiKeyToggle.onClick = [this] { updateRootOctaveControls(); repaint(); };
    presetBox.onChange = [this] { loadSelectedPreset(); };
    savePresetButton.onClick = [this] { saveCurrentPreset(); };
    openPresetFolderButton.onClick = [this] { openPresetFolder(); };
    logButton.onClick = [this] { showLogWindow(); };
    copyStepButton.onClick = [this] { copySelectedStep(); };
    pasteStepButton.onClick = [this] { pasteSelectedStep(); };
    copyPageButton.onClick = [this] { copyCurrentPage(); };
    pastePageButton.onClick = [this] { pasteCurrentPage(); };
    pasteStepButton.setEnabled (false);
    pastePageButton.setEnabled (false);

    initButton.onClick = [this]
    {
        processor.resetToInitialPattern();
        selectedStep = 0;
        page = 0;
        repaint();
    };

    repeatButton.onClick = [this]
    {
        processor.repeatFirstPageToLength();
        repaint();
    };

    shiftLeftButton.onClick = [this] { processor.shiftPattern (-1); repaint(); };
    shiftRightButton.onClick = [this] { processor.shiftPattern (1); repaint(); };
    panicButton.onClick = [this] { processor.panic(); };

    resolutionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (parameters, "resolution", resolutionBox);
    rootAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (parameters, "root", rootBox);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (parameters, "scale", scaleBox);
    lengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (parameters, "length", lengthBox);
    octaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (parameters, "octave", octaveSlider);
    gateMultiplierAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (parameters, "gateMultiplier", gateMultiplierSlider);
    midiKeyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters, "midiKey", midiKeyToggle);

    updateRootOctaveControls();
    refreshPresetList();
}

void PsytrancerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background());

    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop (headerHeight).reduced (16, 10);

    g.setColour (text());
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawFittedText ("Psytrancer", header.removeFromLeft (240), juce::Justification::centredLeft, 1);

    const std::array<std::pair<juce::Component*, const char*>, 6> labelledControls {{
        { &lengthBox, "Pages" },
        { &resolutionBox, "Rate" },
        { &rootBox, "Root" },
        { &octaveSlider, "Octave" },
        { &gateMultiplierSlider, "Gate Mult" },
        { &scaleBox, "Scale" }
    }};

    for (const auto& item : labelledControls)
    {
        auto labelArea = item.first->getBounds().translated (0, -18).withHeight (16);
        drawFieldLabel (g, labelArea, item.second, item.first->isEnabled());
    }

    drawStepGrid (g, gridBounds);

    auto footer = getLocalBounds().removeFromBottom (footerHeight).reduced (16, 6);
    g.setColour (panel());
    g.fillRoundedRectangle (footer.toFloat(), 6.0f);

    auto overview = footer.removeFromRight (252).reduced (8, 3);
    pageOverviewBounds = overview;

    g.setColour (dimText());
    g.setFont (14.0f);
    footer.removeFromRight (356);
    g.drawFittedText ("Page " + juce::String (page + 1) + "/" + juce::String (getPageCount()) + "   Selected Step "
                          + juce::String (selectedStep + 1)
                          + "   Click the mini map to change page   G/C/H",
                      footer.reduced (12, 0), juce::Justification::centredLeft, 1);

    drawPageOverview (g, overview);
}

void PsytrancerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto top = area.removeFromTop (headerHeight - 16);
    top.removeFromLeft (190);

    auto row1 = top.removeFromTop (52);
    auto setControl = [] (juce::Rectangle<int>& row, juce::Component& control, int controlWidth)
    {
        control.setBounds (row.removeFromLeft (controlWidth).reduced (3, 4));
        row.removeFromLeft (10);
    };

    setControl (row1, lengthBox, 74);
    setControl (row1, resolutionBox, 86);
    setControl (row1, rootBox, 70);
    setControl (row1, octaveSlider, 86);
    setControl (row1, midiKeyToggle, 96);
    gateMultiplierSlider.setBounds (row1.removeFromLeft (82).reduced (8, 0));
    row1.removeFromLeft (8);
    setControl (row1, scaleBox, 150);

    auto row2 = top.removeFromTop (34);
    followPlaybackToggle.setBounds (row2.removeFromLeft (82).reduced (3, 2));
    row2.removeFromLeft (14);
    initButton.setBounds (row2.removeFromLeft (70).reduced (3, 4));
    row2.removeFromLeft (10);
    repeatButton.setBounds (row2.removeFromLeft (86).reduced (3, 4));
    row2.removeFromLeft (10);
    shiftLeftButton.setBounds (row2.removeFromLeft (44).reduced (3, 4));
    row2.removeFromLeft (6);
    shiftRightButton.setBounds (row2.removeFromLeft (44).reduced (3, 4));
    row2.removeFromLeft (10);
    panicButton.setBounds (row2.removeFromLeft (80).reduced (3, 4));
    row2.removeFromLeft (14);
    presetBox.setBounds (row2.removeFromLeft (140).reduced (3, 4));
    row2.removeFromLeft (8);
    savePresetButton.setBounds (row2.removeFromLeft (62).reduced (3, 4));
    row2.removeFromLeft (6);
    openPresetFolderButton.setBounds (row2.removeFromLeft (70).reduced (3, 4));
    row2.removeFromLeft (6);
    logButton.setBounds (row2.removeFromLeft (56).reduced (3, 4));

    auto footer = getLocalBounds().removeFromBottom (footerHeight).reduced (16, 6);
    footer.removeFromRight (252);

    auto clipboardControls = footer.removeFromRight (356);
    auto setClipboardButton = [] (juce::Rectangle<int>& row, juce::Component& control, int width)
    {
        control.setBounds (row.removeFromLeft (width).reduced (3, 4));
        row.removeFromLeft (4);
    };

    setClipboardButton (clipboardControls, copyStepButton, 84);
    setClipboardButton (clipboardControls, pasteStepButton, 84);
    setClipboardButton (clipboardControls, copyPageButton, 84);
    setClipboardButton (clipboardControls, pastePageButton, 84);

    area.removeFromBottom (footerHeight);
    gridBounds = area.reduced (0, 8);
}

void PsytrancerAudioProcessorEditor::drawStepGrid (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (bounds.isEmpty())
        return;

    g.setColour (panel());
    g.fillRoundedRectangle (bounds.toFloat(), 8.0f);

    const auto labelWidth = 84;
    auto body = bounds.reduced (10);
    auto rowLabels = body.removeFromLeft (labelWidth);
    auto grid = body;
    const auto visibleSteps = getVisibleSteps();
    const auto cellWidth = juce::jmax (1, grid.getWidth() / visibleSteps);
    const auto startStep = page * visibleSteps;
    const auto playStep = processor.getCurrentStep();
    const auto sequenceLength = processor.getSequenceLength();
    const auto scale = getScaleDefinition (processor.getScaleType());

    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    const std::array<juce::String, 8> labels { "Step", "Pitch", "Octave", "Gate", "Cut", "Hold", "Length", "Velocity" };

    for (const auto& label : labels)
    {
        auto labelArea = rowLabels.removeFromTop (label == "Step" ? stepHeaderHeight : rowHeight);
        g.setColour (dimText());
        g.drawFittedText (label, labelArea.reduced (6), juce::Justification::centredRight, 1);
    }

    for (auto visible = 0; visible < visibleSteps; ++visible)
    {
        const auto stepIndex = startStep + visible;
        const auto x = grid.getX() + visible * cellWidth;
        auto column = juce::Rectangle<int> (x, grid.getY(), cellWidth - 2, stepHeaderHeight + rowHeight * 7);
        const auto active = stepIndex < sequenceLength;
        const auto isSelected = stepIndex == selectedStep;
        const auto isPlaying = stepIndex == playStep;

        g.setColour (isSelected ? selected().withAlpha (0.22f) : cell());
        g.fillRoundedRectangle (column.toFloat(), 5.0f);

        if (! active)
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (column.toFloat(), 5.0f);
        }

        if (isPlaying)
        {
            g.setColour (playhead());
            g.fillRect (column.removeFromTop (3));
        }

        auto step = processor.getStep (stepIndex);
        auto row = juce::Rectangle<int> (x, grid.getY(), cellWidth - 2, stepHeaderHeight);
        const auto stepEnabled = step.enabled;
        const auto valueTextColour = stepEnabled ? text() : text().withAlpha (0.38f);
        const auto valueDimColour = stepEnabled ? dimText() : dimText().withAlpha (0.36f);
        const auto accentColour = stepEnabled ? selected() : dimText().withAlpha (0.48f);

        if (! stepEnabled)
        {
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillRoundedRectangle (column.toFloat(), 5.0f);
        }

        g.setColour (isSelected ? accentColour : valueDimColour);
        g.setFont (13.0f);
        g.drawFittedText (juce::String (stepIndex + 1), row, juce::Justification::centred, 1);

        if (stepIndex == hoverStep && isStepToggleRow (hoverRow))
        {
            g.setColour (selected().withAlpha (0.45f));
            g.drawRoundedRectangle (row.reduced (4).toFloat(), 4.0f, 1.2f);
        }

        auto drawRow = [&] (const juce::String& value, juce::Colour colour, int rowIndex)
        {
            row.translate (0, row.getHeight());
            row.setHeight (rowHeight);

            if (isTypeEditRow (rowIndex))
            {
                const auto type = rowIndex == 2 ? StepType::gate
                                : rowIndex == 3 ? StepType::cut
                                                : StepType::hold;
                const auto typeIsEnabled = step.enabled && step.type == type;
                const auto toggleBounds = row.reduced (6, 5);

                g.setColour (typeIsEnabled ? juce::Colour (0xff9da3aa)
                                           : juce::Colour (0xff3b424c));
                g.fillRoundedRectangle (toggleBounds.toFloat(), 3.0f);
                g.setColour (typeIsEnabled ? juce::Colour (0xffc5cbd1)
                                           : juce::Colour (0xff59616b));
                g.drawRoundedRectangle (toggleBounds.toFloat(), 3.0f, 1.0f);
                return;
            }

            if (rowIndex == 0 || rowIndex == 1)
            {
                const auto scaleMaximum = juce::jmax (1, *(scale.offsets.end() - 1));
                const auto signedAmount = rowIndex == 0 ? (float) step.relativePitch / (float) scaleMaximum
                                                        : (float) step.octaveOffset / 4.0f;
                const auto clampedAmount = juce::jlimit (-1.0f, 1.0f, signedAmount);
                const auto barArea = row.reduced (5, 3);
                const auto halfHeight = barArea.getHeight() / 2;
                const auto barHeight = juce::roundToInt ((float) halfHeight * std::abs (clampedAmount));
                const auto centreY = barArea.getCentreY();
                const auto valueBar = clampedAmount >= 0.0f
                    ? juce::Rectangle<int> (barArea.getX(), centreY - barHeight, barArea.getWidth(), barHeight)
                    : juce::Rectangle<int> (barArea.getX(), centreY, barArea.getWidth(), barHeight);

                g.setColour (playhead().withAlpha (0.20f));
                g.fillRoundedRectangle (valueBar.toFloat(), 2.0f);
                g.setColour (playhead().withAlpha (0.30f));
                g.fillRect (barArea.getX(), centreY, barArea.getWidth(), 1);
            }

            if (rowIndex == 5 || rowIndex == 6)
            {
                const auto fillAmount = rowIndex == 5 ? (step.type == StepType::cut ? step.gateRate * 0.1f
                                                                                      : step.gateRate)
                                                       : (float) step.velocity / 127.0f;
                const auto barArea = row.reduced (5, 4);
                const auto barHeight = juce::roundToInt ((float) barArea.getHeight() * fillAmount);
                auto valueBar = barArea.withTop (barArea.getBottom() - barHeight);

                g.setColour (playhead().withAlpha (0.20f));
                g.fillRoundedRectangle (valueBar.toFloat(), 2.0f);
            }

            g.setColour (colour);
            g.setFont (rowHeight > 50 ? 18.0f : 15.0f);
            g.drawFittedText (value, row.reduced (3), juce::Justification::centred, 2);

            if (isStepParameterRow (rowIndex))
            {
                const auto isDragging = stepIndex == dragStep && rowIndex == dragRow;
                const auto isHovering = stepIndex == hoverStep && rowIndex == hoverRow;

                if (isDragging || isHovering)
                {
                    g.setColour (isDragging ? selected() : selected().withAlpha (0.45f));
                    g.drawRoundedRectangle (row.reduced (4).toFloat(), 4.0f, isDragging ? 2.2f : 1.2f);
                }
            }
        };

        const auto sounded = step.type == StepType::gate || step.type == StepType::cut;
        const auto muted = ! sounded || ! stepEnabled;
        const auto displayedGateRate = step.type == StepType::cut ? step.gateRate * 0.1f : step.gateRate;

        const auto showStoredValues = ! stepEnabled;
        const auto pitchTextColour = muted ? valueDimColour
                                           : step.relativePitch == 0 ? dimText().withAlpha (0.72f) : valueTextColour;
        const auto octaveTextColour = muted ? valueDimColour
                                            : step.octaveOffset == 0 ? dimText().withAlpha (0.72f) : valueTextColour;

        drawRow (showStoredValues || sounded ? juce::String (step.relativePitch) : "-", pitchTextColour, 0);
        drawRow (showStoredValues || sounded ? juce::String (step.octaveOffset) : "-",
                 octaveTextColour, 1);
        drawRow ({}, juce::Colours::transparentBlack, 2);
        drawRow ({}, juce::Colours::transparentBlack, 3);
        drawRow ({}, juce::Colours::transparentBlack, 4);
        drawRow (showStoredValues || sounded ? juce::String (juce::roundToInt (displayedGateRate * 100.0f)) : "-",
                 muted ? valueDimColour : valueTextColour, 5);
        drawRow (showStoredValues || sounded ? juce::String ((int) step.velocity) : "-",
                 muted ? valueDimColour : valueTextColour, 6);

        if (visible % 4 == 3)
        {
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRect (x + cellWidth - 2, grid.getY(), 1, grid.getHeight());
        }
    }
}

void PsytrancerAudioProcessorEditor::drawPageOverview (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto sequenceLength = processor.getSequenceLength();
    const auto pageCount = getPageCount();
    const auto playStep = processor.getCurrentStep();

    g.setColour (dimText());
    g.setFont (11.0f);
    g.drawFittedText ("Page " + juce::String (page + 1) + "/" + juce::String (pageCount),
                      bounds.removeFromTop (12), juce::Justification::centredLeft, 1);

    const auto pageGap = 4;
    const auto pageWidth = juce::jmax (18, (bounds.getWidth() - pageGap * 7) / 8);
    auto pages = bounds.reduced (0, 1);

    for (auto pageIndex = 0; pageIndex < 8; ++pageIndex)
    {
        auto pageArea = pages.removeFromLeft (pageWidth);
        pages.removeFromLeft (pageGap);

        const auto pageActive = pageIndex < pageCount;
        const auto visibleSteps = getVisibleSteps();
        const auto pagePlaying = playStep >= pageIndex * visibleSteps && playStep < (pageIndex + 1) * visibleSteps;
        g.setColour (pageActive ? cell() : juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (pageArea.toFloat(), 3.0f);
        g.setColour (juce::Colour (0xff3b424c));
        g.drawRoundedRectangle (pageArea.toFloat().reduced (0.5f), 3.0f, 1.0f);

        if (pagePlaying)
        {
            g.setColour (playhead());
            g.drawRoundedRectangle (pageArea.toFloat().reduced (1.0f), 3.0f, 2.0f);
        }

        if (pageIndex == page)
        {
            g.setColour (selected());
            g.drawRoundedRectangle (pageArea.toFloat().reduced (3.0f), 2.0f, 2.0f);
        }

        auto stepsArea = pageArea.reduced (3, 4);
        const auto tickWidth = juce::jmax (1, stepsArea.getWidth() / visibleSteps);

        for (auto stepOffset = 0; stepOffset < visibleSteps; ++stepOffset)
        {
            const auto stepIndex = pageIndex * visibleSteps + stepOffset;
            const auto step = processor.getStep (stepIndex);
            auto tick = juce::Rectangle<int> (stepsArea.getX() + stepOffset * tickWidth,
                                              stepsArea.getY(),
                                              juce::jmax (1, tickWidth - 1),
                                              stepsArea.getHeight());

            if (stepIndex >= sequenceLength)
                g.setColour (juce::Colours::black.withAlpha (0.45f));
            else if (! step.enabled)
                g.setColour (dimText().withAlpha (0.16f));
            else if (step.type == StepType::gate || step.type == StepType::cut)
                g.setColour (selected());
            else if (step.type == StepType::hold)
                g.setColour (selected().withAlpha (0.45f));
            else
                g.setColour (dimText().withAlpha (0.24f));

            g.fillRect (tick);
        }
    }
}

int PsytrancerAudioProcessorEditor::getPageAtOverviewPosition (juce::Point<int> position) const
{
    auto bounds = pageOverviewBounds;

    if (! bounds.contains (position))
        return -1;

    bounds.removeFromTop (12);
    const auto pageGap = 4;
    const auto pageWidth = juce::jmax (18, (bounds.getWidth() - pageGap * 7) / 8);
    auto pages = bounds.reduced (0, 1);

    for (auto pageIndex = 0; pageIndex < getPageCount(); ++pageIndex)
    {
        const auto pageArea = pages.removeFromLeft (pageWidth);
        pages.removeFromLeft (pageGap);

        if (pageArea.contains (position))
            return pageIndex;
    }

    return -1;
}

void PsytrancerAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    if (const auto clickedPage = getPageAtOverviewPosition (event.getPosition()); clickedPage >= 0)
    {
        followPlaybackToggle.setToggleState (false, juce::dontSendNotification);
        setPage (clickedPage);
        repaint();
        return;
    }

    dragStep = getStepAtX (event.x);
    dragRow = getGridRowAtY (event.y);
    lastDraggedToggleStep = -1;
    lastDraggedToggleRow = -1;
    dragStartY = event.y;
    hoverStep = dragStep;
    hoverRow = dragRow;

    if (dragStep >= 0 && dragStep < maxSequenceSteps)
    {
        const auto step = processor.getStep (dragStep);
        dragStartPitch = step.relativePitch;
        dragStartOctave = step.octaveOffset;
        dragStartGate = step.gateRate;
        dragStartVelocity = step.velocity;
    }
    else
    {
        dragStartPitch = 0;
        dragStartOctave = 0;
        dragStartGate = 1.0f;
        dragStartVelocity = 100;
    }

    if (event.mods.isMiddleButtonDown() && resetStepValueAt (dragStep, dragRow))
    {
        repaint();
        return;
    }

    editStepAt (event.getPosition(), false);

    if (gridBounds.contains (event.getPosition()) && isTypeEditRow (dragRow))
    {
        lastDraggedToggleStep = dragStep;
        lastDraggedToggleRow = dragRow;
    }
}

void PsytrancerAudioProcessorEditor::mouseDrag (const juce::MouseEvent& event)
{
    if (dragStep >= 0 && dragStep < maxSequenceSteps && isEditableValueRow (dragRow))
    {
        setSelectedStep (dragStep);

        auto stepData = processor.getStep (dragStep);
        const auto dragDelta = dragStartY - event.y;

        if (isPitchEditRow (dragRow))
            stepData.relativePitch = dragStartPitch + dragDelta / 8;
        else if (isOctaveEditRow (dragRow))
            stepData.octaveOffset = dragStartOctave + dragDelta / 24;
        else if (isGateEditRow (dragRow))
            stepData.gateRate = dragStartGate + (float) (dragDelta / 4) * 0.01f;
        else if (isVelocityEditRow (dragRow))
            stepData.velocity = (juce::uint8) juce::jlimit (1, 127, (int) dragStartVelocity + dragDelta / 2);

        processor.setStep (dragStep, stepData);
        repaint();
        return;
    }

    const auto draggedStep = getStepAtX (event.x);
    const auto draggedRow = getGridRowAtY (event.y);

    if (gridBounds.contains (event.getPosition()) && draggedStep >= 0 && draggedStep < maxSequenceSteps
        && isTypeEditRow (draggedRow))
    {
        if (draggedStep != lastDraggedToggleStep || draggedRow != lastDraggedToggleRow)
        {
            toggleStepTypeAt (draggedStep, draggedRow);
            lastDraggedToggleStep = draggedStep;
            lastDraggedToggleRow = draggedRow;
            repaint (gridBounds);
        }

        return;
    }

    editStepAt (event.getPosition(), true);
}

void PsytrancerAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    dragStep = -1;
    dragRow = -1;
    lastDraggedToggleStep = -1;
    lastDraggedToggleRow = -1;
    repaint (gridBounds);
}

void PsytrancerAudioProcessorEditor::mouseMove (const juce::MouseEvent& event)
{
    const auto newHoverStep = getStepAtX (event.x);
    const auto newHoverRow = getGridRowAtY (event.y);

    if (newHoverStep != hoverStep || newHoverRow != hoverRow)
    {
        hoverStep = newHoverStep;
        hoverRow = newHoverRow;
        repaint (gridBounds);
    }
}

void PsytrancerAudioProcessorEditor::mouseExit (const juce::MouseEvent&)
{
    if (hoverStep >= 0 || hoverRow >= 0)
    {
        hoverStep = -1;
        hoverRow = -1;
        repaint (gridBounds);
    }
}

void PsytrancerAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto row = getGridRowAtY (event.y);

    if (gridBounds.contains (event.getPosition()) && isEditableValueRow (row)
        && ! juce::approximatelyEqual (wheel.deltaY, 0.0f))
    {
        const auto step = getStepAtX (event.x);

        if (step >= 0 && step < maxSequenceSteps)
        {
            setSelectedStep (step);

            auto stepData = processor.getStep (step);
            const auto direction = wheel.deltaY > 0.0f ? 1 : -1;

            if (isPitchEditRow (row))
                stepData.relativePitch += direction;
            else if (isOctaveEditRow (row))
                stepData.octaveOffset += direction;
            else if (isGateEditRow (row))
                stepData.gateRate += (float) direction * 0.05f;
            else if (isVelocityEditRow (row))
                stepData.velocity = (juce::uint8) juce::jlimit (1, 127, (int) stepData.velocity + direction * 5);

            processor.setStep (step, stepData);
            repaint();
            return;
        }
    }

    juce::AudioProcessorEditor::mouseWheelMove (event, wheel);
}

void PsytrancerAudioProcessorEditor::editStepAt (juce::Point<int> position, bool drag)
{
    if (! gridBounds.contains (position))
        return;

    const auto step = getStepAtX (position.x);

    if (step < 0 || step >= maxSequenceSteps)
        return;

    setSelectedStep (step);

    if (drag)
        return;

    auto stepData = processor.getStep (step);
    const auto rowIndex = getGridRowAtY (position.y);

    if (isStepToggleRow (rowIndex))
    {
        stepData.enabled = ! stepData.enabled;
        processor.setStep (step, stepData);
    }
    else if (isTypeEditRow (rowIndex))
    {
        toggleStepTypeAt (step, rowIndex);
    }

    repaint();
}

void PsytrancerAudioProcessorEditor::toggleStepTypeAt (int step, int row)
{
    if (step < 0 || step >= maxSequenceSteps || ! isTypeEditRow (row))
        return;

    setSelectedStep (step);
    auto stepData = processor.getStep (step);
    const auto selectedType = row == 2 ? StepType::gate
                            : row == 3 ? StepType::cut
                                       : StepType::hold;

    if (stepData.enabled && stepData.type == selectedType)
        stepData.enabled = false;
    else
    {
        stepData.enabled = true;
        stepData.type = selectedType;
    }

    processor.setStep (step, stepData);
}

int PsytrancerAudioProcessorEditor::getStepAtX (int x) const
{
    const auto body = gridBounds.reduced (10);
    const auto gridX = body.getX() + 84;
    const auto gridWidth = body.getWidth() - 84;
    const auto visibleSteps = getVisibleSteps();
    const auto cellWidth = juce::jmax (1, gridWidth / visibleSteps);

    if (x < gridX)
        return -1;

    return page * visibleSteps + (x - gridX) / cellWidth;
}

int PsytrancerAudioProcessorEditor::getGridRowAtY (int y) const
{
    const auto localY = y - gridBounds.reduced (10).getY();

    if (localY < stepHeaderHeight)
        return -1;

    return (localY - stepHeaderHeight) / rowHeight;
}

bool PsytrancerAudioProcessorEditor::isPitchEditRow (int row) const
{
    return row == 0;
}

bool PsytrancerAudioProcessorEditor::isOctaveEditRow (int row) const
{
    return row == 1;
}

bool PsytrancerAudioProcessorEditor::isStepToggleRow (int row) const
{
    return row == -1;
}

bool PsytrancerAudioProcessorEditor::isTypeEditRow (int row) const
{
    return row >= 2 && row <= 4;
}

bool PsytrancerAudioProcessorEditor::isGateEditRow (int row) const
{
    return row == 5;
}

bool PsytrancerAudioProcessorEditor::isVelocityEditRow (int row) const
{
    return row == 6;
}

bool PsytrancerAudioProcessorEditor::isEditableValueRow (int row) const
{
    return isPitchEditRow (row) || isOctaveEditRow (row) || isGateEditRow (row) || isVelocityEditRow (row);
}

bool PsytrancerAudioProcessorEditor::isStepParameterRow (int row) const
{
    return isEditableValueRow (row) || isTypeEditRow (row);
}

bool PsytrancerAudioProcessorEditor::resetStepValueAt (int step, int row)
{
    if (step < 0 || step >= maxSequenceSteps || ! isStepParameterRow (row))
        return false;

    setSelectedStep (step);

    auto stepData = processor.getStep (step);

    if (isPitchEditRow (row))
        stepData.relativePitch = 0;
    else if (isOctaveEditRow (row))
        stepData.octaveOffset = 0;
    else if (isTypeEditRow (row))
        stepData.type = StepType::gate;
    else if (isGateEditRow (row))
        stepData.gateRate = 1.0f;
    else if (isVelocityEditRow (row))
        stepData.velocity = 100;

    processor.setStep (step, stepData);
    return true;
}

int PsytrancerAudioProcessorEditor::getPageCount() const
{
    return processor.getPageCount();
}

void PsytrancerAudioProcessorEditor::setPage (int newPage)
{
    page = juce::jlimit (0, getPageCount() - 1, newPage);
    const auto visibleSteps = getVisibleSteps();
    const auto offset = selectedStep % visibleSteps;
    const auto maxStep = processor.getSequenceLength() - 1;
    selectedStep = juce::jlimit (0, maxStep, page * visibleSteps + offset);
    repaint();
}

void PsytrancerAudioProcessorEditor::setSelectedStep (int step)
{
    selectedStep = juce::jlimit (0, processor.getSequenceLength() - 1, step);
    updatePageForSelection();
}

void PsytrancerAudioProcessorEditor::updatePageForSelection()
{
    page = juce::jlimit (0, getPageCount() - 1, selectedStep / getVisibleSteps());
}

void PsytrancerAudioProcessorEditor::updateRootOctaveControls()
{
    const auto enabled = ! midiKeyToggle.getToggleState();
    rootBox.setEnabled (enabled);
    octaveSlider.setEnabled (enabled);
}

void PsytrancerAudioProcessorEditor::refreshPresetList (const juce::String& selectedName)
{
    const juce::ScopedValueSetter<bool> scopedSetter (refreshingPresetList, true);
    const auto currentText = selectedName.isNotEmpty() ? selectedName : presetBox.getText();
    const auto names = processor.getPresetNames();

    presetBox.clear (juce::dontSendNotification);

    for (auto i = 0; i < names.size(); ++i)
        presetBox.addItem (names[i], i + 1);

    if (currentText.isNotEmpty())
        presetBox.setText (currentText, juce::dontSendNotification);
}

void PsytrancerAudioProcessorEditor::saveCurrentPreset()
{
    const auto presetName = presetBox.getText().trim();

    if (presetName.isEmpty())
    {
        appendLogMessage ("Preset save was not started: enter a name in the Preset field first.");
        showLogWindow();
        return;
    }

    if (processor.savePreset (presetName))
    {
        auto fileName = juce::File::createLegalFileName (presetName);

        if (fileName.endsWithIgnoreCase (".psytrancerpreset"))
            fileName = fileName.upToLastOccurrenceOf (".psytrancerpreset", false, true);

        if (fileName.isEmpty())
            fileName = "Untitled";

        const auto file = processor.getPresetDirectory().getChildFile (fileName + ".psytrancerpreset");
        appendLogMessage ("Preset saved successfully: " + file.getFullPathName());
        refreshPresetList (presetName);
    }
    else
    {
        appendLogMessage ("Preset save failed: " + processor.getLastPresetError());
        showLogWindow();
    }
}

void PsytrancerAudioProcessorEditor::appendLogMessage (const juce::String& message)
{
    logMessages << "[" << juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S") << "] "
                << message << "\n\n";
}

void PsytrancerAudioProcessorEditor::showLogWindow()
{
    auto* editor = new juce::TextEditor();
    editor->setMultiLine (true);
    editor->setReadOnly (true);
    editor->setScrollbarsShown (true);
    editor->setText (logMessages.isNotEmpty() ? logMessages : "No errors have been recorded.");
    editor->setColour (juce::TextEditor::backgroundColourId, background());
    editor->setColour (juce::TextEditor::textColourId, text());
    editor->setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3b424c));
    editor->setSize (620, 260);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Psytrancer Log";
    options.dialogBackgroundColour = background();
    options.content.setOwned (editor);
    options.componentToCentreAround = this;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

void PsytrancerAudioProcessorEditor::loadSelectedPreset()
{
    if (refreshingPresetList || presetBox.getSelectedId() <= 0)
        return;

    if (processor.loadPreset (presetBox.getText()))
    {
        selectedStep = juce::jlimit (0, processor.getSequenceLength() - 1, selectedStep);
        updatePageForSelection();
        repaint();
    }
}

void PsytrancerAudioProcessorEditor::openPresetFolder()
{
    const auto directory = processor.getPresetDirectory();

    if (! directory.createDirectory())
    {
        appendLogMessage ("Could not create the preset folder:\n" + directory.getFullPathName());
        showLogWindow();
        return;
    }

    directory.revealToUser();
}

void PsytrancerAudioProcessorEditor::copySelectedStep()
{
    copiedStep = processor.getStep (selectedStep);
    hasCopiedStep = true;
    pasteStepButton.setEnabled (true);
}

void PsytrancerAudioProcessorEditor::pasteSelectedStep()
{
    if (! hasCopiedStep)
        return;

    processor.setStep (selectedStep, copiedStep);
    processor.panic();
    repaint (gridBounds);
}

void PsytrancerAudioProcessorEditor::copyCurrentPage()
{
    const auto visibleSteps = getVisibleSteps();
    const auto firstStep = page * visibleSteps;

    for (auto i = 0; i < visibleSteps; ++i)
        copiedPage[(size_t) i] = processor.getStep (firstStep + i);

    hasCopiedPage = true;
    pastePageButton.setEnabled (true);
}

void PsytrancerAudioProcessorEditor::pasteCurrentPage()
{
    if (! hasCopiedPage)
        return;

    const auto visibleSteps = getVisibleSteps();
    const auto firstStep = page * visibleSteps;

    for (auto i = 0; i < visibleSteps; ++i)
        processor.setStep (firstStep + i, copiedPage[(size_t) i]);

    processor.panic();
    repaint (gridBounds);
}

bool PsytrancerAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    auto step = processor.getStep (selectedStep);
    const auto keyCode = key.getKeyCode();

    if (keyCode == juce::KeyPress::leftKey) setSelectedStep (selectedStep - 1);
    else if (keyCode == juce::KeyPress::rightKey) setSelectedStep (selectedStep + 1);
    else if (keyCode == juce::KeyPress::pageUpKey) setSelectedStep (selectedStep - getVisibleSteps());
    else if (keyCode == juce::KeyPress::pageDownKey) setSelectedStep (selectedStep + getVisibleSteps());
    else if (keyCode == juce::KeyPress::homeKey) setSelectedStep (0);
    else if (keyCode == juce::KeyPress::endKey) setSelectedStep (processor.getSequenceLength() - 1);
    else if (keyCode == juce::KeyPress::upKey)
    {
        const auto scaleSize = (int) getScaleDefinition (processor.getScaleType()).offsets.size();
        step.relativePitch += key.getModifiers().isShiftDown() ? scaleSize : 1;
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == juce::KeyPress::downKey)
    {
        const auto scaleSize = (int) getScaleDefinition (processor.getScaleType()).offsets.size();
        step.relativePitch -= key.getModifiers().isShiftDown() ? scaleSize : 1;
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == ' ')
    {
        return false;
    }
    else if (keyCode == 'g' || keyCode == 'G')
    {
        step.type = StepType::gate;
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == 'c' || keyCode == 'C')
    {
        step.type = StepType::cut;
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == 'h' || keyCode == 'H')
    {
        step.type = StepType::hold;
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == juce::KeyPress::deleteKey)
    {
        step.enabled = false;
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == '+' || keyCode == '=')
    {
        step.gateRate = juce::jmin (1.0f, step.gateRate + 0.05f);
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == '-')
    {
        step.gateRate = juce::jmax (0.01f, step.gateRate - 0.05f);
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == ']')
    {
        step.velocity = (juce::uint8) juce::jmin (127, (int) step.velocity + 5);
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == '[')
    {
        step.velocity = (juce::uint8) juce::jmax (1, (int) step.velocity - 5);
        processor.setStep (selectedStep, step);
    }
    else
    {
        return false;
    }

    repaint();
    return true;
}

void PsytrancerAudioProcessorEditor::timerCallback()
{
    if (followPlaybackToggle.getToggleState())
    {
        const auto playStep = processor.getCurrentStep();

        if (playStep >= 0)
            setPage (playStep / getVisibleSteps());
    }

    updateRootOctaveControls();
    repaint (gridBounds);
    repaint (getLocalBounds().removeFromBottom (footerHeight));
}
