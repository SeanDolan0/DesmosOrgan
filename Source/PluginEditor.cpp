#include "PluginProcessor.h"
#include "PluginEditor.h"

SineWaveAudioProcessorEditor::SineWaveAudioProcessorEditor(SineWaveAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor(&p), audioProcessor(p), valueTreeState(vts)
{
    // Apply our modern look and feel
    setLookAndFeel(&modernLookAndFeel);

    // Set up title label
    pluginTitleLabel.setText("DESMOS SYNTH", juce::dontSendNotification);
    pluginTitleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    pluginTitleLabel.setJustificationType(juce::Justification::centred);
    pluginTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00b7ff));
    addAndMakeVisible(pluginTitleLabel);

    // Set up amplitude slider
    amplitudeSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    amplitudeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    amplitudeSlider.setRange(0.0, 1.0, 0.01);
    amplitudeSlider.setValue(0.5);
    amplitudeSlider.setDoubleClickReturnValue(true, 0.5);
    amplitudeSlider.setPopupDisplayEnabled(true, true, this);
    addAndMakeVisible(amplitudeSlider);

    // Set up overtones slider with clearer labeling
    overtonesSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    overtonesSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    overtonesSlider.setRange(1, SineWaveAudioProcessor::MAX_OVERTONES, 1);
    overtonesSlider.setValue(8);
    overtonesSlider.setDoubleClickReturnValue(true, 8);
    overtonesSlider.setPopupDisplayEnabled(true, true, this);
    overtonesSlider.setTextValueSuffix(" harmonics");
    addAndMakeVisible(overtonesSlider);

    // Set up release slider with modern style
    releaseSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    releaseSlider.setRange(0.001, 0.5, 0.001);
    releaseSlider.setValue(0.02);
    releaseSlider.setDoubleClickReturnValue(true, 0.02);
    releaseSlider.setPopupDisplayEnabled(true, true, this);
    releaseSlider.setTextValueSuffix(" s");
    addAndMakeVisible(releaseSlider);

    // Set up labels with modern styling
    auto setupLabel = [this](juce::Label& label, const juce::String& text, juce::Component* component) {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font(14.0f, juce::Font::bold));
        label.setJustificationType(juce::Justification::centred);
        label.attachToComponent(component, false);
        addAndMakeVisible(label);
        };

    setupLabel(amplitudeLabel, "VOLUME", &amplitudeSlider);
    setupLabel(overtonesLabel, "HARMONICS", &overtonesSlider);
    setupLabel(releaseLabel, "RELEASE", &releaseSlider);

    // Add pure sine mode toggle with modern styling
    pureToggle.setButtonText("PURE SINE");
    pureToggle.setToggleState(false, juce::dontSendNotification);
    pureToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    pureToggle.onClick = [this]() {
        if (pureToggle.getToggleState()) {
            previousOvertoneValue = static_cast<int>(overtonesSlider.getValue());
            overtonesSlider.setValue(1.0);
        }
        else if (previousOvertoneValue > 1) {
            overtonesSlider.setValue(static_cast<double>(previousOvertoneValue));
        }
        };
    addAndMakeVisible(pureToggle);

    // Add voice meter
    addAndMakeVisible(voiceMeter);

    // Connect sliders to parameters
    amplitudeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        valueTreeState, "amplitude", amplitudeSlider);

    overtonesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        valueTreeState, "overtones", overtonesSlider);

    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        valueTreeState, "release", releaseSlider);

    // Set the plugin's size for modern layout
    setSize(500, 320);

    // Start the timer to update the display
    startTimerHz(30); // Higher refresh rate for smoother metering
}

SineWaveAudioProcessorEditor::~SineWaveAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SineWaveAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Fill the background with gradient
    juce::ColourGradient gradient(
        juce::Colour(0xff141414), 0.0f, 0.0f,
        juce::Colour(0xff252525), 0.0f, (float)getHeight(),
        false);

    g.setGradientFill(gradient);
    g.fillAll();

    // Draw subtle grid pattern
    g.setColour(juce::Colour(0xff353535));
    for (int i = 0; i < getWidth(); i += 20)
    {
        g.drawVerticalLine(i, 0, getHeight());
    }

    for (int i = 0; i < getHeight(); i += 20)
    {
        g.drawHorizontalLine(i, 0, getWidth());
    }

    // Draw decorative accent lines
    g.setColour(juce::Colour(0xff00b7ff).withAlpha(0.4f));
    g.drawLine(0, 45.0f, getWidth(), 45.0f, 1.0f);
    g.drawLine(0, getHeight() - 40.0f, getWidth(), getHeight() - 40.0f, 1.0f);

    // Draw plugin version
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(12.0f);
    g.drawText("v1.0", getLocalBounds().withTrimmedBottom(10).withTrimmedRight(10), juce::Justification::bottomRight);
}

void SineWaveAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Place title at the top
    pluginTitleLabel.setBounds(bounds.removeFromTop(35));

    // Place voice meter below title with some spacing
    bounds.removeFromTop(15);
    voiceMeter.setBounds(bounds.removeFromTop(30).reduced(50, 0));

    // Leave space between meter and controls
    bounds.removeFromTop(20);

    // Position Pure Sine toggle 
    pureToggle.setBounds(bounds.removeFromBottom(30).withSizeKeepingCentre(120, 24));

    // Leave space for release slider
    bounds.removeFromBottom(50);

    // Position release slider
    auto releaseArea = bounds.removeFromBottom(40).reduced(40, 0);
    releaseSlider.setBounds(releaseArea);

    // Leave space between sliders
    bounds.removeFromBottom(30);

    // Position rotary knobs side by side
    auto sliderArea = bounds;
    int sliderWidth = (sliderArea.getWidth() - 20) / 2;

    amplitudeSlider.setBounds(sliderArea.removeFromLeft(sliderWidth));

    // Add spacing between sliders
    sliderArea.removeFromLeft(20);

    overtonesSlider.setBounds(sliderArea);
}

void SineWaveAudioProcessorEditor::timerCallback()
{
    // Update the voice meter
    voiceMeter.setValues(audioProcessor.getActiveVoiceCount(), audioProcessor.getVoiceScalingFactor());

    // Update pure sine toggle state if needed
    if (pureToggle.getToggleState() && static_cast<int>(overtonesSlider.getValue()) > 1)
    {
        pureToggle.setToggleState(false, juce::dontSendNotification);
    }
    else if (!pureToggle.getToggleState() && static_cast<int>(overtonesSlider.getValue()) == 1)
    {
        pureToggle.setToggleState(true, juce::dontSendNotification);
    }
}