#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"

struct TTPreset
{
    const char* name;
    float tune, clean, eq, compress, air, space, polish;
};

class TuneTackleAudioProcessor : public juce::AudioProcessor
{
public:
    TuneTackleAudioProcessor();
    ~TuneTackleAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 2.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static const std::vector<TTPreset>& getPresets();
    void applyPreset (int index);

    juce::AudioProcessorValueTreeState apvts;

    // Shared with the editor (lock-free)
    std::atomic<float>  visPeak   { 0.0f };  // running max block peak, editor resets
    std::atomic<int>    keyIndex  { -1 };    // 0..11 = major keys C..B, 12..23 = minor, -1 unknown
    std::atomic<double> hostBpm   { 0.0 };
    std::atomic<double> hostTime  { 0.0 };
    std::atomic<int>    currentPreset { 0 };

private:
    void updateKeyEstimate();

    using IIRDup = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;

    std::atomic<float>* pTune     = nullptr;
    std::atomic<float>* pClean    = nullptr;
    std::atomic<float>* pEq       = nullptr;
    std::atomic<float>* pCompress = nullptr;
    std::atomic<float>* pAir      = nullptr;
    std::atomic<float>* pSpace    = nullptr;
    std::atomic<float>* pPolish   = nullptr;
    std::atomic<float>* pBypass   = nullptr;

    PitchDetector detector;
    PitchShifter  shifter[2];

    IIRDup hpf, mud, presence, airShelf;
    juce::dsp::NoiseGate<float>  gate;
    juce::dsp::Compressor<float> comp;
    juce::dsp::Reverb            reverb;

    double sampleRate = 44100.0;
    float lastHpfFreq = -1.0f, lastMudGain = -99.0f, lastPresGain = -99.0f, lastAirGain = -99.0f;

    float targetRatio = 1.0f, ratioNow = 1.0f, mixNow = 0.0f;
    std::vector<float> ratioBuf, mixBuf;

    float pcWeights[12] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneTackleAudioProcessor)
};
