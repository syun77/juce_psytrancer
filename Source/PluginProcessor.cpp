#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::StringArray makeChoiceList (std::initializer_list<const char*> names)
{
    juce::StringArray result;

    for (auto* name : names)
        result.add (name);

    return result;
}

juce::String stepTypeToString (StepType type)
{
    switch (type)
    {
        case StepType::gate: return "gate";
        case StepType::cut: return "cut";
        case StepType::hold: return "hold";
    }

    return "gate";
}

StepType stepTypeFromString (const juce::String& value)
{
    if (value == "cut")
        return StepType::cut;

    if (value == "hold")
        return StepType::hold;

    return StepType::gate;
}

juce::ValueTree makeParameterOnlyState (const juce::ValueTree& source)
{
    juce::ValueTree result { "PARAMETERS" };

    if (! source.isValid())
        return result;

    for (const auto& child : source)
        if (child.hasType ("PARAM"))
            result.appendChild (child.createCopy(), nullptr);

    return result;
}

juce::ValueTree getParameterStateFromRoot (const juce::ValueTree& root)
{
    if (root.hasType ("PSYTRANCER_STATE"))
        return root.getChildWithName ("PARAMETERS");

    return root;
}

juce::ValueTree getStepStateFromRoot (const juce::ValueTree& root)
{
    if (root.hasType ("PSYTRANCER_STATE"))
        return root.getChildWithName ("STEPS");

    return root.getChildWithName ("STEPS");
}

juce::String sanitisePresetName (const juce::String& name)
{
    auto result = juce::File::createLegalFileName (name.trim());

    if (result.endsWithIgnoreCase (".psytrancerpreset"))
        result = result.upToLastOccurrenceOf (".psytrancerpreset", false, true);

    return result.isNotEmpty() ? result : "Untitled";
}
}

PsytrancerAudioProcessor::PsytrancerAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    clearPattern();
}

juce::AudioProcessorValueTreeState::ParameterLayout PsytrancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "length", 1 }, "Length", 1, 8, 1));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "resolution", 1 }, "Rate",
        makeChoiceList ({ "1/8", "1/16", "1/32", "1/8T", "1/16T", "1/32T", "1/8D", "1/16D" }), 1));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "root", 1 }, "Root",
        makeChoiceList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }), 0));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "octave", 1 }, "Octave", 0, 8, 4));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "midiKey", 1 }, "MIDI Key", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gateMultiplier", 1 }, "Gate Mult",
        juce::NormalisableRange<float> { 0.01f, 2.0f, 0.01f }, 1.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (juce::roundToInt (value * 100.0f)) + "%";
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.").getFloatValue() / 100.0f;
            })));

    juce::StringArray scaleNames;
    for (const auto& scale : getScaleDefinitions())
        scaleNames.add (scale.name);

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "scale", 1 }, "Scale", scaleNames, (int) ScaleType::naturalMinor));

    return { params.begin(), params.end() };
}

void PsytrancerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    resetTransportState();
}

void PsytrancerAudioProcessor::releaseResources()
{
    resetTransportState();
}

bool PsytrancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    return input.isDisabled()
        && (output == juce::AudioChannelSet::mono()
            || output == juce::AudioChannelSet::stereo());
}

void PsytrancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    processAudioBlock (buffer, midi);
}

void PsytrancerAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    processAudioBlock (buffer, midi);
}

template <typename FloatType>
void PsytrancerAudioProcessor::processAudioBlock (juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();

    juce::MidiBuffer generated;

    if (panicRequested.exchange (false))
    {
        sendActiveNoteOff (generated, 0);
        generated.addEvent (juce::MidiMessage::allNotesOff (activeChannel), 0);
        generated.addEvent (juce::MidiMessage::allSoundOff (activeChannel), 0);
        midiTriggerActive = false;
        triggerNote = -1;
    }

    auto* hostPlayHead = getPlayHead();
    const auto position = hostPlayHead != nullptr ? hostPlayHead->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo> {};

    const auto isPlaying = position.hasValue() && position->getIsPlaying();
    const auto ppq = position.hasValue() ? position->getPpqPosition().orFallback (previousPpq) : previousPpq;
    const auto bpm = position.hasValue() ? position->getBpm().orFallback (120.0) : 120.0;
    const auto numSamples = buffer.getNumSamples();

    if (! isPlaying || bpm <= 0.0)
    {
        if (wasPlaying)
            sendActiveNoteOff (generated, 0);

        wasPlaying = false;

        auto segmentStart = 0;

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            const auto eventSample = juce::jlimit (0, numSamples, metadata.samplePosition);

            if (midiTriggerActive && eventSample > segmentStart)
            {
                renderSequenceSegment (generated, segmentStart, eventSample - segmentStart,
                                       triggerPpq, bpm, getBaseRootMidiNote());
                triggerPpq += (double) (eventSample - segmentStart) / (currentSampleRate * 60.0 / bpm);
            }

            if (message.isNoteOn())
            {
                midiKeyNote.store (message.getNoteNumber());
                sendActiveNoteOff (generated, eventSample);
                midiTriggerActive = true;
                triggerNote = message.getNoteNumber();
                triggerChannel = message.getChannel();
                activeChannel = triggerChannel;
                triggerPpq = 0.0;
                currentStep.store (-1);
            }
            else if (message.isNoteOff() && midiTriggerActive
                     && message.getNoteNumber() == triggerNote
                     && message.getChannel() == triggerChannel)
            {
                sendActiveNoteOff (generated, eventSample);
                midiTriggerActive = false;
                triggerNote = -1;
                pendingNoteOffPpq = -1.0;
                currentStep.store (-1);
            }

            segmentStart = eventSample;
        }

        if (midiTriggerActive && segmentStart < numSamples)
        {
            renderSequenceSegment (generated, segmentStart, numSamples - segmentStart,
                                   triggerPpq, bpm, getBaseRootMidiNote());
            triggerPpq += (double) (numSamples - segmentStart) / (currentSampleRate * 60.0 / bpm);
        }

        midi.swapWith (generated);
        return;
    }

    if (isMidiKeyEnabled())
    {
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();

            if (message.isNoteOn())
                midiKeyNote.store (message.getNoteNumber());
        }
    }

    if (midiTriggerActive)
    {
        sendActiveNoteOff (generated, 0);
        midiTriggerActive = false;
        triggerNote = -1;
        triggerPpq = 0.0;
    }

    const auto quarterNoteSamples = currentSampleRate * 60.0 / bpm;
    const auto blockLengthPpq = (double) numSamples / quarterNoteSamples;
    const auto expectedPpq = previousPpq + blockLengthPpq;
    const auto transportJumped = wasPlaying
        && (ppq + 0.0001 < previousPpq || std::abs (ppq - expectedPpq) > 0.25);
    const auto triggerCurrentStepAtStart = ! wasPlaying || transportJumped;

    if (transportJumped)
        sendActiveNoteOff (generated, 0);

    wasPlaying = true;
    activeChannel = 1;
    renderSequenceSegment (generated, 0, numSamples, ppq, bpm, getBaseRootMidiNote(), triggerCurrentStepAtStart);
    previousPpq = ppq;
    midi.swapWith (generated);
}

void PsytrancerAudioProcessor::renderSequenceSegment (juce::MidiBuffer& generated, int segmentStart,
                                                      int segmentLength, double ppq, double bpm,
                                                      int baseNote, bool triggerCurrentStepAtStart)
{
    if (segmentLength <= 0 || bpm <= 0.0)
        return;

    const auto quarterNoteSamples = currentSampleRate * 60.0 / bpm;
    const auto blockEndPpq = ppq + (double) segmentLength / quarterNoteSamples;
    const auto numSamples = segmentLength;

    if (pendingNoteOffPpq >= ppq && pendingNoteOffPpq < blockEndPpq)
    {
        const auto sampleOffset = juce::jlimit (0, numSamples - 1,
                                                (int) std::round ((pendingNoteOffPpq - ppq) * quarterNoteSamples));
        sendActiveNoteOff (generated, segmentStart + sampleOffset);
    }

    const auto stepLengthPpq = getStepLengthPpq (getResolution());
    const auto sequenceLength = getSequenceLength();
    auto firstStep = (int64_t) std::ceil ((ppq - 1.0e-9) / stepLengthPpq);

    if (triggerCurrentStepAtStart)
        firstStep = (int64_t) std::floor ((ppq + 1.0e-9) / stepLengthPpq);

    auto lastStep = (int64_t) std::floor ((blockEndPpq - 1.0e-9) / stepLengthPpq);

    const auto scale = getScaleDefinition (getScaleType());
    const auto gateMultiplier = getGateMultiplier();

    for (auto absoluteStep = firstStep; absoluteStep <= lastStep; ++absoluteStep)
    {
        const auto boundaryPpq = (double) absoluteStep * stepLengthPpq;
        const auto sampleOffset = juce::jlimit (0, numSamples - 1,
                                                (int) std::round ((boundaryPpq - ppq) * quarterNoteSamples));
        const auto outputSample = segmentStart + sampleOffset;
        const auto stepIndex = positiveModulo ((int) absoluteStep, sequenceLength);
        currentStep.store (stepIndex);

        StepData step;
        {
            const juce::ScopedLock lock (stepLock);
            step = steps[(size_t) stepIndex];
        }

        if (! step.enabled)
        {
            sendActiveNoteOff (generated, outputSample);
        }
        else if (step.type == StepType::gate || step.type == StepType::cut)
        {
            sendActiveNoteOff (generated, outputSample);

            const auto note = juce::jlimit (0, 127,
                                            relativePitchToMidiNote (baseNote, step.relativePitch, scale)
                                                + step.octaveOffset * 12);
            activeNote = note;
            const auto typeGateMultiplier = step.type == StepType::cut ? 0.1f : 1.0f;
            const auto effectiveGateRate = juce::jlimit (0.01f, 1.0f, step.gateRate * gateMultiplier * typeGateMultiplier);
            pendingNoteOffPpq = boundaryPpq + stepLengthPpq * effectiveGateRate;
            generated.addEvent (juce::MidiMessage::noteOn (activeChannel, note, (juce::uint8) step.velocity), outputSample);
        }
    }

    if (pendingNoteOffPpq >= ppq && pendingNoteOffPpq < blockEndPpq)
    {
        const auto sampleOffset = juce::jlimit (0, numSamples - 1,
                                                (int) std::round ((pendingNoteOffPpq - ppq) * quarterNoteSamples));
        sendActiveNoteOff (generated, segmentStart + sampleOffset);
    }
}

void PsytrancerAudioProcessor::sendActiveNoteOff (juce::MidiBuffer& midi, int sampleOffset)
{
    if (activeNote >= 0)
        midi.addEvent (juce::MidiMessage::noteOff (activeChannel, activeNote), sampleOffset);

    activeNote = -1;
    pendingNoteOffPpq = -1.0;
}

void PsytrancerAudioProcessor::resetTransportState()
{
    previousPpq = 0.0;
    triggerPpq = 0.0;
    pendingNoteOffPpq = -1.0;
    activeNote = -1;
    triggerNote = -1;
    midiTriggerActive = false;
    currentStep.store (-1);
    wasPlaying = false;
}

juce::AudioProcessorEditor* PsytrancerAudioProcessor::createEditor()
{
    return new PsytrancerAudioProcessorEditor (*this);
}

StepData PsytrancerAudioProcessor::getStep (int index) const
{
    const juce::ScopedLock lock (stepLock);
    return steps[(size_t) juce::jlimit (0, maxSequenceSteps - 1, index)];
}

void PsytrancerAudioProcessor::setStep (int index, StepData step)
{
    const auto safeIndex = juce::jlimit (0, maxSequenceSteps - 1, index);
    step.relativePitch = juce::jlimit (-12, 12, step.relativePitch);
    step.octaveOffset = juce::jlimit (-4, 4, step.octaveOffset);
    step.gateRate = juce::jlimit (0.01f, 1.0f, step.gateRate);
    step.velocity = (juce::uint8) juce::jlimit (1, 127, (int) step.velocity);

    const juce::ScopedLock lock (stepLock);
    steps[(size_t) safeIndex] = step;
}

void PsytrancerAudioProcessor::clearPattern()
{
    const juce::ScopedLock lock (stepLock);

    for (auto& step : steps)
        step = {};

    for (auto i = 0; i < (int) steps.size(); ++i)
    {
        steps[(size_t) i].enabled = i % 4 == 0;
        steps[(size_t) i].type = StepType::gate;
    }
}

void PsytrancerAudioProcessor::resetToInitialPattern()
{
    if (auto* parameter = parameters.getParameter ("length"))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (1.0f));
        parameter->endChangeGesture();
    }

    clearPattern();
    currentPresetName.clear();
    panic();
}

void PsytrancerAudioProcessor::repeatFirstPageToLength()
{
    const auto length = getSequenceLength();
    const juce::ScopedLock lock (stepLock);

    const auto stepsPerPage = getStepsPerPage();

    for (auto i = stepsPerPage; i < length; ++i)
        steps[(size_t) i] = steps[(size_t) (i % stepsPerPage)];
}

void PsytrancerAudioProcessor::shiftPattern (int amount)
{
    const auto length = getSequenceLength();
    std::array<StepData, maxSequenceSteps> copy;

    {
        const juce::ScopedLock lock (stepLock);
        copy = steps;

        for (auto i = 0; i < length; ++i)
            steps[(size_t) i] = copy[(size_t) positiveModulo (i - amount, length)];
    }
}

void PsytrancerAudioProcessor::panic()
{
    panicRequested.store (true);
}

int PsytrancerAudioProcessor::getBaseRootMidiNote() const
{
    const auto root = (int) *parameters.getRawParameterValue ("root");
    const auto octave = (int) *parameters.getRawParameterValue ("octave");

    if (isMidiKeyEnabled())
    {
        const auto inputNote = midiKeyNote.load();

        if (inputNote >= 0)
            return juce::jlimit (0, 127, inputNote + (octave - 4) * 12);
    }

    return juce::jlimit (0, 127, (octave + 1) * 12 + root);
}

bool PsytrancerAudioProcessor::isMidiKeyEnabled() const
{
    return parameters.getRawParameterValue ("midiKey")->load() >= 0.5f;
}

ScaleType PsytrancerAudioProcessor::getScaleType() const
{
    return (ScaleType) juce::jlimit (0, (int) getScaleDefinitions().size() - 1,
                                     (int) *parameters.getRawParameterValue ("scale"));
}

StepResolution PsytrancerAudioProcessor::getResolution() const
{
    return (StepResolution) juce::jlimit (0, 7, (int) *parameters.getRawParameterValue ("resolution"));
}

int PsytrancerAudioProcessor::getStepsPerPage() const
{
    return juce::jlimit (1, 48, (int) std::floor (4.0 / getStepLengthPpq (getResolution())));
}

int PsytrancerAudioProcessor::getPageCount() const
{
    return juce::jlimit (1, 8, (int) *parameters.getRawParameterValue ("length"));
}

int PsytrancerAudioProcessor::getSequenceLength() const
{
    return getPageCount() * getStepsPerPage();
}

float PsytrancerAudioProcessor::getGateMultiplier() const
{
    return juce::jlimit (0.01f, 2.0f, parameters.getRawParameterValue ("gateMultiplier")->load());
}

juce::File PsytrancerAudioProcessor::getPresetDirectory() const
{
    auto applicationDataDirectory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

   #if JUCE_MAC
    // JUCE maps userApplicationDataDirectory to ~/Library on macOS. Keep our data
    // in Application Support, which is writable without Documents/Desktop TCC access.
    applicationDataDirectory = applicationDataDirectory.getChildFile ("Application Support");
   #endif

    // This resolves to a per-user, writable location on every desktop platform:
    // %APPDATA% on Windows, ~/Library/Application Support on macOS, and
    // $XDG_CONFIG_HOME (normally ~/.config) on Linux.
    return applicationDataDirectory
        .getChildFile ("Psytrancer")
        .getChildFile ("Presets");
}

juce::StringArray PsytrancerAudioProcessor::getPresetNames() const
{
    juce::StringArray names;
    const auto directory = getPresetDirectory();

    for (const auto& file : directory.findChildFiles (juce::File::findFiles, false, "*.psytrancerpreset"))
        names.add (file.getFileNameWithoutExtension());

    names.sort (true);
    return names;
}

bool PsytrancerAudioProcessor::savePreset (const juce::String& name)
{
    const auto presetName = sanitisePresetName (name);
    const auto directory = getPresetDirectory();

    if (! directory.createDirectory())
    {
        lastPresetError = "Could not create the preset folder:\n" + directory.getFullPathName();
        return false;
    }

    const auto file = directory.getChildFile (presetName + ".psytrancerpreset");
    currentPresetName = presetName;

    if (auto xml = createStateValueTree().createXml())
    {
        if (xml->writeTo (file))
        {
            lastPresetError.clear();
            return true;
        }

        lastPresetError = "Could not write the preset file:\n" + file.getFullPathName();
        return false;
    }

    lastPresetError = "Could not create preset data.";
    return false;
}

bool PsytrancerAudioProcessor::loadPreset (const juce::String& name)
{
    const auto file = getPresetDirectory().getChildFile (sanitisePresetName (name) + ".psytrancerpreset");

    if (! file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr)
        return false;

    const auto state = juce::ValueTree::fromXml (*xml);

    if (! state.isValid())
        return false;

    loadStateValueTree (state);
    currentPresetName = sanitisePresetName (name);
    panic();
    return true;
}

juce::ValueTree PsytrancerAudioProcessor::stepsToValueTree() const
{
    juce::ValueTree root { "STEPS" };
    const juce::ScopedLock lock (stepLock);

    for (auto i = 0; i < (int) steps.size(); ++i)
    {
        const auto& step = steps[(size_t) i];
        juce::ValueTree node { "STEP" };
        node.setProperty ("index", i, nullptr);
        node.setProperty ("enabled", step.enabled, nullptr);
        node.setProperty ("type", stepTypeToString (step.type), nullptr);
        node.setProperty ("pitch", step.relativePitch, nullptr);
        node.setProperty ("octave", step.octaveOffset, nullptr);
        node.setProperty ("gate", step.gateRate, nullptr);
        node.setProperty ("velocity", (int) step.velocity, nullptr);
        root.appendChild (node, nullptr);
    }

    return root;
}

void PsytrancerAudioProcessor::loadStepsFromValueTree (const juce::ValueTree& root)
{
    if (! root.isValid())
        return;

    const juce::ScopedLock lock (stepLock);

    for (const auto& node : root)
    {
        const auto index = juce::jlimit (0, maxSequenceSteps - 1, (int) node.getProperty ("index", 0));
        auto& step = steps[(size_t) index];
        const auto typeName = node.getProperty ("type", "gate").toString();
        step.enabled = (bool) node.getProperty ("enabled", true);
        step.type = stepTypeFromString (typeName);

        if (typeName == "rest")
            step.enabled = false;
        step.relativePitch = juce::jlimit (-12, 12, (int) node.getProperty ("pitch", 0));
        step.octaveOffset = juce::jlimit (-4, 4, (int) node.getProperty ("octave", 0));
        step.gateRate = juce::jlimit (0.01f, 1.0f, (float) node.getProperty ("gate", 0.75f));
        step.velocity = (juce::uint8) juce::jlimit (1, 127, (int) node.getProperty ("velocity", 100));
    }
}

juce::ValueTree PsytrancerAudioProcessor::createStateValueTree()
{
    juce::ValueTree state { "PSYTRANCER_STATE" };
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.appendChild (makeParameterOnlyState (parameters.copyState()), nullptr);
    state.appendChild (stepsToValueTree(), nullptr);
    return state;
}

void PsytrancerAudioProcessor::loadStateValueTree (const juce::ValueTree& state)
{
    parameters.replaceState (makeParameterOnlyState (getParameterStateFromRoot (state)));
    loadStepsFromValueTree (getStepStateFromRoot (state));

    if (state.hasType ("PSYTRANCER_STATE"))
        currentPresetName = state.getProperty ("presetName", "").toString();
}

void PsytrancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = createStateValueTree();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PsytrancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml (*xml);

    if (! state.isValid())
        return;

    loadStateValueTree (state);
    panic();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PsytrancerAudioProcessor();
}
