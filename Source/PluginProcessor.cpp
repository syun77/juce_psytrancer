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
        case StepType::hold: return "hold";
        case StepType::rest: return "rest";
    }

    return "rest";
}

StepType stepTypeFromString (const juce::String& value)
{
    if (value == "hold")
        return StepType::hold;

    if (value == "rest")
        return StepType::rest;

    return StepType::gate;
}
}

PsytrancerAudioProcessor::PsytrancerAudioProcessor()
    : AudioProcessor (BusesProperties()),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    clearPattern();
}

juce::AudioProcessorValueTreeState::ParameterLayout PsytrancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "length", 1 }, "Length", 1, 128, 16));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "resolution", 1 }, "Rate",
        makeChoiceList ({ "1/4", "1/8", "1/16", "1/32", "1/64", "1/8T", "1/16T", "1/32T", "1/8D", "1/16D" }), 2));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "root", 1 }, "Root",
        makeChoiceList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }), 0));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "octave", 1 }, "Octave", 0, 8, 4));

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
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet().isDisabled();
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

        resetTransportState();
        midi.swapWith (generated);
        return;
    }

    const auto quarterNoteSamples = currentSampleRate * 60.0 / bpm;
    const auto blockLengthPpq = (double) numSamples / quarterNoteSamples;
    const auto blockEndPpq = ppq + blockLengthPpq;
    const auto expectedPpq = previousPpq + blockLengthPpq;

    if (wasPlaying && (ppq + 0.0001 < previousPpq || std::abs (ppq - expectedPpq) > 0.25))
        sendActiveNoteOff (generated, 0);

    wasPlaying = true;

    if (pendingNoteOffPpq >= ppq && pendingNoteOffPpq < blockEndPpq)
    {
        const auto sampleOffset = juce::jlimit (0, numSamples - 1,
                                                (int) std::round ((pendingNoteOffPpq - ppq) * quarterNoteSamples));
        sendActiveNoteOff (generated, sampleOffset);
    }

    const auto stepLengthPpq = getStepLengthPpq (getResolution());
    const auto sequenceLength = getSequenceLength();
    auto firstStep = (int64_t) std::ceil ((ppq - 1.0e-9) / stepLengthPpq);
    auto lastStep = (int64_t) std::floor ((blockEndPpq - 1.0e-9) / stepLengthPpq);

    const auto baseNote = getBaseRootMidiNote();
    const auto scale = getScaleDefinition (getScaleType());

    for (auto absoluteStep = firstStep; absoluteStep <= lastStep; ++absoluteStep)
    {
        const auto boundaryPpq = (double) absoluteStep * stepLengthPpq;
        const auto sampleOffset = juce::jlimit (0, numSamples - 1,
                                                (int) std::round ((boundaryPpq - ppq) * quarterNoteSamples));
        const auto stepIndex = positiveModulo ((int) absoluteStep, sequenceLength);
        currentStep.store (stepIndex);

        StepData step;
        {
            const juce::ScopedLock lock (stepLock);
            step = steps[(size_t) stepIndex];
        }

        if (step.type == StepType::gate)
        {
            sendActiveNoteOff (generated, sampleOffset);

            const auto note = relativePitchToMidiNote (baseNote, step.relativePitch, scale);
            activeNote = note;
            activeChannel = 1;
            pendingNoteOffPpq = boundaryPpq + stepLengthPpq * juce::jlimit (0.01f, 1.0f, step.gateRate);
            generated.addEvent (juce::MidiMessage::noteOn (activeChannel, note, (juce::uint8) step.velocity), sampleOffset);
        }
        else if (step.type == StepType::rest)
        {
            sendActiveNoteOff (generated, sampleOffset);
        }
    }

    if (pendingNoteOffPpq >= ppq && pendingNoteOffPpq < blockEndPpq)
    {
        const auto sampleOffset = juce::jlimit (0, numSamples - 1,
                                                (int) std::round ((pendingNoteOffPpq - ppq) * quarterNoteSamples));
        sendActiveNoteOff (generated, sampleOffset);
    }

    previousPpq = ppq;
    midi.swapWith (generated);
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
    pendingNoteOffPpq = -1.0;
    activeNote = -1;
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
    return steps[(size_t) juce::jlimit (0, 127, index)];
}

void PsytrancerAudioProcessor::setStep (int index, StepData step)
{
    const auto safeIndex = juce::jlimit (0, 127, index);
    step.relativePitch = juce::jlimit (-64, 64, step.relativePitch);
    step.gateRate = juce::jlimit (0.01f, 1.0f, step.gateRate);
    step.velocity = (juce::uint8) juce::jlimit (1, 127, (int) step.velocity);

    if (step.type == StepType::hold)
    {
        const juce::ScopedLock lock (stepLock);
        auto hasGateBefore = false;

        for (auto i = safeIndex - 1; i >= 0; --i)
        {
            if (steps[(size_t) i].type == StepType::gate)
            {
                hasGateBefore = true;
                break;
            }

            if (steps[(size_t) i].type == StepType::rest)
                break;
        }

        if (! hasGateBefore)
            step.type = StepType::rest;

        steps[(size_t) safeIndex] = step;
        return;
    }

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
        steps[(size_t) i].type = (i % 4 == 0) ? StepType::gate : StepType::rest;
        steps[(size_t) i].relativePitch = (i % 16 == 8) ? 4 : 0;
    }
}

void PsytrancerAudioProcessor::repeatFirstPageToLength()
{
    const auto length = getSequenceLength();
    const juce::ScopedLock lock (stepLock);

    for (auto i = 16; i < length; ++i)
        steps[(size_t) i] = steps[(size_t) (i % 16)];
}

void PsytrancerAudioProcessor::shiftPattern (int amount)
{
    const auto length = getSequenceLength();
    std::array<StepData, 128> copy;

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
    return juce::jlimit (0, 127, (octave + 1) * 12 + root);
}

ScaleType PsytrancerAudioProcessor::getScaleType() const
{
    return (ScaleType) juce::jlimit (0, (int) getScaleDefinitions().size() - 1,
                                     (int) *parameters.getRawParameterValue ("scale"));
}

StepResolution PsytrancerAudioProcessor::getResolution() const
{
    return (StepResolution) juce::jlimit (0, 9, (int) *parameters.getRawParameterValue ("resolution"));
}

int PsytrancerAudioProcessor::getSequenceLength() const
{
    return juce::jlimit (1, 128, (int) *parameters.getRawParameterValue ("length"));
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
        node.setProperty ("type", stepTypeToString (step.type), nullptr);
        node.setProperty ("pitch", step.relativePitch, nullptr);
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
        const auto index = juce::jlimit (0, 127, (int) node.getProperty ("index", 0));
        auto& step = steps[(size_t) index];
        step.type = stepTypeFromString (node.getProperty ("type", "gate").toString());
        step.relativePitch = juce::jlimit (-64, 64, (int) node.getProperty ("pitch", 0));
        step.gateRate = juce::jlimit (0.01f, 1.0f, (float) node.getProperty ("gate", 0.75f));
        step.velocity = (juce::uint8) juce::jlimit (1, 127, (int) node.getProperty ("velocity", 100));
    }
}

void PsytrancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.appendChild (stepsToValueTree(), nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PsytrancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    parameters.replaceState (state);
    loadStepsFromValueTree (state.getChildWithName ("STEPS"));
    panic();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PsytrancerAudioProcessor();
}
