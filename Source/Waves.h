// The arp's waveforms: SINE, TRIANGLE, SAW, SQUARE and WAVETABLE, each with SHAPE (bends the
// wave), FOLD (a wavefolder) and MOVE (SHAPE follows the note's envelope and a slow drift).
// The wavetables are made here from scratch (additive and drawn waves), band-limited per octave.
// All original code.
#pragma once

#include "DSP.h"
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace dw
{
struct Wavetables
{
    static constexpr int N = 2048, Frames = 8, Levels = 8, MaxH = 512, NumBanks = 6;
    // [bank][frame][level][sample]
    std::vector<float> data;

    static const char* bankName (int b)
    {
        static const char* n[NumBanks] { "SOFT > BRIGHT", "VOWELS", "GLASS", "ORGAN", "SYNC", "DIGITAL" };
        return n[b < 0 ? 0 : (b >= NumBanks ? NumBanks - 1 : b)];
    }

    static const Wavetables& get()
    {
        static const std::unique_ptr<Wavetables> t (new Wavetables());
        return *t;
    }

    const float* table (int bank, int frame, int level) const
    {
        return data.data() + ((((size_t) bank * Frames + (size_t) frame) * Levels + (size_t) level) * N);
    }

    // One sample at phase ph (0..1), position pos (0..1 across the frames), for a note at
    // freq (Hz): the octave's table keeps every harmonic under Nyquist.
    float read (int bank, float pos, double ph, double freq, double sr) const
    {
        bank = bank < 0 ? 0 : (bank >= NumBanks ? NumBanks - 1 : bank);
        const double top = (double) MaxH * freq / (0.45 * sr);
        int level = top <= 1.0 ? 0 : (int) std::ceil (std::log2 (top));
        level = level < 0 ? 0 : (level >= Levels ? Levels - 1 : level);
        const float fp = clampf (pos, 0.0f, 1.0f) * (float) (Frames - 1);
        const int f0 = std::min (Frames - 2, (int) fp);
        const float ff = fp - (float) f0;
        const double x = ph * (double) N;
        const int i0 = (int) x & (N - 1), i1 = (i0 + 1) & (N - 1);
        const float fr = (float) (x - std::floor (x));
        const float* a = table (bank, f0, level);
        const float* b = table (bank, f0 + 1, level);
        const float va = a[i0] + (a[i1] - a[i0]) * fr;
        const float vb = b[i0] + (b[i1] - b[i0]) * fr;
        return va + (vb - va) * ff;
    }

private:
    Wavetables()
    {
        data.assign ((size_t) NumBanks * Frames * Levels * N, 0.0f);
        std::vector<float> sinT ((size_t) N), cosT ((size_t) N);
        for (int i = 0; i < N; ++i) { sinT[(size_t) i] = std::sin (kTwoPi * (float) i / (float) N); cosT[(size_t) i] = std::cos (kTwoPi * (float) i / (float) N); }
        std::vector<float> sa ((size_t) MaxH + 1), ca ((size_t) MaxH + 1), wave ((size_t) N);
        for (int bank = 0; bank < NumBanks; ++bank)
            for (int fr = 0; fr < Frames; ++fr)
            {
                std::fill (sa.begin(), sa.end(), 0.0f);
                std::fill (ca.begin(), ca.end(), 0.0f);
                const float x = (float) fr / (float) (Frames - 1);
                harmonics (bank, x, sa, ca, wave, sinT, cosT);
                // Build each octave's table, then scale the whole frame so its fullest table peaks at 1.
                float peak = 1.0e-6f;
                for (int lv = 0; lv < Levels; ++lv)
                {
                    float* t = const_cast<float*> (table (bank, fr, lv));
                    const int hmax = MaxH >> lv;
                    for (int h = 1; h <= hmax; ++h)
                    {
                        if (sa[(size_t) h] == 0.0f && ca[(size_t) h] == 0.0f) continue;
                        for (int i = 0; i < N; ++i)
                        {
                            const int k = (h * i) & (N - 1);
                            t[i] += sa[(size_t) h] * sinT[(size_t) k] + ca[(size_t) h] * cosT[(size_t) k];
                        }
                    }
                    if (lv == 0) for (int i = 0; i < N; ++i) peak = std::max (peak, std::abs (t[i]));
                }
                for (int lv = 0; lv < Levels; ++lv)
                {
                    float* t = const_cast<float*> (table (bank, fr, lv));
                    for (int i = 0; i < N; ++i) t[i] /= peak;
                }
            }
    }

    // The harmonics (sine and cosine parts) of one frame of one bank; x goes 0..1 across the frames.
    static void harmonics (int bank, float x, std::vector<float>& sa, std::vector<float>& ca, std::vector<float>& wave,
                           const std::vector<float>& sinT, const std::vector<float>& cosT)
    {
        switch (bank)
        {
            case 0:   // SOFT > BRIGHT: a sine that slowly grows the harmonics of a saw
            {
                const float reach = 1.0f + x * x * 70.0f;
                for (int h = 1; h <= MaxH; ++h) sa[(size_t) h] = (1.0f / (float) h) * std::exp (-(float) (h - 1) / reach);
                break;
            }
            case 1:   // VOWELS: a buzz shaped by the formants of oo > oh > ah > eh (as if sung at 150 Hz)
            {
                float fm[3]; vowelFormants (x, fm);
                const float bw[3] { 90.0f, 120.0f, 170.0f }, g[3] { 1.0f, 0.6f, 0.25f };
                for (int h = 1; h <= MaxH; ++h)
                {
                    const float hz = 150.0f * (float) h;
                    float a = 0.0f;
                    for (int k = 0; k < 3; ++k) { const float d = (hz - fm[k]) / bw[k]; a += g[k] * std::exp (-0.5f * d * d); }
                    sa[(size_t) h] = (a + 0.02f) / std::pow ((float) h, 0.6f);
                }
                break;
            }
            case 2:   // GLASS: a few bright partials that climb as the frame moves
            {
                static const int parts[] { 1, 2, 3, 5, 7, 9, 11, 13, 17, 19, 23, 29 };
                const float centre = 0.4f + 2.6f * x;
                for (int p : parts)
                {
                    const float d = std::log ((float) p) - centre;
                    sa[(size_t) p] = (p == 1 ? 1.0f : 0.0f) + 0.8f * std::exp (-d * d / 0.35f);
                }
                break;
            }
            case 3:   // ORGAN: drawbar registrations, from a flute to a full reedy chorus
            {
                static const int bars[] { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
                static const float reg[4][9] { { 1, .5f, 0, 0, 0, 0, 0, 0, 0 }, { 1, .8f, .6f, .5f, 0, .3f, 0, 0, 0 },
                                               { .8f, 1, .7f, .8f, .5f, .6f, .3f, .3f, .2f }, { .6f, .9f, 1, 1, .8f, .9f, .7f, .7f, .6f } };
                const float fp = x * 2.999f; const int r0 = (int) fp; const float rf = fp - (float) r0;
                for (int i = 0; i < 9; ++i) sa[(size_t) bars[i]] = reg[r0][i] + (reg[r0 + 1][i] - reg[r0][i]) * rf;
                break;
            }
            default:  // SYNC and DIGITAL are drawn in time, then measured
            {
                for (int i = 0; i < N; ++i)
                {
                    const float ph = (float) i / (float) N;
                    if (bank == 4)   // SYNC: a saw restarted every cycle, running 1 to 4.5 times faster
                    {
                        const float r = 1.0f + 3.5f * x;
                        const float s = ph * r;
                        wave[(size_t) i] = (2.0f * (s - std::floor (s)) - 1.0f) * (1.0f - 0.3f * ph);
                    }
                    else             // DIGITAL: a sine with fewer and fewer steps
                    {
                        const float steps = std::round (std::exp2 (5.0f - 3.6f * x));
                        wave[(size_t) i] = std::round (std::sin (kTwoPi * ph) * steps) / steps;
                    }
                }
                for (int h = 1; h <= MaxH; ++h)
                {
                    double s = 0.0, c = 0.0;
                    for (int i = 0; i < N; ++i)
                    {
                        const int k = (h * i) & (N - 1);
                        s += (double) wave[(size_t) i] * sinT[(size_t) k];
                        c += (double) wave[(size_t) i] * cosT[(size_t) k];
                    }
                    sa[(size_t) h] = (float) (2.0 * s / N);
                    ca[(size_t) h] = (float) (2.0 * c / N);
                }
                break;
            }
        }
    }
};

// One arp oscillator. wave: 1 SINE, 2 TRIANGLE, 3 SAW, 4 SQUARE, 5 WAVETABLE (0 = the patched VCO).
struct WaveOsc
{
    enum Wave { Patch = 0, Sine, Triangle, Saw, Square, Table, NumWaves };
    double ph = 0.0, ph2 = 0.37, ph3 = 0.71;

    static float polyBlep (double t, double dt)
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }
    static float sawAt (double p, double dt) { return (float) (2.0 * p - 1.0) - polyBlep (p, dt); }
    static void advance (double& p, double dt) { p += dt; if (p >= 1.0) p -= 1.0; }

    // The shape of one cycle without band-limiting (the editor draws this).
    static float shapeAt (int wave, float p, float shape, float fold, int bank)
    {
        float y = 0.0f;
        switch (wave)
        {
            case Sine:     y = std::sin (kTwoPi * bendPhase (p, shape)); break;
            case Triangle: { const float d = 0.5f + 0.49f * shape; y = p < d ? -1.0f + 2.0f * p / d : 1.0f - 2.0f * (p - d) / (1.0f - d); break; }
            case Saw:      y = 2.0f * p - 1.0f; if (shape > 0.0f) { float q = p + 0.37f; q -= std::floor (q); y = (y + shape * (2.0f * q - 1.0f)) / (1.0f + shape); } break;
            case Square:   y = p < 0.5f - 0.45f * shape ? 1.0f : -1.0f; break;
            case Table:    y = Wavetables::get().read (bank, shape, p, 20.0, 44100.0); break;
            default:       y = 2.0f * p - 1.0f; break;
        }
        return applyFold (y, fold);
    }

    // SINE's SHAPE squeezes the first half of the cycle (phase distortion): a pure tone turns reedy.
    static float bendPhase (float p, float shape)
    {
        const float d = 0.5f - 0.46f * clampf (shape, 0.0f, 1.0f);
        return p < d ? p * 0.5f / d : 0.5f + (p - d) * 0.5f / (1.0f - d);
    }
    // FOLD: the wave is pushed past its edges and folds back, adding glassy overtones.
    static float applyFold (float y, float fold)
    {
        if (fold <= 0.001f) return y;
        return std::sin (kPi * 0.5f * y * (1.0f + 5.0f * fold));
    }

    float process (int wave, double freq, double sr, float shape, float fold, int bank)
    {
        const double dt = std::min (0.49, freq / sr);
        shape = clampf (shape, 0.0f, 1.0f);
        float y = 0.0f;
        switch (wave)
        {
            case Sine:     y = std::sin (kTwoPi * bendPhase ((float) ph, shape)); break;
            case Triangle:
            {
                // a triangle leaning into a ramp; the two corners are smoothed (poly-BLAMP)
                const double d = 0.5 + 0.49 * shape;
                y = ph < d ? (float) (-1.0 + 2.0 * ph / d) : (float) (1.0 - 2.0 * (ph - d) / (1.0 - d));
                const double slopeJump = 2.0 / d + 2.0 / (1.0 - d);
                y -= (float) (slopeJump * dt) * blamp (ph < d ? ph + 1.0 - d : ph - d, dt);
                y += (float) (slopeJump * dt) * blamp (ph, dt);
                break;
            }
            case Saw:
            {
                // SHAPE spreads three saws apart: one becomes a shimmering, slowly beating chord of saws
                const double spread = 1.0 + 0.012 * shape;
                y = sawAt (ph, dt);
                if (shape > 0.001f)
                {
                    const double d2 = std::min (0.49, dt * spread), d3 = std::min (0.49, dt / spread);
                    y = (y + shape * (sawAt (ph2, d2) + sawAt (ph3, d3))) / (1.0f + 1.2f * shape);
                    advance (ph2, d2); advance (ph3, d3);
                }
                break;
            }
            case Square:
            {
                const double pw = 0.5 - 0.45 * shape;
                y = ph < pw ? 1.0f : -1.0f;
                y += polyBlep (ph, dt);
                double t2 = ph - pw; if (t2 < 0.0) t2 += 1.0;
                y -= polyBlep (t2, dt);
                y *= 0.8f;
                break;
            }
            case Table:    y = Wavetables::get().read (bank, shape, ph, freq, sr); break;
            default:       y = sawAt (ph, dt); break;
        }
        advance (ph, dt);
        return applyFold (y, fold);
    }

    // Integrated BLEP, for the corners of the triangle.
    static float blamp (double t, double dt)
    {
        if (t < dt) { const double x = t / dt - 1.0; return (float) (-x * x * x / 3.0); }
        if (t > 1.0 - dt) { const double x = (t - 1.0) / dt + 1.0; return (float) (x * x * x / 3.0); }
        return 0.0f;
    }
};
}
