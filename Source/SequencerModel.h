#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

enum class StepType
{
    gate = 0,
    cut,
    hold
};

struct StepData
{
    bool enabled = true;
    StepType type = StepType::gate;
    int relativePitch = 0;
    int octaveOffset = 0;
    float gateRate = 1.0f;
    juce::uint8 velocity = 100;
};

enum class StepResolution
{
    eighth = 0,
    sixteenth,
    thirtySecond,
    eighthTriplet,
    sixteenthTriplet,
    thirtySecondTriplet,
    eighthDotted,
    sixteenthDotted
};

constexpr int maxSequenceSteps = 384;

enum class ScaleType
{
    chromatic = 0,
    major,
    naturalMinor,
    harmonicMinor,
    melodicMinor,
    dorian,
    phrygian,
    lydian,
    mixolydian,
    locrian,
    majorPentatonic,
    minorPentatonic,
    phrygianDominant
};

struct ScaleDefinition
{
    const char* id;
    const char* name;
    std::initializer_list<int> offsets;
};

inline const std::array<ScaleDefinition, 13>& getScaleDefinitions()
{
    static const std::array<ScaleDefinition, 13> scales {{
        { "chromatic", "Chromatic", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
        { "major", "Major", { 0, 2, 4, 5, 7, 9, 11 } },
        { "natural_minor", "Natural Minor", { 0, 2, 3, 5, 7, 8, 10 } },
        { "harmonic_minor", "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11 } },
        { "melodic_minor", "Melodic Minor", { 0, 2, 3, 5, 7, 9, 11 } },
        { "dorian", "Dorian", { 0, 2, 3, 5, 7, 9, 10 } },
        { "phrygian", "Phrygian", { 0, 1, 3, 5, 7, 8, 10 } },
        { "lydian", "Lydian", { 0, 2, 4, 6, 7, 9, 11 } },
        { "mixolydian", "Mixolydian", { 0, 2, 4, 5, 7, 9, 10 } },
        { "locrian", "Locrian", { 0, 1, 3, 5, 6, 8, 10 } },
        { "major_pentatonic", "Major Pentatonic", { 0, 2, 4, 7, 9 } },
        { "minor_pentatonic", "Minor Pentatonic", { 0, 3, 5, 7, 10 } },
        { "phrygian_dominant", "Phrygian Dominant", { 0, 1, 4, 5, 7, 8, 10 } }
    }};

    return scales;
}

inline const ScaleDefinition& getScaleDefinition (ScaleType type)
{
    const auto index = juce::jlimit (0, (int) getScaleDefinitions().size() - 1, (int) type);
    return getScaleDefinitions()[(size_t) index];
}

inline int floorDiv (int value, int divisor)
{
    const auto quotient = value / divisor;
    const auto remainder = value % divisor;
    return remainder < 0 ? quotient - 1 : quotient;
}

inline int positiveModulo (int value, int divisor)
{
    const auto result = value % divisor;
    return result < 0 ? result + divisor : result;
}

inline int relativePitchToMidiNoteUnclamped (int baseRootMidiNote, int relativePitch, const ScaleDefinition& scale)
{
    const auto degreeCount = (int) scale.offsets.size();

    if (degreeCount <= 0)
        return baseRootMidiNote;

    const auto octaveOffset = floorDiv (relativePitch, degreeCount);
    const auto degreeIndex = positiveModulo (relativePitch, degreeCount);
    return baseRootMidiNote + octaveOffset * 12 + *(scale.offsets.begin() + degreeIndex);
}

inline int relativePitchToMidiNote (int baseRootMidiNote, int relativePitch, const ScaleDefinition& scale)
{
    return juce::jlimit (0, 127, relativePitchToMidiNoteUnclamped (baseRootMidiNote, relativePitch, scale));
}

inline juce::String midiNoteName (int midiNote)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[positiveModulo (midiNote, 12)]) + juce::String (midiNote / 12 - 1);
}

inline double getStepLengthPpq (StepResolution resolution)
{
    switch (resolution)
    {
        case StepResolution::eighth: return 0.5;
        case StepResolution::sixteenth: return 0.25;
        case StepResolution::thirtySecond: return 0.125;
        case StepResolution::eighthTriplet: return 1.0 / 3.0;
        case StepResolution::sixteenthTriplet: return 1.0 / 6.0;
        case StepResolution::thirtySecondTriplet: return 1.0 / 12.0;
        case StepResolution::eighthDotted: return 0.75;
        case StepResolution::sixteenthDotted: return 0.375;
    }

    return 0.25;
}

inline const char* getResolutionName (StepResolution resolution)
{
    switch (resolution)
    {
        case StepResolution::eighth: return "1/8";
        case StepResolution::sixteenth: return "1/16";
        case StepResolution::thirtySecond: return "1/32";
        case StepResolution::eighthTriplet: return "1/8T";
        case StepResolution::sixteenthTriplet: return "1/16T";
        case StepResolution::thirtySecondTriplet: return "1/32T";
        case StepResolution::eighthDotted: return "1/8D";
        case StepResolution::sixteenthDotted: return "1/16D";
    }

    return "1/16";
}

inline juce::String getStepTypeName (StepType type)
{
    switch (type)
    {
        case StepType::gate: return "G";
        case StepType::cut: return "C";
        case StepType::hold: return "H";
    }

    return "G";
}
