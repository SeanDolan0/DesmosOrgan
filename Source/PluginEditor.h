#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// Modern look and feel implementation integrated directly in PluginEditor.h
class ModernLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernLookAndFeel()
    {
        // Modern color scheme
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff1e1e1e));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xff00b7ff));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00b7ff));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::Slider::trackColourId, juce::Colour(0xff00b7ff));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00b7ff));
        setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::Label::textColourId, juce::Colours::white);
        setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00b7ff));
        setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff2a2a2a));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
        auto centreX = (float)x + (float)width * 0.5f;
        auto centreY = (float)y + (float)height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Fill background
        g.setColour(findColour(juce::Slider::rotarySliderOutlineColourId));
        g.fillEllipse(rx, ry, rw, rw);

        // Draw outer ring
        g.setColour(findColour(juce::Slider::rotarySliderFillColourId));
        g.drawEllipse(rx, ry, rw, rw, 2.0f);

        // Create a path for the value arc
        juce::Path valueArc;
        valueArc.addPieSegment(rx, ry, rw, rw, rotaryStartAngle, angle, 0.0);
        g.setColour(findColour(juce::Slider::rotarySliderFillColourId).withAlpha(0.8f));
        g.fillPath(valueArc);

        // Draw pointer
        juce::Path p;
        auto pointerLength = radius * 0.7f;
        auto pointerThickness = 2.5f;

        p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

        g.setColour(juce::Colours::white);
        g.fillPath(p);

        // Draw central dot
        g.setColour(findColour(juce::Slider::thumbColourId));
        g.fillEllipse(centreX - 3.0f, centreY - 3.0f, 6.0f, 6.0f);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearHorizontal)
        {
            auto trackWidth = height * 0.3f;
            auto trackX = x;
            auto trackY = y + (height - trackWidth) * 0.5f;
            auto trackLength = width;

            // Draw background track
            g.setColour(findColour(juce::Slider::backgroundColourId));
            g.fillRoundedRectangle(trackX, trackY, trackLength, trackWidth, trackWidth * 0.5f);

            // Draw filled part
            auto fillLength = (sliderPos - x);
            g.setColour(findColour(juce::Slider::trackColourId));
            g.fillRoundedRectangle(trackX, trackY, juce::jlimit(0.0f, (float)trackLength, fillLength), trackWidth, trackWidth * 0.5f);

            // Draw thumb
            auto thumbWidth = trackWidth * 1.5f;
            auto thumbX = sliderPos - thumbWidth * 0.5f;
            auto thumbY = trackY + trackWidth * 0.5f - thumbWidth * 0.5f;
            g.setColour(findColour(juce::Slider::thumbColourId));
            g.fillEllipse(thumbX, thumbY, thumbWidth, thumbWidth);
        }
        else
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto fontSize = juce::jmin(15.0f, button.getHeight() * 0.75f);
        auto tickWidth = fontSize * 1.1f;

        // Draw tick box
        juce::Rectangle<float> tickBounds(4.0f, (button.getHeight() - tickWidth) * 0.5f, tickWidth, tickWidth);

        g.setColour(button.findColour(juce::ToggleButton::tickDisabledColourId));
        g.fillRoundedRectangle(tickBounds, 3.0f);

        if (button.getToggleState())
        {
            g.setColour(button.findColour(juce::ToggleButton::tickColourId));
            g.fillRoundedRectangle(tickBounds.reduced(2.0f), 3.0f);
        }

        // Draw text
        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(fontSize);

        // Fixed: Use the correct drawFittedText method with proper arguments
        g.drawFittedText(button.getButtonText(),
            button.getLocalBounds().withTrimmedLeft(juce::roundToInt(tickWidth) + 10)
            .withTrimmedRight(2),
            juce::Justification::centredLeft, 1, 1.0f);
    }
};

// A custom component for displaying active voices with a meter
class VoiceActivityMeter : public juce::Component
{
public:
    VoiceActivityMeter() : activeVoices(0), scalingFactor(1.0f) {}

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Draw background
        g.setColour(juce::Colour(0xff2a2a2a));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Draw voice count meter
        if (activeVoices > 0)
        {
            float meterWidth = bounds.getWidth() * (float)activeVoices / 16.0f;
            g.setColour(juce::Colour(0xff00b7ff).withAlpha(0.8f));
            g.fillRoundedRectangle(bounds.withWidth(meterWidth), 4.0f);
        }

        // Draw scaling factor meter
        float scalingY = bounds.getY() + bounds.getHeight() + 5.0f;
        float scalingHeight = 4.0f;
        juce::Rectangle<float> scalingBounds(bounds.getX(), scalingY, bounds.getWidth(), scalingHeight);

        g.setColour(juce::Colour(0xff2a2a2a));
        g.fillRoundedRectangle(scalingBounds, 2.0f);

        float scalingMeterWidth = bounds.getWidth() * scalingFactor;
        g.setColour(juce::Colour(0xffff7700));
        g.fillRoundedRectangle(scalingBounds.withWidth(scalingMeterWidth), 2.0f);

        // Draw text
        g.setColour(juce::Colours::white);
        g.setFont(12.0f);
        juce::String text = "Active Voices: " + juce::String(activeVoices) + " | Scaling: " + juce::String(scalingFactor, 2);

        // Fixed: Use the correct drawText method
        g.drawText(text, bounds.toNearestInt(), juce::Justification::centred, false);
    }

    void setValues(int voices, float scaling)
    {
        activeVoices = voices;
        scalingFactor = scaling;
        repaint();
    }

private:
    int activeVoices;
    float scalingFactor;
};

class SineWaveAudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    SineWaveAudioProcessorEditor(SineWaveAudioProcessor&, juce::AudioProcessorValueTreeState& vts);
    ~SineWaveAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Timer callback for updating the voice count display
    void timerCallback() override;

private:
    SineWaveAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& valueTreeState;

    ModernLookAndFeel modernLookAndFeel;

    juce::Slider amplitudeSlider;
    juce::Label amplitudeLabel;

    juce::Slider overtonesSlider;
    juce::Label overtonesLabel;

    juce::Slider releaseSlider;
    juce::Label releaseLabel;

    juce::ToggleButton pureToggle;
    int previousOvertoneValue = 8;

    juce::Label pluginTitleLabel;
    VoiceActivityMeter voiceMeter;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amplitudeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> overtonesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineWaveAudioProcessorEditor)
};