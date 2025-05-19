#include "PluginProcessor.h"
#include "PluginEditor.h"

SineWaveAudioProcessor::SineWaveAudioProcessor() :
    AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, "PARAMETERS",
        {
            std::make_unique<juce::AudioParameterFloat>("amplitude", "Master Amplitude", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterInt>("overtones", "Number of Overtones", 1, MAX_OVERTONES, 8),
            std::make_unique<juce::AudioParameterFloat>("release", "Release Time", 0.001f, 0.5f, 0.02f)
        }),
    currentSampleRate(44100.0),
    currentVoiceScalingFactor(1.0f),
    targetVoiceScalingFactor(1.0f),
    voiceScalingSmoothingCoeff(0.1f)
{
    // Initialize wavetable
    SineWaveVoice::initializeWavetable();

    // Initialize voices array
    for (int i = 0; i < MAX_VOICES; ++i)
    {
        voices.emplace_back(currentSampleRate);
    }
}

SineWaveAudioProcessor::~SineWaveAudioProcessor()
{
}

const juce::String SineWaveAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SineWaveAudioProcessor::acceptsMidi() const
{
    return true;
}

bool SineWaveAudioProcessor::producesMidi() const
{
    return false;
}

bool SineWaveAudioProcessor::isMidiEffect() const
{
    return false;
}

double SineWaveAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SineWaveAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int SineWaveAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SineWaveAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String SineWaveAudioProcessor::getProgramName(int index)
{
    return {};
}

void SineWaveAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

void SineWaveAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Update sample rate for all voices
    currentSampleRate = sampleRate;
    for (auto& voice : voices)
    {
        voice.setSampleRate(sampleRate);
    }

    // Initialize smoothing
    currentVoiceScalingFactor = 1.0f;
    targetVoiceScalingFactor = 1.0f;

    // Calculate smoothing coefficient - smoother transition over ~20ms
    voiceScalingSmoothingCoeff = 1.0f - std::exp(-1.0f / (0.02f * sampleRate));
}

void SineWaveAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool SineWaveAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // This is a synth plugin, so we only support mono or stereo output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // We don't need input, so set it to no channels
    if (!layouts.getMainInputChannelSet().isDisabled())
        return false;

    return true;
}

void SineWaveAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear the buffer first
    buffer.clear();

    // Get parameters
    float masterAmplitude = *parameters.getRawParameterValue("amplitude");
    int numOvertones = static_cast<int>(*parameters.getRawParameterValue("overtones"));
    float releaseTime = *parameters.getRawParameterValue("release");
    float attackTime = 0.002f;  // Fixed 2ms attack

    // Make sure overtone value is valid
    numOvertones = std::max(1, std::min(MAX_OVERTONES, numOvertones));

    // Update parameters on all voices
    for (auto& voice : voices)
    {
        voice.setNumOvertones(numOvertones, MAX_OVERTONES);
        voice.setReleaseTime(releaseTime);
        voice.setAttackTime(attackTime);
    }

    // Process MIDI messages
    juce::MidiBuffer::Iterator midiIterator(midiMessages);
    juce::MidiMessage message;
    int midiEventPos;

    // Process any MIDI messages
    while (midiIterator.getNextEvent(message, midiEventPos))
    {
        if (message.isNoteOn())
        {
            int noteNumber = message.getNoteNumber();

            // Scale velocity more conservatively to prevent clipping at max velocity
            float velocity = message.getFloatVelocity() * 0.8f;

            // Find a free voice and start the note
            SineWaveVoice* voice = findFreeVoice();
            if (voice != nullptr)
            {
                voice->startNote(noteNumber, velocity);
            }
        }
        else if (message.isNoteOff())
        {
            int noteNumber = message.getNoteNumber();

            // Find the voice playing this note and stop it
            SineWaveVoice* voice = findVoiceForNote(noteNumber);
            if (voice != nullptr)
            {
                voice->stopNote();
            }
        }
        else if (message.isAllNotesOff())
        {
            // Stop all notes
            for (auto& voice : voices)
            {
                voice.stopNote();
            }
        }
    }

    // Count active voices
    int activeVoiceCount = getActiveVoiceCount();

    // Calculate target scaling factor to prevent clipping
    if (activeVoiceCount > 0)
    {
        // More conservative scaling for multiple voices
        // This ensures even at max velocity we won't clip
        float baseScaling = 1.0f / std::sqrt(static_cast<float>(activeVoiceCount));

        // Apply additional scaling based on overtone count
        if (numOvertones > 1)
        {
            // Apply logarithmic scaling for overtones to maintain perceived volume
            targetVoiceScalingFactor = baseScaling * (0.7f + (0.3f / std::log10(numOvertones + 1)));
        }
        else
        {
            targetVoiceScalingFactor = baseScaling;
        }
    }
    else
    {
        targetVoiceScalingFactor = 1.0f;
    }

    // Get buffer pointers for direct access
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    const int numSamples = buffer.getNumSamples();

#if JUCE_USE_SIMD
    // SIMD optimized processing when supported
    for (int sample = 0; sample < numSamples; sample += 4) {
        // Smooth the voice scaling factor
        currentVoiceScalingFactor += voiceScalingSmoothingCoeff * (targetVoiceScalingFactor - currentVoiceScalingFactor);

        // Process 4 samples at once
        float samples[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        // Sum all active voices
        for (auto& voice : voices) {
            if (voice.isNoteActive()) {
                for (int i = 0; i < 4 && (sample + i) < numSamples; ++i) {
                    samples[i] += voice.getSample();
                    voice.advancePhase();
                }
            }
        }

        // Apply scaling and master amplitude
        for (int i = 0; i < 4 && (sample + i) < numSamples; ++i) {
            float value = samples[i] * currentVoiceScalingFactor * masterAmplitude;

            // Soft clipping
            if (value > 0.7f)
                value = 0.7f + (1.0f - 0.7f) * std::tanh((value - 0.7f) / (1.0f - 0.7f));
            else if (value < -0.7f)
                value = -0.7f + (1.0f - 0.7f) * std::tanh((value + 0.7f) / (1.0f - 0.7f));

            leftChannel[sample + i] = value;
            if (rightChannel)
                rightChannel[sample + i] = value;
        }
    }
#else
    // Process each sample
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Smooth the voice scaling factor
        currentVoiceScalingFactor += voiceScalingSmoothingCoeff * (targetVoiceScalingFactor - currentVoiceScalingFactor);

        float sampleValue = 0.0f;

        // Sum all active voices for this sample
        for (auto& voice : voices)
        {
            if (voice.isNoteActive())
            {
                sampleValue += voice.getSample();
            }
        }

        // Apply voice scaling to prevent clipping
        sampleValue *= currentVoiceScalingFactor;

        // Apply master amplitude
        sampleValue *= masterAmplitude;

        // Apply a more gradual soft clipping with lower threshold
        if (sampleValue > 0.7f)
        {
            sampleValue = 0.7f + (1.0f - 0.7f) * std::tanh((sampleValue - 0.7f) / (1.0f - 0.7f));
        }
        else if (sampleValue < -0.7f)
        {
            sampleValue = -0.7f + (1.0f - 0.7f) * std::tanh((sampleValue + 0.7f) / (1.0f - 0.7f));
        }

        // Copy the same sample value to all channels
        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        {
            buffer.setSample(channel, sample, sampleValue);
        }

        // Advance the phase of all active voices AFTER processing all channels
        for (auto& voice : voices)
        {
            voice.advancePhase();
        }
    }
#endif
}

bool SineWaveAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SineWaveAudioProcessor::createEditor()
{
    return new SineWaveAudioProcessorEditor(*this, parameters);
}

void SineWaveAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Store the plugin's state for persistence between sessions
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SineWaveAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Restore the plugin's state from saved data
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

SineWaveVoice* SineWaveAudioProcessor::findFreeVoice()
{
    // Find an inactive voice to use
    for (auto& voice : voices)
    {
        if (!voice.isNoteActive())
            return &voice;
    }

    // If all voices are active, find the oldest releasing voice
    SineWaveVoice* oldestReleasingVoice = nullptr;
    int oldestReleaseTime = 0;

    for (auto& voice : voices) {
        if (voice.isReleasing()) {
            int releaseTime = voice.getReleaseSamplesRemaining();
            if (oldestReleasingVoice == nullptr || releaseTime < oldestReleaseTime) {
                oldestReleasingVoice = &voice;
                oldestReleaseTime = releaseTime;
            }
        }
    }

    if (oldestReleasingVoice != nullptr)
        return oldestReleasingVoice;

    // If no releasing voices, find the quietest voice
    SineWaveVoice* quietestVoice = &voices[0];
    float minAmplitude = 1.0f;

    for (auto& voice : voices) {
        float amplitude = voice.getCurrentAmplitude();
        if (amplitude < minAmplitude) {
            quietestVoice = &voice;
            minAmplitude = amplitude;
        }
    }

    return quietestVoice;
}

SineWaveVoice* SineWaveAudioProcessor::findVoiceForNote(int midiNote)
{
    // Find a voice playing the given note
    for (auto& voice : voices)
    {
        if (voice.isNoteActive() && voice.getMidiNote() == midiNote)
            return &voice;
    }

    return nullptr;
}

int SineWaveAudioProcessor::getActiveVoiceCount() const
{
    int count = 0;
    for (const auto& voice : voices)
    {
        if (voice.isNoteActive())
            count++;
    }
    return count;
}

float SineWaveAudioProcessor::getVoiceScalingFactor() const
{
    return currentVoiceScalingFactor;
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SineWaveAudioProcessor();
}