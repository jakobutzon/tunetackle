#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto range = juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f);

    layout.add (std::make_unique<P> (juce::ParameterID { "tune",     1 }, "Tune",     range, 27.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "clean",    1 }, "Clean",    range, 55.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "eq",       1 }, "EQ",       range, 0.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "compress", 1 }, "Compress", range, 7.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "air",      1 }, "Air",      range, 0.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "space",    1 }, "Space",    range, 38.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "polish",   1 }, "Polish",   range, 72.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Before/After", false));
    return layout;
}

TuneTackleAudioProcessor::TuneTackleAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TuneTackle", createLayout())
{
    pTune     = apvts.getRawParameterValue ("tune");
    pClean    = apvts.getRawParameterValue ("clean");
    pEq       = apvts.getRawParameterValue ("eq");
    pCompress = apvts.getRawParameterValue ("compress");
    pAir      = apvts.getRawParameterValue ("air");
    pSpace    = apvts.getRawParameterValue ("space");
    pPolish   = apvts.getRawParameterValue ("polish");
    pBypass   = apvts.getRawParameterValue ("bypass");
}

const std::vector<TTPreset>& TuneTackleAudioProcessor::getPresets()
{
    static const std::vector<TTPreset> presets {
        { "Radio Ready",    27, 55,  0,  7,  0, 38, 72 },
        { "Natural",        12, 40, 15, 20, 10, 18, 55 },
        { "Pop Sheen",      45, 60, 40, 55, 65, 45, 80 },
        { "Trap Hard Tune", 95, 65, 45, 60, 50, 30, 85 },
        { "R&B Smooth",     60, 55, 30, 40, 45, 55, 75 },
        { "Rock Grit",      20, 45, 50, 65, 25, 35, 70 },
        { "Podcast Voice",   0, 70, 45, 60, 20,  5, 65 },
        { "Intimate Booth", 15, 50, 20, 30, 35, 70, 60 },
    };
    return presets;
}

void TuneTackleAudioProcessor::applyPreset (int index)
{
    const auto& presets = getPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    const auto& p = presets[(size_t) index];
    auto set = [this] (const char* id, float value)
    {
        if (auto* par = apvts.getParameter (id))
            par->setValueNotifyingHost (par->convertTo0to1 (value));
    };
    set ("tune", p.tune);
    set ("clean", p.clean);
    set ("eq", p.eq);
    set ("compress", p.compress);
    set ("air", p.air);
    set ("space", p.space);
    set ("polish", p.polish);
    currentPreset.store (index);
}

bool TuneTackleAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TuneTackleAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock,
                                  (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };

    hpf.prepare (spec);
    mud.prepare (spec);
    presence.prepare (spec);
    airShelf.prepare (spec);
    gate.prepare (spec);
    comp.prepare (spec);
    reverb.prepare (spec);

    *hpf.state      = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 60.0f);
    *mud.state      = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 260.0f, 0.8f, 1.0f);
    *presence.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 3400.0f, 0.7f, 1.0f);
    *airShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 11000.0f, 0.6f, 1.0f);
    lastHpfFreq = lastMudGain = lastPresGain = lastAirGain = -99.0f;

    gate.setRatio (4.0f);
    gate.setAttack (2.0f);
    gate.setRelease (120.0f);
    comp.setAttack (8.0f);
    comp.setRelease (140.0f);

    detector.prepare (sr);
    shifter[0].prepare (sr);
    shifter[1].prepare (sr);

    ratioNow = targetRatio = 1.0f;
    mixNow = 0.0f;
    ratioBuf.assign ((size_t) juce::jmax (16, samplesPerBlock), 1.0f);
    mixBuf.assign ((size_t) juce::jmax (16, samplesPerBlock), 0.0f);
}

void TuneTackleAudioProcessor::updateKeyEstimate()
{
    if (! (detector.conf > 0.55f && detector.freq > 70.0f && detector.freq < 950.0f))
        return;

    const float midi = 69.0f + 12.0f * std::log2 (detector.freq / 440.0f);
    int pc = juce::roundToInt (midi) % 12;
    if (pc < 0) pc += 12;

    float total = 0.0f;
    for (auto& w : pcWeights)
    {
        w *= 0.995f;
        total += w;
    }
    pcWeights[pc] += detector.conf;
    total += detector.conf;

    if (total < 8.0f)
        return;

    // Krumhansl-Schmuckler key profiles
    static const float major[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float minor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    auto correlate = [this] (const float* profile, int rot)
    {
        float sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float x = pcWeights[(i + rot) % 12];
            const float y = profile[i];
            sx += x; sy += y; sxx += x * x; syy += y * y; sxy += x * y;
        }
        const float denom = std::sqrt ((12.0f * sxx - sx * sx) * (12.0f * syy - sy * sy));
        return denom > 1.0e-9f ? (12.0f * sxy - sx * sy) / denom : 0.0f;
    };

    int best = -1;
    float bestR = -2.0f;
    for (int k = 0; k < 12; ++k)
    {
        const float rMaj = correlate (major, k);
        const float rMin = correlate (minor, k);
        if (rMaj > bestR) { bestR = rMaj; best = k; }
        if (rMin > bestR) { bestR = rMin; best = k + 12; }
    }
    keyIndex.store (best);
}

void TuneTackleAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (auto* ph = getPlayHead())
    {
        if (auto posInfo = ph->getPosition())
        {
            if (auto b = posInfo->getBpm())           hostBpm.store (*b);
            if (auto t = posInfo->getTimeInSeconds()) hostTime.store (*t);
        }
    }

    const float polish = pPolish->load() / 100.0f;
    const float mul = juce::jlimit (0.0f, 1.25f, polish / 0.72f);
    auto eff = [mul] (float v) { return juce::jlimit (0.0f, 1.0f, (v / 100.0f) * mul); };

    const float tuneEff  = eff (pTune->load());
    const float cleanEff = eff (pClean->load());
    const float eqEff    = eff (pEq->load());
    const float compEff  = eff (pCompress->load());
    const float airEff   = eff (pAir->load());
    const float spaceEff = eff (pSpace->load());
    const bool  bypassed = pBypass->load() > 0.5f;

    // Pitch detection runs even when bypassed so the key display stays live.
    if (detector.push (buffer))
    {
        updateKeyEstimate();
        if (detector.conf > 0.55f && detector.freq > 70.0f && detector.freq < 950.0f)
        {
            const float midi = 69.0f + 12.0f * std::log2 (detector.freq / 440.0f);
            const float corrSemis = ((float) juce::roundToInt (midi) - midi) * tuneEff;
            targetRatio = juce::jlimit (0.9f, 1.12f, std::pow (2.0f, corrSemis / 12.0f));
        }
        else
        {
            targetRatio = 1.0f;
        }
    }

    if (! bypassed)
    {
        const int numCh = juce::jmin (2, buffer.getNumChannels());
        juce::dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);

        // --- CLEAN: rumble filter + noise gate --------------------------------
        const float hpfFreq = 40.0f + 90.0f * cleanEff;
        if (std::abs (hpfFreq - lastHpfFreq) > 0.5f)
        {
            lastHpfFreq = hpfFreq;
            *hpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, hpfFreq);
        }
        hpf.process (ctx);

        gate.setThreshold (cleanEff > 0.001f ? -72.0f + 38.0f * cleanEff : -120.0f);
        gate.process (ctx);

        // --- TUNE: pitch correction -------------------------------------------
        if ((int) ratioBuf.size() < numSamples)
        {
            ratioBuf.resize ((size_t) numSamples, 1.0f);
            mixBuf.resize ((size_t) numSamples, 0.0f);
        }
        const float tau = juce::jmax (0.004f, 0.10f - 0.096f * tuneEff);
        const float rCoef = std::exp (-1.0f / (tau * (float) sampleRate));
        const float mCoef = std::exp (-1.0f / (0.012f * (float) sampleRate));
        const float mixTgt = tuneEff > 0.01f ? 1.0f : 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            ratioNow = targetRatio + (ratioNow - targetRatio) * rCoef;
            mixNow   = mixTgt + (mixNow - mixTgt) * mCoef;
            ratioBuf[(size_t) i] = ratioNow;
            mixBuf[(size_t) i]   = mixNow;
        }
        if (mixNow > 0.0005f || mixTgt > 0.0f)
            for (int ch = 0; ch < numCh; ++ch)
                shifter[ch].process (buffer.getWritePointer (ch), numSamples, ratioBuf.data(), mixBuf.data());

        // --- EQ: mud cut + presence -------------------------------------------
        const float mudGain = -4.5f * eqEff;
        if (std::abs (mudGain - lastMudGain) > 0.05f)
        {
            lastMudGain = mudGain;
            *mud.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sampleRate, 260.0f, 0.8f, juce::Decibels::decibelsToGain (mudGain));
        }
        mud.process (ctx);

        const float presGain = 4.5f * eqEff;
        if (std::abs (presGain - lastPresGain) > 0.05f)
        {
            lastPresGain = presGain;
            *presence.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sampleRate, 3400.0f, 0.7f, juce::Decibels::decibelsToGain (presGain));
        }
        presence.process (ctx);

        // --- COMPRESS -----------------------------------------------------------
        comp.setThreshold (-6.0f - 26.0f * compEff);
        comp.setRatio (1.0f + 3.5f * compEff);
        comp.process (ctx);
        buffer.applyGain (juce::Decibels::decibelsToGain (8.0f * compEff));

        // --- AIR: high shelf ------------------------------------------------------
        const float airGain = 6.0f * airEff;
        if (std::abs (airGain - lastAirGain) > 0.05f)
        {
            lastAirGain = airGain;
            *airShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sampleRate, 11000.0f, 0.6f, juce::Decibels::decibelsToGain (airGain));
        }
        airShelf.process (ctx);

        // --- SPACE: reverb ----------------------------------------------------------
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.50f + 0.28f * spaceEff;
        rp.damping = 0.45f;
        rp.wetLevel = 0.42f * spaceEff;
        rp.dryLevel = 1.0f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.process (ctx);
    }

    const float peak = buffer.getMagnitude (0, numSamples);
    float cur = visPeak.load();
    while (peak > cur && ! visPeak.compare_exchange_weak (cur, peak)) {}
}

void TuneTackleAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TuneTackleAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            currentPreset.store ((int) state.getProperty ("preset", 0));
            apvts.replaceState (state);
        }
    }
}

juce::AudioProcessorEditor* TuneTackleAudioProcessor::createEditor()
{
    return new TuneTackleAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TuneTackleAudioProcessor();
}
