#pragma once
#include <cmath>

/*  Scale maths for the harmonizer.  Note numbers are MIDI-style (60 = C4), `root` is 0..11 (C..B).
    type: 0 Major, 1 Minor, 2 Major pentatonic, 3 Minor pentatonic, 4 Chromatic (= no quantization). */
namespace harmony
{
    constexpr int tables[4][7] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 7, 9, 0, 0 }, { 0, 3, 5, 7, 10, 0, 0 } };
    constexpr int lens[4] = { 7, 7, 5, 5 };
    constexpr int chromaticType = 4;

    inline int floorDiv (int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) --q; return q; }

    /** MIDI note of scale degree `idx` (degree 0 = root, idx may be negative / > scale length) */
    inline int noteOf (int idx, int type, int root)
    {
        const int n = lens[type], o = floorDiv (idx, n), dg = idx - o * n;
        return root + 12 * o + tables[type][dg];
    }
    /** degree index of the scale note closest to x */
    inline int nearest (float x, int type, int root)
    {
        const int n = lens[type], o = (int) std::floor ((x - (float) root) / 12.f);
        int best = o * n;  float bd = 1.0e9f;
        for (int i = o * n - n; i <= o * n + 2 * n; ++i)
        {
            const float dd = std::fabs ((float) noteOf (i, type, root) - x);
            if (dd < bd) { bd = dd; best = i; }
        }
        return best;
    }
    /** semitone shift of extra voice v (0 3rd up, 1 3rd down, 2 5th up, 3 5th down, 4 oct up, 5 oct down).
        Octaves are ALWAYS exactly +-12.  Thirds/fifths follow the scale when a scale is active and a note is known;
        otherwise (chromatic / no note yet) fixed intervals: 3rd = +-4, 5th = +-7. */
    inline float voiceSemitones (int v, int type, int root, int curDeg, bool haveNote)
    {
        static const float fixedSt[6] = { 4.f, -4.f, 7.f, -7.f, 12.f, -12.f };
        if (v >= 4 || type == chromaticType || ! haveNote) return fixedSt[v];
        const int off = (v < 2 ? 2 : (lens[type] == 7 ? 4 : 3)) * ((v & 1) ? -1 : 1);
        return (float) (noteOf (curDeg + off, type, root) - noteOf (curDeg, type, root));
    }
}
