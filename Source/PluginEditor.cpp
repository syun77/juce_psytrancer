#include "PluginEditor.h"

namespace
{
constexpr auto headerHeight = 96;
constexpr auto footerHeight = 44;
constexpr auto stepHeaderHeight = 30;
constexpr auto rowHeight = 58;

juce::Colour background() { return juce::Colour (0xff17191d); }
juce::Colour panel() { return juce::Colour (0xff22262c); }
juce::Colour cell() { return juce::Colour (0xff2b3037); }
juce::Colour selected() { return juce::Colour (0xffe6c15a); }
juce::Colour playhead() { return juce::Colour (0xff59c0d8); }
juce::Colour text() { return juce::Colour (0xffe8ecef); }
juce::Colour dimText() { return juce::Colour (0xff9aa4ad); }

void drawFieldLabel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label)
{
    g.setColour (dimText());
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
    setSize (1080, 450);
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

    addCombo (resolutionBox);
    addCombo (rootBox);
    addCombo (scaleBox);
    addCombo (presetBox);
    presetBox.setEditableText (true);
    presetBox.setTextWhenNothingSelected ("Preset");

    setComboItems (resolutionBox, { "1/4", "1/8", "1/16", "1/32", "1/64", "1/8T", "1/16T", "1/32T", "1/8D", "1/16D" });
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

    addSlider (lengthSlider);
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

    for (auto* button : { &lengthDownButton, &lengthUpButton, &prevPageButton, &nextPageButton, &savePresetButton,
                          &initButton, &repeatButton, &shiftLeftButton, &shiftRightButton, &panicButton })
    {
        addAndMakeVisible (*button);
        button->setColour (juce::TextButton::buttonColourId, cell());
        button->setColour (juce::TextButton::textColourOffId, text());
    }

    addAndMakeVisible (pageMapToggle);
    pageMapToggle.setToggleState (true, juce::dontSendNotification);
    pageMapToggle.setColour (juce::ToggleButton::textColourId, text());
    pageMapToggle.setColour (juce::ToggleButton::tickColourId, selected());
    pageMapToggle.setColour (juce::ToggleButton::tickDisabledColourId, dimText());

    addAndMakeVisible (midiKeyToggle);
    midiKeyToggle.setColour (juce::ToggleButton::textColourId, text());
    midiKeyToggle.setColour (juce::ToggleButton::tickColourId, selected());
    midiKeyToggle.setColour (juce::ToggleButton::tickDisabledColourId, dimText());

    lengthDownButton.onClick = [this] { changeLengthBy (-visibleSteps); };
    lengthUpButton.onClick = [this] { changeLengthBy (visibleSteps); };
    prevPageButton.onClick = [this] { setPage (page - 1); };
    nextPageButton.onClick = [this] { setPage (page + 1); };
    pageMapToggle.onClick = [this] { repaint(); };
    presetBox.onChange = [this] { loadSelectedPreset(); };
    savePresetButton.onClick = [this] { saveCurrentPreset(); };

    initButton.onClick = [this]
    {
        processor.clearPattern();
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
    lengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (parameters, "length", lengthSlider);
    octaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (parameters, "octave", octaveSlider);
    gateMultiplierAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (parameters, "gateMultiplier", gateMultiplierSlider);
    midiKeyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters, "midiKey", midiKeyToggle);

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
        { &lengthSlider, "Length" },
        { &resolutionBox, "Rate" },
        { &rootBox, "Root" },
        { &octaveSlider, "Octave" },
        { &gateMultiplierSlider, "Gate Mult" },
        { &scaleBox, "Scale" }
    }};

    for (const auto& item : labelledControls)
    {
        auto labelArea = item.first->getBounds().translated (0, -18).withHeight (16);
        drawFieldLabel (g, labelArea, item.second);
    }

    drawStepGrid (g, gridBounds);

    auto footer = getLocalBounds().removeFromBottom (footerHeight).reduced (16, 6);
    g.setColour (panel());
    g.fillRoundedRectangle (footer.toFloat(), 6.0f);

    const auto overviewWidth = pageMapToggle.getToggleState() ? 252 : 0;
    auto overview = overviewWidth > 0 ? footer.removeFromRight (overviewWidth).reduced (8, 3)
                                      : juce::Rectangle<int>();

    g.setColour (dimText());
    g.setFont (14.0f);
    g.drawFittedText ("Page " + juce::String (page + 1) + "/" + juce::String (getPageCount()) + "   Selected Step "
                          + juce::String (selectedStep + 1)
                          + "   Length +/-16, Page << >>, Space, G/H/R",
                      footer.reduced (12, 0), juce::Justification::centredLeft, 1);

    if (! overview.isEmpty())
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

    setControl (row1, lengthSlider, 112);
    setControl (row1, lengthDownButton, 44);
    setControl (row1, lengthUpButton, 44);
    setControl (row1, resolutionBox, 86);
    setControl (row1, rootBox, 70);
    setControl (row1, octaveSlider, 86);
    setControl (row1, midiKeyToggle, 96);
    gateMultiplierSlider.setBounds (row1.removeFromLeft (82).reduced (8, 0));
    row1.removeFromLeft (8);
    setControl (row1, scaleBox, 150);

    auto row2 = top.removeFromTop (34);
    prevPageButton.setBounds (row2.removeFromLeft (46).reduced (3, 4));
    row2.removeFromLeft (10);
    nextPageButton.setBounds (row2.removeFromLeft (46).reduced (3, 4));
    row2.removeFromLeft (10);
    pageMapToggle.setBounds (row2.removeFromLeft (74).reduced (3, 2));
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
    presetBox.setBounds (row2.removeFromLeft (160).reduced (3, 4));
    row2.removeFromLeft (8);
    savePresetButton.setBounds (row2.removeFromLeft (62).reduced (3, 4));

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
    const auto cellWidth = juce::jmax (32, grid.getWidth() / visibleSteps);
    const auto startStep = page * visibleSteps;
    const auto playStep = processor.getCurrentStep();
    const auto sequenceLength = processor.getSequenceLength();
    const auto baseNote = processor.getBaseRootMidiNote();
    const auto scale = getScaleDefinition (processor.getScaleType());

    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    const std::array<juce::String, 6> labels { "Step", "Pitch", "Note", "Type", "Gate", "Velocity" };

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
        auto column = juce::Rectangle<int> (x, grid.getY(), cellWidth - 2, stepHeaderHeight + rowHeight * 5);
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

        g.setColour (isSelected ? selected() : dimText());
        g.setFont (13.0f);
        g.drawFittedText (juce::String (stepIndex + 1), row, juce::Justification::centred, 1);

        auto drawRow = [&] (const juce::String& value, juce::Colour colour)
        {
            row.translate (0, row.getHeight());
            row.setHeight (rowHeight);
            g.setColour (colour);
            g.setFont (rowHeight > 50 ? 18.0f : 15.0f);
            g.drawFittedText (value, row.reduced (3), juce::Justification::centred, 2);
        };

        const auto note = relativePitchToMidiNote (baseNote, step.relativePitch, scale);
        const auto muted = step.type != StepType::gate;
        drawRow (step.type == StepType::hold ? "-" : step.type == StepType::rest ? "-" : juce::String (step.relativePitch),
                 muted ? dimText() : text());
        drawRow (step.type == StepType::gate ? midiNoteName (note) : "-", muted ? dimText() : text());
        drawRow (getStepTypeName (step.type), step.type == StepType::rest ? dimText() : selected());
        drawRow (step.type == StepType::gate ? juce::String (juce::roundToInt (step.gateRate * 100.0f)) + "%" : "-",
                 step.type == StepType::gate ? text() : dimText());
        drawRow (step.type == StepType::gate ? juce::String ((int) step.velocity) : "-",
                 step.type == StepType::gate ? text() : dimText());

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
        const auto pagePlaying = playStep >= pageIndex * visibleSteps && playStep < (pageIndex + 1) * visibleSteps;
        const auto outline = pageIndex == page ? selected() : pagePlaying ? playhead() : juce::Colour (0xff3b424c);

        g.setColour (pageActive ? cell() : juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (pageArea.toFloat(), 3.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (pageArea.toFloat().reduced (0.5f), 3.0f, pageIndex == page || pagePlaying ? 1.5f : 1.0f);

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
            else if (step.type == StepType::gate)
                g.setColour (selected());
            else if (step.type == StepType::hold)
                g.setColour (selected().withAlpha (0.45f));
            else
                g.setColour (dimText().withAlpha (0.24f));

            g.fillRect (tick);
        }
    }
}

void PsytrancerAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    dragStep = getStepAtX (event.x);
    dragRow = getGridRowAtY (event.y);
    dragStartY = event.y;
    dragStartPitch = dragStep >= 0 && dragStep < 128 ? processor.getStep (dragStep).relativePitch : 0;

    editStepAt (event.getPosition(), false);
}

void PsytrancerAudioProcessorEditor::mouseDrag (const juce::MouseEvent& event)
{
    if (dragStep >= 0 && dragStep < 128 && isPitchEditRow (dragRow))
    {
        setSelectedStep (dragStep);

        auto stepData = processor.getStep (dragStep);
        const auto pitchDelta = (dragStartY - event.y) / 8;
        stepData.relativePitch = dragStartPitch + pitchDelta;
        stepData.type = StepType::gate;
        processor.setStep (dragStep, stepData);
        repaint();
        return;
    }

    editStepAt (event.getPosition(), true);
}

void PsytrancerAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (gridBounds.contains (event.getPosition()) && isPitchEditRow (getGridRowAtY (event.y))
        && ! juce::approximatelyEqual (wheel.deltaY, 0.0f))
    {
        const auto step = getStepAtX (event.x);

        if (step >= 0 && step < 128)
        {
            setSelectedStep (step);

            auto stepData = processor.getStep (step);
            stepData.relativePitch += wheel.deltaY > 0.0f ? 1 : -1;
            stepData.type = StepType::gate;
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

    if (step < 0 || step >= 128)
        return;

    setSelectedStep (step);

    if (drag)
        return;

    auto stepData = processor.getStep (step);
    const auto rowIndex = getGridRowAtY (position.y);

    if (isPitchEditRow (rowIndex))
    {
        stepData.type = StepType::gate;
        processor.setStep (step, stepData);
    }
    else if (rowIndex == 2)
    {
        stepData.type = stepData.type == StepType::gate ? StepType::hold
                      : stepData.type == StepType::hold ? StepType::rest
                                                        : StepType::gate;
        processor.setStep (step, stepData);
    }

    repaint();
}

int PsytrancerAudioProcessorEditor::getStepAtX (int x) const
{
    const auto body = gridBounds.reduced (10);
    const auto gridX = body.getX() + 84;
    const auto gridWidth = body.getWidth() - 84;
    const auto cellWidth = juce::jmax (32, gridWidth / visibleSteps);

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
    return row == 0 || row == 1;
}

int PsytrancerAudioProcessorEditor::getPageCount() const
{
    return juce::jlimit (1, 8, (processor.getSequenceLength() + visibleSteps - 1) / visibleSteps);
}

void PsytrancerAudioProcessorEditor::setPage (int newPage)
{
    page = juce::jlimit (0, getPageCount() - 1, newPage);
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
    page = juce::jlimit (0, getPageCount() - 1, selectedStep / visibleSteps);
}

void PsytrancerAudioProcessorEditor::changeLengthBy (int amount)
{
    if (auto* parameter = parameters.getParameter ("length"))
    {
        const auto newLength = juce::jlimit (1, 128, processor.getSequenceLength() + amount);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) newLength));
        parameter->endChangeGesture();

        selectedStep = juce::jlimit (0, newLength - 1, selectedStep);
        updatePageForSelection();
        repaint();
    }
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
        return;

    if (processor.savePreset (presetName))
        refreshPresetList (presetName);
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

bool PsytrancerAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    auto step = processor.getStep (selectedStep);
    const auto keyCode = key.getKeyCode();

    if (keyCode == juce::KeyPress::leftKey) setSelectedStep (selectedStep - 1);
    else if (keyCode == juce::KeyPress::rightKey) setSelectedStep (selectedStep + 1);
    else if (keyCode == juce::KeyPress::pageUpKey) setSelectedStep (selectedStep - visibleSteps);
    else if (keyCode == juce::KeyPress::pageDownKey) setSelectedStep (selectedStep + visibleSteps);
    else if (keyCode == juce::KeyPress::homeKey) setSelectedStep (0);
    else if (keyCode == juce::KeyPress::endKey) setSelectedStep (processor.getSequenceLength() - 1);
    else if (keyCode == juce::KeyPress::upKey)
    {
        const auto scaleSize = (int) getScaleDefinition (processor.getScaleType()).offsets.size();
        step.relativePitch += key.getModifiers().isShiftDown() ? scaleSize : 1;
        step.type = StepType::gate;
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == juce::KeyPress::downKey)
    {
        const auto scaleSize = (int) getScaleDefinition (processor.getScaleType()).offsets.size();
        step.relativePitch -= key.getModifiers().isShiftDown() ? scaleSize : 1;
        step.type = StepType::gate;
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == ' ' || keyCode == 'g' || keyCode == 'G')
    {
        step.type = step.type == StepType::gate && keyCode == ' ' ? StepType::rest : StepType::gate;
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == 'h' || keyCode == 'H')
    {
        step.type = StepType::hold;
        processor.setStep (selectedStep, step);
    }
    else if (keyCode == 'r' || keyCode == 'R' || keyCode == juce::KeyPress::deleteKey)
    {
        step.type = StepType::rest;
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
    repaint (gridBounds);
}
