#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ttc
{
    const juce::Colour bg          { 0xff060608 };
    const juce::Colour panel       { 0xff121218 };
    const juce::Colour panelStroke { 0xff232330 };
    const juce::Colour card        { 0xff17171d };
    const juce::Colour cardStroke  { 0xff26262e };
    const juce::Colour display     { 0xff0c0c10 };
    const juce::Colour lime        { 0xffc6f542 };
    const juce::Colour limeDim     { 0xff6f7a36 };
    const juce::Colour text        { 0xfff2f2f5 };
    const juce::Colour sub         { 0xff8f8f99 };
    const juce::Colour track       { 0xff26262d };
    const juce::Colour barGrey     { 0xff56565f };

    inline juce::Font font (float height, bool bold = true, bool italic = false)
    {
        int style = juce::Font::plain;
        if (bold)   style |= juce::Font::bold;
        if (italic) style |= juce::Font::italic;
        return juce::Font (juce::FontOptions ("Helvetica Neue", height, style));
    }
}

class TTLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TTLookAndFeel();
    bool processing = true;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ttc::font (16.0f); }
};

// ---------------------------------------------------------------------------
class WaveDisplay : public juce::Component
{
public:
    WaveDisplay() { hist.assign (64, 0.0f); }

    void push (float v)
    {
        std::rotate (hist.begin(), hist.begin() + 1, hist.end());
        hist.back() = juce::jlimit (0.0f, 1.0f, v);
    }

    bool processing = true;
    juce::String timeStr { "00:00.0" }, keyStr { "KEY: LISTENING" }, dbStr { "-∞ dB" };

    void paint (juce::Graphics&) override;

private:
    std::vector<float> hist;
};

// ---------------------------------------------------------------------------
class KnobCard : public juce::Component
{
public:
    KnobCard (const juce::String& title,
              juce::AudioProcessorValueTreeState& state,
              const juce::String& paramID,
              std::function<bool()> processingFn);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::String name;
    juce::Slider knob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::function<bool()> isProcessing;
};

// ---------------------------------------------------------------------------
class PolishBar : public juce::Component
{
public:
    PolishBar (juce::AudioProcessorValueTreeState& state, std::function<bool()> processingFn);

    void resized() override;
    void paint (juce::Graphics&) override;

    float level = 0.0f;
    float phase = 0.0f;

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::function<bool()> isProcessing;
};

// ---------------------------------------------------------------------------
class SegToggle : public juce::Component
{
public:
    std::function<bool()> getBypassed;
    std::function<void (bool)> setBypassed;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
};

// ---------------------------------------------------------------------------
class TuneTackleAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit TuneTackleAudioProcessorEditor (TuneTackleAudioProcessor&);
    ~TuneTackleAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    TuneTackleAudioProcessor& processor;
    TTLookAndFeel laf;

    juce::ComboBox presetBox;
    SegToggle toggle;
    WaveDisplay wave;
    juce::OwnedArray<KnobCard> cards;
    std::unique_ptr<PolishBar> polishBar;

    float smoothLevel = 0.0f;
    double localTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneTackleAudioProcessorEditor)
};
