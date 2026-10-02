#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

/*  Monophonic pitch detector (YIN) used to know WHICH note is playing, so the
    scale quantizer can place harmonies on real scale degrees.  Runs on a ~11 kHz
    decimated copy of the signal; one estimate every ~11 ms.  70 Hz .. 1 kHz.     */
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        dsFactor = std::max (1, (int) std::lround (sampleRate / 11025.0));
        fs = sampleRate / dsFactor;
        tauMax = (int) (fs / 70.0);  tauMin = std::max (2, (int) (fs / 1000.0));
        const int total = W + tauMax;
        ring.assign ((size_t) total + 8, 0.f);  lin.assign ((size_t) total, 0.f);
        d.assign ((size_t) tauMax + 2, 0.f);    cm.assign ((size_t) tauMax + 2, 1.f);
        reset();
    }
    void reset() { std::fill (ring.begin(), ring.end(), 0.f); pos = 0; cnt = 0; acc = 0.f; since = 0; voiced = false; midi = 0.f; }

    /** feed one sample; returns true when a fresh estimate is available */
    bool push (float x)
    {
        acc += x;
        if (++cnt < dsFactor) return false;
        cnt = 0;  ring[(size_t) pos] = acc / (float) dsFactor;  acc = 0.f;
        pos = (pos + 1) % (int) ring.size();
        if (++since < hop) return false;
        since = 0;  analyse();  return true;
    }
    bool  isVoiced() const { return voiced; }
    float getMidi()  const { return midi; }

private:
    void analyse()
    {
        const int N = (int) ring.size(), total = W + tauMax;
        int start = ((pos - total) % N + N) % N;
        for (int i = 0; i < total; ++i) lin[(size_t) i] = ring[(size_t) ((start + i) % N)];

        float e = 0.f;  for (int j = 0; j < W; ++j) e += lin[(size_t) j] * lin[(size_t) j];
        if (std::sqrt (e / (float) W) < 1.0e-3f) { voiced = false; return; }

        for (int t = 1; t <= tauMax; ++t)
        {
            float s = 0.f;
            for (int j = 0; j < W; ++j) { const float df = lin[(size_t) j] - lin[(size_t) (j + t)]; s += df * df; }
            d[(size_t) t] = s;
        }
        float run = 0.f;  cm[0] = 1.f;
        for (int t = 1; t <= tauMax; ++t) { run += d[(size_t) t]; cm[(size_t) t] = run > 0.f ? d[(size_t) t] * (float) t / run : 1.f; }

        int tau = -1;
        for (int t = tauMin; t <= tauMax; ++t)
            if (cm[(size_t) t] < 0.15f) { while (t + 1 <= tauMax && cm[(size_t) t + 1] < cm[(size_t) t]) ++t; tau = t; break; }
        if (tau < 0)
        {
            int best = tauMin;
            for (int t = tauMin; t <= tauMax; ++t) if (cm[(size_t) t] < cm[(size_t) best]) best = t;
            if (cm[(size_t) best] > 0.30f) { voiced = false; return; }
            tau = best;
        }
        float tEst = (float) tau;
        if (tau > 1 && tau < tauMax)
        {
            const float a = cm[(size_t) tau - 1], b = cm[(size_t) tau], c = cm[(size_t) tau + 1], den = a - 2.f * b + c;
            if (std::abs (den) > 1.0e-9f) tEst += 0.5f * (a - c) / den;
        }
        const float f = (float) fs / tEst;
        midi = 69.f + 12.f * std::log2 (f / 440.f);  voiced = true;
    }

    static constexpr int W = 512, hop = 128;
    int dsFactor = 4, tauMax = 157, tauMin = 11, pos = 0, cnt = 0, since = 0;
    double fs = 11025.0;  float acc = 0.f, midi = 0.f;  bool voiced = false;
    std::vector<float> ring, lin, d, cm;
};
