#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <array>

class SineWaveVoice
{
public:
    // Wavetable constants and data
    static constexpr int WAVETABLE_SIZE = 4096;
    static std::array<float, WAVETABLE_SIZE> sineTable;
    static bool wavetableInitialized;

    // Initialize wavetable
    static void initializeWavetable() {
        if (!wavetableInitialized) {
            for (int i = 0; i < WAVETABLE_SIZE; ++i) {
                sineTable[i] = std::sin(juce::MathConstants<double>::twoPi * i / WAVETABLE_SIZE);
            }
            wavetableInitialized = true;
        }
    }

    SineWaveVoice(double sampleRate)
        : sampleRate(sampleRate), isActive(false), numOvertones(8),
        attackStage(false), attackLevel(0.0f), attackSamples(0), attackSamplesRemaining(0),
        releaseStage(false), releaseLevel(0.0f), releaseSamples(0), releaseSamplesRemaining(0)
    {
        // Initialize wavetable
        initializeWavetable();

        // Initialize arrays for overtones
        const int maxOvertones = 32;

        phases.resize(maxOvertones, 0.0);
        phaseIncrements.resize(maxOvertones, 0.0);
        gains.resize(maxOvertones, 0.0);

        // Default envelope times (in seconds)
        setAttackTime(0.002f);  // 2ms attack
        setReleaseTime(0.02f);  // 20ms release
    }

    // Set the sample rate and recalculate time-based parameters
    void setSampleRate(double newSampleRate)
    {
        sampleRate = newSampleRate;

        // Recalculate time-based values
        setAttackTime(0.002f);
        setReleaseTime(0.02f);

        // Update phase increments if active
        if (isActive)
        {
            startNote(midiNote, velocity);
        }
    }

    void startNote(int midiNoteNumber, float velocity)
    {
        midiNote = midiNoteNumber;
        this->velocity = velocity;
        isActive = true;

        // Start with attack phase
        attackStage = true;
        attackLevel = 0.0f;
        attackSamplesRemaining = attackSamples;

        // Not in release stage
        releaseStage = false;
        releaseLevel = 1.0f;

        // Convert MIDI note to fundamental frequency
        float baseFrequency = 440.0f * std::pow(2.0f, (midiNoteNumber - 69) / 12.0f);

        // Calculate the maximum possible gain sum to normalize later
        float maxGainSum = 0.0f;

        // Reset all phases
        for (int i = 0; i < numOvertones; ++i)
        {
            // Initialize phase at zero
            phases[i] = 0.0;

            // Calculate the frequency for this overtone (fundamental + harmonics)
            float overtoneFreq = baseFrequency * (i + 1);

            // Calculate gain
            gains[i] = calculateGain(overtoneFreq, i + 1, baseFrequency);

            // Keep track of the total gain
            maxGainSum += std::abs(gains[i]);

            // Calculate phase increment for this frequency
            phaseIncrements[i] = juce::MathConstants<double>::twoPi * overtoneFreq / sampleRate;
        }

        // Normalize gains to prevent clipping when all sine waves align
        // We use 0.9 as safety factor to stay away from the edge
        if (maxGainSum > 0.9f)
        {
            float normalizationFactor = 0.9f / maxGainSum;
            for (int i = 0; i < numOvertones; ++i)
            {
                gains[i] *= normalizationFactor;
            }
        }
    }

    // Calculate gain using the formula from Python code
    float calculateGain(float freq, int overtoneNumber, float baseFrequency)
    {
        // Scale the gain to prevent clipping (modified from Python)
        return 2.0f / (std::pow(1.1f, freq / baseFrequency) * std::pow(1.6f, overtoneNumber));
    }

    // Set attack time in seconds
    void setAttackTime(float seconds)
    {
        attackSamples = static_cast<int>(seconds * sampleRate);
        // Ensure at least one sample for attack
        attackSamples = std::max(1, attackSamples);
    }

    // Set release time in seconds
    void setReleaseTime(float seconds)
    {
        releaseSamples = static_cast<int>(seconds * sampleRate);
        // Ensure at least one sample for release
        releaseSamples = std::max(1, releaseSamples);
    }

    void stopNote()
    {
        // Start release phase
        if (isActive && !releaseStage)
        {
            releaseStage = true;
            attackStage = false;  // Exit attack stage if still in it
            releaseLevel = attackStage ? attackLevel : 1.0f;  // Start from current level
            releaseSamplesRemaining = releaseSamples;
        }
    }

    bool isNoteActive() const
    {
        return isActive;
    }

    bool isReleasing() const
    {
        return isActive && releaseStage;
    }

    int getReleaseSamplesRemaining() const
    {
        return releaseStage ? releaseSamplesRemaining : 0;
    }

    float getCurrentAmplitude() const
    {
        if (attackStage)
            return velocity * attackLevel;
        else if (releaseStage)
            return velocity * releaseLevel;
        else
            return velocity;
    }

    int getMidiNote() const
    {
        return midiNote;
    }

    // Set number of overtones (harmonics) to generate
    void setNumOvertones(int num, int maxOvertones = 32)
    {
        // Ensure valid range
        num = std::max(1, std::min(maxOvertones, num));

        if (numOvertones != num)
        {
            numOvertones = num;

            // Resize to only what we need (with small buffer for performance)
            int allocSize = numOvertones + 2;
            if (phases.size() < allocSize) {
                phases.resize(allocSize, 0.0);
                phaseIncrements.resize(allocSize, 0.0);
                gains.resize(allocSize, 0.0);
            }

            // Re-initialize if already playing a note
            if (isActive && !releaseStage)
            {
                startNote(midiNote, velocity);
            }
        }
    }

    // Generate one sample summing all overtones
    float getSample() const
    {
        if (!isActive)
            return 0.0f;

        float sample = 0.0f;

        // Sum the fundamental and all overtones using wavetable
        for (int i = 0; i < numOvertones; ++i)
        {
            // Convert phase to table index
            int index = static_cast<int>((phases[i] / juce::MathConstants<double>::twoPi) * WAVETABLE_SIZE) % WAVETABLE_SIZE;
            if (index < 0) index += WAVETABLE_SIZE;

            // Linear interpolation for smoother waveform
            float frac = ((phases[i] / juce::MathConstants<double>::twoPi) * WAVETABLE_SIZE) - index;
            int nextIndex = (index + 1) % WAVETABLE_SIZE;

            float value = sineTable[index] + frac * (sineTable[nextIndex] - sineTable[index]);
            sample += value * gains[i];
        }

        // Multiply by velocity and envelope
        if (attackStage)
        {
            sample *= velocity * attackLevel;
        }
        else if (releaseStage)
        {
            sample *= velocity * releaseLevel;
        }
        else
        {
            sample *= velocity;
        }

        return sample;
    }

    // Advance the phase for all oscillators and update envelope
    void advancePhase()
    {
        if (!isActive)
            return;

        // Update all phases
        for (int i = 0; i < numOvertones; ++i)
        {
            phases[i] += phaseIncrements[i];
            if (phases[i] >= juce::MathConstants<double>::twoPi)
                phases[i] -= juce::MathConstants<double>::twoPi;
        }

        // Handle attack stage
        if (attackStage)
        {
            if (attackSamplesRemaining > 0)
            {
                // Linear attack ramp
                attackLevel = 1.0f - (static_cast<float>(attackSamplesRemaining) / static_cast<float>(attackSamples));
                attackSamplesRemaining--;
            }
            else
            {
                // End of attack
                attackStage = false;
                attackLevel = 1.0f;
            }
        }

        // Handle release stage
        if (releaseStage)
        {
            if (releaseSamplesRemaining > 0)
            {
                // Linear release ramp
                releaseLevel = static_cast<float>(releaseSamplesRemaining) / static_cast<float>(releaseSamples);
                releaseSamplesRemaining--;
            }
            else
            {
                // End of release
                isActive = false;
                releaseStage = false;
            }
        }
    }

private:
    double sampleRate;
    bool isActive;
    int midiNote;
    float velocity;

    // Arrays for managing overtones
    std::vector<double> phases;
    std::vector<double> phaseIncrements;
    std::vector<float> gains;

    int numOvertones;

    // Attack envelope
    bool attackStage;
    float attackLevel;
    int attackSamples;
    int attackSamplesRemaining;

    // Release envelope
    bool releaseStage;
    float releaseLevel;
    int releaseSamples;
    int releaseSamplesRemaining;
};

// Static member initialization outside the class
inline std::array<float, SineWaveVoice::WAVETABLE_SIZE> SineWaveVoice::sineTable;
inline bool SineWaveVoice::wavetableInitialized = false;

class SineWaveAudioProcessor : public juce::AudioProcessor
{
public:
    // Define the constant as a static member of the processor class
    static constexpr int MAX_OVERTONES = 32;
    static constexpr int MAX_VOICES = 16;

    SineWaveAudioProcessor();
    ~SineWaveAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameters
    juce::AudioProcessorValueTreeState parameters;

    // Current polyphony count (active voices)
    int getActiveVoiceCount() const;

    // Get the current scaling factor used to prevent clipping
    float getVoiceScalingFactor() const;

private:
    // Collection of voices for polyphony
    std::vector<SineWaveVoice> voices;

    // Voice management
    SineWaveVoice* findFreeVoice();
    SineWaveVoice* findVoiceForNote(int midiNote);

    // Current sample rate
    double currentSampleRate;

    // Voice scaling smoothing
    float currentVoiceScalingFactor;
    float targetVoiceScalingFactor;
    float voiceScalingSmoothingCoeff;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineWaveAudioProcessor)
};