#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <cstring>
#include <cmath>

/*  Phase-vocoder pitch shifter with Laroche–Dolson peak tracking + rigid phase locking.
    Naive phase vocoders sound "metallic"/phasey because every bin advances its phase
    independently, destroying the phase coherence of each sinusoidal partial's main lobe.
    Here, each spectral peak advances its own phase (tracked frame to frame) and all bins
    in its region of influence keep their ORIGINAL phase offset relative to it, so
    partials stay coherent.  4096-pt FFT, 8x overlap (hop 512), Hann window.           */
class PitchShifter
{
public:
    static constexpr int fftSize = 4096, osamp = 8, hop = fftSize / osamp, half = fftSize / 2;
    static constexpr int latency = fftSize - hop;

    PitchShifter() : fft (12)
    {
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / fftSize);

        // Calibrate the FFT's inverse scaling so we don't depend on backend conventions.
        std::vector<juce::dsp::Complex<float>> a (fftSize), b (fftSize);
        a[0] = 1.0f;  fft.perform (a.data(), b.data(), false);
        a = b;        fft.perform (a.data(), b.data(), true);
        ifftScale = 1.0f / std::max (1.0e-12f, b[0].real());
        outScale  = ifftScale / (osamp * 0.375f);          // Hann^2 overlap-add gain = osamp * 3/8
        reset();
    }

    void reset()
    {
        inFifo.assign (fftSize, 0.f);  outFifo.assign (fftSize, 0.f);  accum.assign (2 * fftSize, 0.f);
        lastPhase.assign (half + 1, 0.f);  prevSynPhase.assign (half + 1, 0.f);
        prevPeaks.clear();  rover = latency;
    }

    void setRatio (float r) noexcept { ratio = r; }

    float process (float x) noexcept
    {
        inFifo[(size_t) rover] = x;
        const float y = outFifo[(size_t) (rover - latency)];
        if (++rover >= fftSize)
        {
            rover = latency;
            processFrame();
            std::copy (accum.begin(), accum.begin() + hop, outFifo.begin());
            std::memmove (accum.data(), accum.data() + hop, sizeof (float) * (size_t) fftSize);
            std::fill (accum.begin() + fftSize, accum.end(), 0.f);
            std::memmove (inFifo.data(), inFifo.data() + hop, sizeof (float) * (size_t) latency);
        }
        return y;
    }

private:
    using C = juce::dsp::Complex<float>;
    static constexpr float twoPi = juce::MathConstants<float>::twoPi;

    void processFrame()
    {
        for (int i = 0; i < fftSize; ++i) td[(size_t) i] = C (inFifo[(size_t) i] * window[(size_t) i], 0.f);
        fft.perform (td.data(), fd.data(), false);

        // ---- analysis: magnitude, phase, true (instantaneous) frequency in bins
        const float expected = twoPi * (float) hop / fftSize;
        for (int k = 0; k <= half; ++k)
        {
            mag[(size_t) k] = std::abs (fd[(size_t) k]);
            const float ph  = std::arg (fd[(size_t) k]);
            float d = ph - lastPhase[(size_t) k] - expected * (float) k;
            lastPhase[(size_t) k] = ph;
            d -= twoPi * std::round (d / twoPi);
            phase[(size_t) k]   = ph;
            trueBin[(size_t) k] = (float) k + d * (float) osamp / twoPi;
        }

        // ---- shift (inverse-mapped so there are no spectral holes)
        for (int j = 0; j <= half; ++j)
        {
            const float src = (float) j / ratio;
            if (src >= (float) half) { sMag[(size_t) j] = 0.f; sFreq[(size_t) j] = (float) j; sAna[(size_t) j] = 0.f; continue; }
            const int k0 = (int) src, k1 = k0 + 1;  const float fr = src - (float) k0;
            const int kn = fr < 0.5f ? k0 : k1;
            sMag[(size_t) j]  = mag[(size_t) k0] * (1.f - fr) + mag[(size_t) k1] * fr;
            sFreq[(size_t) j] = trueBin[(size_t) kn] * ratio;
            sAna[(size_t) j]  = phase[(size_t) kn];
        }

        // ---- peak picking
        peaks.clear();
        for (int j = 2; j < half - 2; ++j)
        {
            const float m = sMag[(size_t) j];
            if (m > 1.0e-7f && m > sMag[(size_t) j-1] && m > sMag[(size_t) j-2] && m >= sMag[(size_t) j+1] && m >= sMag[(size_t) j+2])
                peaks.push_back (j);
        }

        // ---- synthesis phases with rigid phase locking
        const float adv = twoPi * (float) hop / fftSize;
        if (peaks.empty())
            for (int j = 0; j <= half; ++j) synPhase[(size_t) j] = sAna[(size_t) j];
        else
        {
            size_t pp = 0;
            for (size_t n = 0; n < peaks.size(); ++n)
            {
                const int p  = peaks[n];
                const int lo = n == 0 ? 0 : (peaks[n-1] + p) / 2 + 1;
                const int hi = n + 1 == peaks.size() ? half : (p + peaks[n+1]) / 2;

                float base = sAna[(size_t) p];                      // first frame / new partial
                if (! prevPeaks.empty())
                {
                    while (pp + 1 < prevPeaks.size() && std::abs (prevPeaks[pp+1] - p) < std::abs (prevPeaks[pp] - p)) ++pp;
                    base = prevSynPhase[(size_t) prevPeaks[pp]] + sFreq[(size_t) p] * adv;
                }
                for (int j = lo; j <= hi; ++j)
                    synPhase[(size_t) j] = base + sAna[(size_t) j] - sAna[(size_t) p];
            }
        }
        for (int j = 0; j <= half; ++j)
        {
            synPhase[(size_t) j] -= twoPi * std::floor (synPhase[(size_t) j] / twoPi);
            prevSynPhase[(size_t) j] = synPhase[(size_t) j];
        }
        prevPeaks = peaks;

        // ---- resynthesis
        for (int j = 0; j <= half; ++j)
            fd[(size_t) j] = std::polar (sMag[(size_t) j], synPhase[(size_t) j]);
        fd[0] = C (fd[0].real(), 0.f);  fd[(size_t) half] = C (fd[(size_t) half].real(), 0.f);
        for (int j = 1; j < half; ++j) fd[(size_t) (fftSize - j)] = std::conj (fd[(size_t) j]);
        fft.perform (fd.data(), td.data(), true);

        for (int i = 0; i < fftSize; ++i)
            accum[(size_t) i] += td[(size_t) i].real() * window[(size_t) i] * outScale;
    }

    juce::dsp::FFT fft;
    float ratio = 1.f, ifftScale = 1.f, outScale = 1.f;
    int rover = latency;
    std::array<float, fftSize> window {}, mag {}, phase {}, trueBin {}, synPhase {};
    std::array<float, half + 1> sMag {}, sFreq {}, sAna {};
    std::array<C, fftSize> td {}, fd {};
    std::vector<float> inFifo, outFifo, accum, lastPhase, prevSynPhase;
    std::vector<int> peaks, prevPeaks;
};
