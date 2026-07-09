#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

// Dual-tap "doppler" delay-line pitch shifter. Cheap, low-latency, good enough
// for gentle vocal correction through hard-tune style effects.
class PitchShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        windowSamples = (float) (0.030 * sr);
        bufSize = juce::nextPowerOfTwo (juce::jmax (4096, (int) (sr / 4)));
        mask = bufSize - 1;
        buf.assign ((size_t) bufSize, 0.0f);
        writePos = 0;
        phase = 0.0f;
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        phase = 0.0f;
    }

    // ratios: per-sample pitch ratio (1.0 = no shift). mix: per-sample dry/wet.
    void process (float* data, int n, const float* ratios, const float* mix)
    {
        const float w = windowSamples;
        for (int i = 0; i < n; ++i)
        {
            buf[(size_t) writePos] = data[i];

            phase += (1.0f - ratios[i]);
            while (phase >= w)   phase -= w;
            while (phase < 0.0f) phase += w;

            float dA = phase;
            float dB = dA + w * 0.5f;
            if (dB >= w) dB -= w;

            const float gA = std::sin (juce::MathConstants<float>::pi * dA / w);
            const float gB = std::sin (juce::MathConstants<float>::pi * dB / w);

            const float wet = gA * readInterp (dA) + gB * readInterp (dB);
            data[i] = data[i] * (1.0f - mix[i]) + wet * mix[i];

            writePos = (writePos + 1) & mask;
        }
    }

private:
    float readInterp (float delay) const
    {
        const float fpos = (float) writePos - delay - 2.0f;
        int ip = (int) std::floor (fpos);
        const float fr = fpos - (float) ip;
        const float a = buf[(size_t) ((ip + bufSize) & mask)];
        const float b = buf[(size_t) ((ip + 1 + bufSize) & mask)];
        return a + fr * (b - a);
    }

    double sr = 44100.0;
    float windowSamples = 1323.0f;
    int bufSize = 0, mask = 0, writePos = 0;
    float phase = 0.0f;
    std::vector<float> buf;
};

// Autocorrelation pitch detector running on a 2x-decimated mono stream.
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ring.assign ((size_t) ringSize, 0.0f);
        pos = 0;
        sinceHop = 0;
        freq = 0.0f;
        conf = 0.0f;
        rvals.assign (1024, 0.0f);
    }

    // Mixes the buffer to mono and accumulates. Returns true when a new
    // analysis was performed (freq/conf updated).
    bool push (const juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int nCh = juce::jmax (1, buffer.getNumChannels());
        bool analysed = false;

        for (int i = 0; i < n; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < nCh; ++ch)
                s += buffer.getReadPointer (ch)[i];
            ring[(size_t) pos] = s / (float) nCh;
            pos = (pos + 1) % ringSize;

            if (++sinceHop >= hop)
            {
                sinceHop = 0;
                analyse();
                analysed = true;
            }
        }
        return analysed;
    }

    float freq = 0.0f;   // Hz, valid when conf is high
    float conf = 0.0f;   // 0..1
    float rms  = 0.0f;

private:
    void analyse()
    {
        constexpr int win = 1024;               // decimated x2 -> spans 2048 native samples
        float x[win];
        int start = pos - win * 2;
        while (start < 0) start += ringSize;
        for (int i = 0; i < win; ++i)
            x[i] = ring[(size_t) ((start + i * 2) % ringSize)];

        double e0 = 0.0;
        for (int i = 0; i < win; ++i)
            e0 += (double) x[i] * x[i];
        rms = (float) std::sqrt (e0 / win);

        if (rms < 3.0e-4)
        {
            conf = 0.0f;
            return;
        }

        const double sr2 = sr * 0.5;
        const int minLag = juce::jmax (2,  (int) (sr2 / 950.0));
        const int maxLag = juce::jmin (win - 32, (int) (sr2 / 65.0));

        int bestLag = 0;
        float bestVal = 0.0f;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double num = 0.0;
            const int len = win - lag;
            for (int i = 0; i < len; ++i)
                num += (double) x[i] * x[i + lag];

            const float r = (float) ((num / (e0 + 1.0e-12)) * ((double) win / (double) len));
            rvals[(size_t) lag] = r;
            if (r > bestVal)
            {
                bestVal = r;
                bestLag = lag;
            }
        }

        if (bestLag == 0 || bestVal < 0.4f)
        {
            conf = bestVal;
            return;
        }

        // Prefer a smaller lag (higher octave) if nearly as strong — fixes
        // the classic octave-down autocorrelation error.
        for (int div = 4; div >= 2; --div)
        {
            const int l2 = juce::roundToInt ((float) bestLag / (float) div);
            if (l2 >= minLag && rvals[(size_t) l2] > 0.88f * bestVal)
            {
                bestLag = l2;
                bestVal = rvals[(size_t) l2];
                break;
            }
        }

        // Parabolic refinement
        float refined = (float) bestLag;
        if (bestLag > minLag && bestLag < maxLag)
        {
            const float rm = rvals[(size_t) (bestLag - 1)];
            const float r0 = rvals[(size_t) bestLag];
            const float rp = rvals[(size_t) (bestLag + 1)];
            const float denom = rm - 2.0f * r0 + rp;
            if (std::abs (denom) > 1.0e-9f)
                refined += 0.5f * (rm - rp) / denom;
        }

        conf = bestVal;
        freq = (float) (sr2 / (double) refined);
    }

    static constexpr int ringSize = 8192;
    static constexpr int hop = 1024;

    double sr = 44100.0;
    std::vector<float> ring, rvals;
    int pos = 0, sinceHop = 0;
};
