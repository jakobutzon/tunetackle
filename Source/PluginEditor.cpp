#include "PluginEditor.h"

// ============================================================ LookAndFeel ==
TTLookAndFeel::TTLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, ttc::card);
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffd0d0d6));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ttc::lime.withAlpha (0.16f));
    setColour (juce::PopupMenu::highlightedTextColourId, ttc::lime);
    setColour (juce::ComboBox::textColourId, ttc::text);
}

void TTLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                      float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                      juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const auto accent = processing ? ttc::lime : ttc::limeDim;

    // track arc
    juce::Path track;
    track.addCentredArc (cx, cy, radius - 2.5f, radius - 2.5f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (ttc::track);
    g.strokePath (track, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // value arc
    if (sliderPos > 0.003f)
    {
        juce::Path val;
        val.addCentredArc (cx, cy, radius - 2.5f, radius - 2.5f, 0.0f,
                           rotaryStartAngle, angle, true);
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // knob body
    const float bodyR = radius - 8.0f;
    juce::ColourGradient grad (juce::Colour (0xff2c2c34), cx - bodyR, cy - bodyR,
                               juce::Colour (0xff121216), cx + bodyR, cy + bodyR, false);
    g.setGradientFill (grad);
    g.fillEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colour (0xff35353f));
    g.drawEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);

    // pointer
    juce::Path pointer;
    pointer.startNewSubPath (cx + 0.35f * bodyR * std::sin (angle), cy - 0.35f * bodyR * std::cos (angle));
    pointer.lineTo (cx + 0.80f * bodyR * std::sin (angle), cy - 0.80f * bodyR * std::cos (angle));
    g.setColour (processing ? ttc::lime : juce::Colour (0xff9a9aa2));
    g.strokePath (pointer, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

void TTLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                      float sliderPos, float, float, juce::Slider::SliderStyle,
                                      juce::Slider&)
{
    const auto accent = processing ? ttc::lime : ttc::limeDim;
    const float cy = (float) y + (float) h * 0.5f;

    juce::Rectangle<float> trackRect ((float) x, cy - 3.0f, (float) w, 6.0f);
    g.setColour (ttc::track);
    g.fillRoundedRectangle (trackRect, 3.0f);

    juce::Rectangle<float> fill ((float) x, cy - 3.0f, sliderPos - (float) x, 6.0f);
    if (fill.getWidth() > 1.0f)
    {
        g.setColour (accent);
        g.fillRoundedRectangle (fill, 3.0f);
    }

    const float r = 10.0f;
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (sliderPos - r, cy - r + 1.5f, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (sliderPos - r, cy - r, r * 2.0f, r * 2.0f);
}

void TTLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                  int, int, int, int, juce::ComboBox&)
{
    juce::Rectangle<float> b (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (juce::Colour (0xff15151b));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (ttc::cardStroke);
    g.drawRoundedRectangle (b.reduced (0.6f), 10.0f, 1.2f);

    g.setColour (ttc::sub);
    g.setFont (ttc::font (15.0f, false));
    g.drawText ("Preset:", 16, 0, 64, height, juce::Justification::centredLeft);

    // little arrow
    juce::Path arrow;
    const float ax = (float) width - 22.0f;
    const float ay = (float) height * 0.5f - 2.0f;
    arrow.addTriangle (ax, ay, ax + 8.0f, ay, ax + 4.0f, ay + 5.0f);
    g.setColour (ttc::sub);
    g.fillPath (arrow);
}

void TTLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (84, 1, box.getWidth() - 84 - 32, box.getHeight() - 2);
    label.setFont (ttc::font (16.0f));
    label.setJustificationType (juce::Justification::centredLeft);
}

// ============================================================ WaveDisplay ==
void WaveDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (ttc::display);
    g.fillRoundedRectangle (b, 16.0f);
    g.setColour (juce::Colour (0xff1e1e26));
    g.drawRoundedRectangle (b.reduced (0.6f), 16.0f, 1.2f);

    const auto accent = ttc::lime;

    // header row
    g.setColour (juce::Colour (0xffc9c9d2));
    g.setFont (ttc::font (13.5f).withExtraKerningFactor (0.14f));
    g.drawText (juce::String::fromUTF8 ("VOCAL INPUT \xe2\x80\x94 LIVE"), 28, 18, 320, 20,
                juce::Justification::centredLeft);

    const juce::String status = processing ? "PROCESSING" : "BYPASSED";
    g.setColour (accent);
    g.setFont (ttc::font (13.5f).withExtraKerningFactor (0.14f));
    const int statusW = 120;
    g.drawText (status, (int) b.getWidth() - 28 - statusW, 18, statusW, 20,
                juce::Justification::centredRight);
    g.fillEllipse (b.getWidth() - 28.0f - (float) statusW - 14.0f, 24.5f, 7.0f, 7.0f);

    // footer row
    g.setColour (ttc::sub);
    g.setFont (ttc::font (14.0f).withExtraKerningFactor (0.10f));
    const int fy = (int) b.getHeight() - 38;
    g.drawText (timeStr, 28, fy, 200, 20, juce::Justification::centredLeft);
    g.drawText (keyStr, 0, fy, (int) b.getWidth(), 20, juce::Justification::centred);
    g.drawText (dbStr, (int) b.getWidth() - 228, fy, 200, 20, juce::Justification::centredRight);

    // bars
    auto area = juce::Rectangle<float> (28.0f, 52.0f, b.getWidth() - 56.0f, b.getHeight() - 52.0f - 50.0f);
    const int n = (int) hist.size();
    const float pitch = area.getWidth() / (float) n;
    const float barW = pitch * 0.58f;
    const float cy = area.getCentreY();
    const float maxH = area.getHeight();

    g.setColour (processing ? accent : ttc::barGrey);
    for (int i = 0; i < n; ++i)
    {
        const float v = hist[(size_t) i];
        const float hgt = juce::jlimit (5.0f, maxH, 5.0f + std::pow (v, 0.65f) * (maxH - 5.0f));
        juce::Rectangle<float> bar (area.getX() + (float) i * pitch + (pitch - barW) * 0.5f,
                                    cy - hgt * 0.5f, barW, hgt);
        g.fillRoundedRectangle (bar, juce::jmin (barW * 0.5f, 3.5f));
    }
}

// =============================================================== KnobCard ==
KnobCard::KnobCard (const juce::String& title,
                    juce::AudioProcessorValueTreeState& state,
                    const juce::String& paramID,
                    std::function<bool()> processingFn)
    : name (title), isProcessing (std::move (processingFn))
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    addAndMakeVisible (knob);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, knob);
    knob.onValueChange = [this] { repaint(); };
}

void KnobCard::resized()
{
    const int size = juce::jmin (getWidth() - 20, 100);
    knob.setBounds ((getWidth() - size) / 2, 42, size, size);
}

void KnobCard::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool proc = isProcessing();

    g.setColour (ttc::card);
    g.fillRoundedRectangle (b, 14.0f);

    if (proc)
    {
        g.setColour (ttc::lime.withAlpha (0.10f));
        g.drawRoundedRectangle (b.reduced (1.0f), 14.0f, 4.0f);
        g.setColour (ttc::lime.withAlpha (0.55f));
        g.drawRoundedRectangle (b, 14.0f, 1.2f);
    }
    else
    {
        g.setColour (ttc::cardStroke);
        g.drawRoundedRectangle (b, 14.0f, 1.0f);
    }

    g.setColour (ttc::text);
    g.setFont (ttc::font (14.0f).withExtraKerningFactor (0.14f));
    g.drawText (name.toUpperCase(), 0, 16, getWidth(), 20, juce::Justification::centred);

    g.setFont (ttc::font (17.0f));
    g.setColour (ttc::text);
    g.drawText (juce::String (juce::roundToInt (knob.getValue())) + "%",
                0, getHeight() - 34, getWidth(), 22, juce::Justification::centred);
}

// ============================================================== PolishBar ==
PolishBar::PolishBar (juce::AudioProcessorValueTreeState& state, std::function<bool()> processingFn)
    : isProcessing (std::move (processingFn))
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, "polish", slider);
    slider.onValueChange = [this] { repaint(); };
}

void PolishBar::resized()
{
    slider.setBounds (128, (getHeight() - 30) / 2, getWidth() - 128 - 200, 30);
}

void PolishBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool proc = isProcessing();
    const auto accent = proc ? ttc::lime : ttc::limeDim;

    g.setColour (ttc::card);
    g.fillRoundedRectangle (b, 14.0f);
    g.setColour (proc ? ttc::lime.withAlpha (0.35f) : ttc::cardStroke);
    g.drawRoundedRectangle (b, 14.0f, proc ? 1.2f : 1.0f);

    g.setColour (ttc::text);
    g.setFont (ttc::font (15.0f).withExtraKerningFactor (0.14f));
    g.drawText ("POLISH", 26, 0, 90, getHeight(), juce::Justification::centredLeft);

    g.setColour (accent);
    g.setFont (ttc::font (18.0f));
    g.drawText (juce::String (juce::roundToInt (slider.getValue())) + "%",
                getWidth() - 192, 0, 64, getHeight(), juce::Justification::centredLeft);

    // mini output meter
    const float baseH[5] = { 0.35f, 0.50f, 0.68f, 1.00f, 0.55f };
    const float mx = (float) getWidth() - 110.0f;
    const float mw = 80.0f;
    const float bw = 9.0f;
    const float gap = (mw - 5.0f * bw) / 4.0f;
    const float maxH = (float) getHeight() * 0.52f;
    const float baseline = ((float) getHeight() + maxH) * 0.5f;
    const float drive = juce::jlimit (0.0f, 1.0f, level * 2.2f);

    g.setColour (accent);
    for (int i = 0; i < 5; ++i)
    {
        const float anim = 0.22f + 0.78f * drive * (0.72f + 0.28f * std::sin (phase * 2.0f + (float) i * 1.3f));
        const float hgt = juce::jmax (4.0f, maxH * baseH[i] * anim);
        g.fillRoundedRectangle (mx + (float) i * (bw + gap), baseline - hgt, bw, hgt, 2.5f);
    }
}

// ============================================================== SegToggle ==
void SegToggle::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff15151b));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (ttc::cardStroke);
    g.drawRoundedRectangle (b.reduced (0.6f), 10.0f, 1.2f);

    const bool byp = getBypassed();
    auto before = b.removeFromLeft (b.getWidth() * 0.5f).reduced (3.0f);
    auto after = b.reduced (3.0f);

    g.setColour (ttc::lime);
    g.fillRoundedRectangle (byp ? before : after, 8.0f);

    g.setFont (ttc::font (15.5f));
    g.setColour (byp ? juce::Colours::black : juce::Colour (0xffcfcfd6));
    g.drawText ("Before", before, juce::Justification::centred);
    g.setColour (byp ? juce::Colour (0xffcfcfd6) : juce::Colours::black);
    g.drawText ("After", after, juce::Justification::centred);
}

void SegToggle::mouseDown (const juce::MouseEvent& e)
{
    setBypassed (e.x < getWidth() / 2);
    repaint();
}

// ================================================================= Editor ==
TuneTackleAudioProcessorEditor::TuneTackleAudioProcessorEditor (TuneTackleAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&laf);

    auto processingFn = [this] { return processor.apvts.getRawParameterValue ("bypass")->load() < 0.5f; };

    // preset menu
    const auto& presets = TuneTackleAudioProcessor::getPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (presets[(size_t) i].name, i + 1);
    presetBox.setSelectedId (processor.currentPreset.load() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != processor.currentPreset.load())
            processor.applyPreset (idx);
    };
    addAndMakeVisible (presetBox);

    // before / after
    toggle.getBypassed = [this] { return processor.apvts.getRawParameterValue ("bypass")->load() > 0.5f; };
    toggle.setBypassed = [this] (bool b)
    {
        if (auto* par = processor.apvts.getParameter ("bypass"))
            par->setValueNotifyingHost (b ? 1.0f : 0.0f);
    };
    addAndMakeVisible (toggle);

    addAndMakeVisible (wave);

    const char* titles[6] = { "Tune", "Clean", "EQ", "Compress", "Air", "Space" };
    const char* ids[6]    = { "tune", "clean", "eq", "compress", "air", "space" };
    for (int i = 0; i < 6; ++i)
        addAndMakeVisible (cards.add (new KnobCard (titles[i], processor.apvts, ids[i], processingFn)));

    polishBar = std::make_unique<PolishBar> (processor.apvts, processingFn);
    addAndMakeVisible (*polishBar);

    setSize (1040, 740);
    startTimerHz (30);
}

TuneTackleAudioProcessorEditor::~TuneTackleAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void TuneTackleAudioProcessorEditor::resized()
{
    auto panel = getLocalBounds().reduced (14);
    auto inner = panel.reduced (26, 0);

    // header
    auto header = inner.removeFromTop (86);
    presetBox.setBounds (header.getCentreX() - 130, header.getCentreY() - 20, 260, 40);
    toggle.setBounds (header.getRight() - 208, header.getCentreY() - 20, 208, 40);

    inner.removeFromTop (8);
    wave.setBounds (inner.removeFromTop (244));

    inner.removeFromTop (20);
    auto knobRow = inner.removeFromTop (188);
    const int gap = 14;
    const int cardW = (knobRow.getWidth() - gap * 5) / 6;
    for (int i = 0; i < 6; ++i)
        cards[i]->setBounds (knobRow.getX() + i * (cardW + gap), knobRow.getY(), cardW, knobRow.getHeight());

    inner.removeFromTop (18);
    polishBar->setBounds (inner.removeFromTop (82));
}

void TuneTackleAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ttc::bg);

    auto panel = getLocalBounds().toFloat().reduced (14.0f);
    g.setColour (ttc::panel);
    g.fillRoundedRectangle (panel, 22.0f);
    g.setColour (ttc::panelStroke);
    g.drawRoundedRectangle (panel.reduced (0.6f), 22.0f, 1.2f);

    // header divider
    const float hy = panel.getY() + 86.0f;
    g.setColour (juce::Colour (0xff1e1e26));
    g.drawLine (panel.getX() + 1.0f, hy, panel.getRight() - 1.0f, hy, 1.0f);

    // ---- logo ----
    const float lx = panel.getX() + 30.0f;
    const float cy = panel.getY() + 43.0f;

    g.setColour (ttc::lime);
    // waveform ticks
    g.fillRoundedRectangle (lx,        cy - 5.0f, 3.0f, 10.0f, 1.5f);
    g.fillRoundedRectangle (lx + 6.0f, cy - 9.0f, 3.0f, 18.0f, 1.5f);
    g.fillRoundedRectangle (lx + 34.0f, cy - 9.0f, 3.0f, 18.0f, 1.5f);
    g.fillRoundedRectangle (lx + 40.0f, cy - 5.0f, 3.0f, 10.0f, 1.5f);
    // mic capsule
    const float mcx = lx + 21.5f;
    g.fillRoundedRectangle (mcx - 4.5f, cy - 14.0f, 9.0f, 17.0f, 4.5f);
    // mic cradle
    juce::Path cradle;
    cradle.addCentredArc (mcx, cy - 1.0f, 8.5f, 8.5f, 0.0f,
                          juce::MathConstants<float>::halfPi,
                          juce::MathConstants<float>::pi * 1.5f, true);
    g.strokePath (cradle, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    g.fillRoundedRectangle (mcx - 1.1f, cy + 7.5f, 2.2f, 5.5f, 1.1f);
    g.fillRoundedRectangle (mcx - 5.0f, cy + 13.0f, 10.0f, 2.2f, 1.1f);

    g.setColour (ttc::text);
    g.setFont (ttc::font (21.0f, true, true).withExtraKerningFactor (0.04f));
    g.drawText ("TUNETACKLE", (int) (lx + 54.0f), (int) (cy - 13.0f), 220, 26,
                juce::Justification::centredLeft);
}

void TuneTackleAudioProcessorEditor::timerCallback()
{
    const float pk = processor.visPeak.exchange (0.0f);
    smoothLevel = juce::jmax (pk, smoothLevel * 0.80f);

    const bool byp = processor.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    laf.processing = ! byp;
    wave.processing = ! byp;

    // time readout: host position if available, else our own run clock
    double t = processor.hostTime.load();
    if (t < 0.01)
    {
        if (smoothLevel > 0.02f)
            localTime += 1.0 / 30.0;
        t = localTime;
    }
    const int mins = (int) (t / 60.0);
    const int secs = (int) t % 60;
    const int tenths = (int) (t * 10.0) % 10;
    wave.timeStr = juce::String::formatted ("%02d:%02d.%d", mins, secs, tenths);

    // key + bpm readout
    static const char* names[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    const int k = processor.keyIndex.load();
    juce::String ks = "KEY: LISTENING";
    if (k >= 0)
        ks = juce::String ("KEY: ") + names[k % 12] + (k < 12 ? " MAJOR" : " MINOR");
    const double bpm = processor.hostBpm.load();
    if (bpm > 1.0)
        ks += juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String ((int) std::round (bpm)) + " BPM";
    wave.keyStr = ks;

    const float db = juce::Decibels::gainToDecibels (juce::jmax (smoothLevel, 1.0e-3f));
    wave.dbStr = db <= -59.9f ? juce::String (juce::CharPointer_UTF8 ("-\xe2\x88\x9e dB"))
                              : juce::String (db, 1) + " dB";

    wave.push (smoothLevel);
    polishBar->level = smoothLevel;
    polishBar->phase += 0.25f;

    const int wantId = processor.currentPreset.load() + 1;
    if (presetBox.getSelectedId() != wantId)
        presetBox.setSelectedId (wantId, juce::dontSendNotification);

    repaint();
}
