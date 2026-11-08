// Driftwerk DSP building blocks.
// Original code: PolyBLEP oscillators, ZDF ladder filter, envelopes, LFOs,
// string ensemble, tape echo and an FDN hall reverb.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace dw
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float midiToHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }

// Cheap, smooth soft clipper (rational tanh approximation).
inline float softClip (float x)
{
    x = clampf (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Exponential smoothing coefficient for a time constant in seconds.
inline float onePoleCoef (float seconds, float sampleRate)
{
    if (seconds <= 0.0f) return 1.0f;
    return 1.0f - std::exp (-1.0f / (seconds * sampleRate));
}

struct Random
{
    uint32_t state = 0x12345678u;
    explicit Random (uint32_t seed = 0x12345678u) : state (seed ? seed : 1u) {}
    float next() // uniform -1..1
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) (state & 0xFFFFFF) / (float) 0x7FFFFF - 1.0f;
    }
};

//==============================================================================
// Band-limited oscillator using PolyBLEP corrections.
struct BlepOsc
{
    enum Wave { Saw = 0, Square = 1, Triangle = 2 };

    double phase = 0.0;
    float triState = 0.0f;

    static float blep (double t, double dt)
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    float process (int wave, double freq, double sampleRate, float pulseWidth = 0.5f)
    {
        const double dt = std::min (0.49, freq / sampleRate);
        float out = 0.0f;

        if (wave == Saw)
        {
            out = (float) (2.0 * phase - 1.0) - blep (phase, dt);
        }
        else
        {
            const double pw = clampf (pulseWidth, 0.05f, 0.95f);
            float sq = phase < pw ? 1.0f : -1.0f;
            sq += blep (phase, dt);
            double t2 = phase - pw; if (t2 < 0.0) t2 += 1.0;
            sq -= blep (t2, dt);

            if (wave == Square)
                out = sq;
            else // leaky-integrated square -> triangle
            {
                triState = (float) (dt * 4.0) * sq + (1.0f - (float) dt * 0.05f) * triState;
                out = triState;
            }
        }

        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        return out;
    }
};

//==============================================================================
// 4-pole zero-delay-feedback ladder low-pass with a saturating input stage.
struct Ladder
{
    float s[4] { 0, 0, 0, 0 };
    float sampleRate = 44100.0f;

    void reset() { std::fill (std::begin (s), std::end (s), 0.0f); }

    float process (float x, float cutoff, float resonance, float drive)
    {
        cutoff = clampf (cutoff, 15.0f, sampleRate * 0.45f);
        const float g = std::tan (kPi * cutoff / sampleRate);
        const float G = g / (1.0f + g);
        const float k = resonance * 3.9f;

        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float inv = 1.0f / (1.0f + g);
        const float sigma = G3 * s[0] * inv + G2 * s[1] * inv + G * s[2] * inv + s[3] * inv;

        // Resonance gain compensation keeps the level from collapsing at high Q.
        const float in = x * (1.0f + resonance * 1.2f);
        float u = (in - k * sigma) / (1.0f + k * G4);
        u = softClip (u * (1.0f + drive * 3.0f)) / (1.0f + drive * 0.8f);

        float y = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (y - s[i]) * G;
            const float out = v + s[i];
            s[i] = out + v;
            y = out;
        }
        return y;
    }
};

//==============================================================================
// Analog-style ADSR with exponential segments.
struct Envelope
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float value = 0.0f;
    float aCoef = 0.01f, dCoef = 0.01f, rCoef = 0.01f, sustain = 1.0f;

    void set (float a, float d, float s, float r, float sr)
    {
        aCoef = onePoleCoef (a * 0.6f, sr);  // overshoot target makes this ~a seconds
        dCoef = onePoleCoef (d * 0.25f, sr);
        rCoef = onePoleCoef (r * 0.22f, sr);
        sustain = s;
    }

    void noteOn() { stage = Attack; }
    void noteOff() { if (stage != Idle) stage = Release; }
    bool isActive() const { return stage != Idle; }

    float process()
    {
        switch (stage)
        {
            case Attack:
                value += (1.25f - value) * aCoef;
                if (value >= 1.0f) { value = 1.0f; stage = Decay; }
                break;
            case Decay:
                value += (sustain - value) * dCoef;
                if (std::abs (value - sustain) < 1.0e-4f) { value = sustain; stage = Sustain; }
                break;
            case Sustain:
                value = sustain;
                break;
            case Release:
                value += (0.0f - value) * rCoef;
                if (value < 1.0e-5f) { value = 0.0f; stage = Idle; }
                break;
            case Idle: default:
                value = 0.0f;
                break;
        }
        return value;
    }
};

//==============================================================================
// Slow LFO: sine, triangle, sample & hold, smooth random drift.
struct LFO
{
    enum Shape { Sine = 0, Triangle = 1, SampleHold = 2, Drift = 3 };
    double phase = 0.0;
    float held = 0.0f, target = 0.0f, smooth = 0.0f;
    Random rng { 0xA5A5F00Du };

    float process (int shape, float rateHz, float sampleRate)
    {
        const double inc = rateHz / sampleRate;
        phase += inc;
        bool wrapped = false;
        if (phase >= 1.0) { phase -= 1.0; wrapped = true; }

        switch (shape)
        {
            case Sine: return std::sin (kTwoPi * (float) phase);
            case Triangle: return 1.0f - 4.0f * std::abs ((float) phase - 0.5f);
            case SampleHold:
                if (wrapped) held = rng.next();
                return held;
            case Drift: default:
                if (wrapped) target = rng.next();
                smooth += (target - smooth) * (float) std::min (1.0, inc * 3.0);
                return smooth;
        }
    }
};

//==============================================================================
// Fractional delay line with linear interpolation.
struct DelayLine
{
    std::vector<float> buf;
    int write = 0;

    void prepare (int maxSamples) { buf.assign ((size_t) maxSamples + 4, 0.0f); write = 0; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

    void push (float x)
    {
        buf[(size_t) write] = x;
        if (++write >= (int) buf.size()) write = 0;
    }

    float read (float delaySamples) const
    {
        const int size = (int) buf.size();
        delaySamples = clampf (delaySamples, 1.0f, (float) size - 3.0f);
        float rp = (float) write - delaySamples;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float frac = rp - (float) i0;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * frac;
    }
};

//==============================================================================
// String-machine style ensemble: three modulated delay taps per side.
struct Ensemble
{
    DelayLine dl[2];
    double ph[3] { 0.0, 0.333, 0.666 };
    double phFast[3] { 0.1, 0.45, 0.8 };
    float sr = 44100.0f;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (auto& d : dl) d.prepare ((int) (sampleRate * 0.05f));
    }

    void process (float& l, float& r, float mix)
    {
        dl[0].push (l);
        dl[1].push (r);
        float wl = 0.0f, wr = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            ph[i] += 0.55 / sr;      if (ph[i] >= 1.0) ph[i] -= 1.0;
            phFast[i] += 5.8 / sr;   if (phFast[i] >= 1.0) phFast[i] -= 1.0;
            const float m = std::sin (kTwoPi * (float) ph[i]) * 0.0040f
                          + std::sin (kTwoPi * (float) phFast[i]) * 0.00035f;
            const float base = 0.011f;
            wl += dl[0].read ((base + m) * sr);
            wr += dl[1].read ((base - m) * sr);
        }
        wl *= 0.45f; wr *= 0.45f;
        l = l * (1.0f - mix * 0.6f) + wl * mix;
        r = r * (1.0f - mix * 0.6f) + wr * mix;
    }
};

//==============================================================================
// Stereo ping-pong tape echo with wow, flutter, filtering and saturation.
struct TapeEcho
{
    DelayLine dl[2];
    float sr = 44100.0f;
    float smoothedTime = 0.3f;
    float lp[2] { 0, 0 }, hp[2] { 0, 0 };
    double wowPh = 0.0, flutPh = 0.0;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (auto& d : dl) d.prepare ((int) (sampleRate * 2.6f));
    }

    void process (float& l, float& r, float timeSec, float feedback, float mix, float wow)
    {
        smoothedTime += (timeSec - smoothedTime) * 0.00008f * (44100.0f / sr); // tape-speed glide
        wowPh += 0.6 / sr;  if (wowPh >= 1.0) wowPh -= 1.0;
        flutPh += 7.3 / sr; if (flutPh >= 1.0) flutPh -= 1.0;
        const float mod = 1.0f + wow * (0.0045f * std::sin (kTwoPi * (float) wowPh)
                                      + 0.0012f * std::sin (kTwoPi * (float) flutPh));
        const float d = smoothedTime * sr * mod;

        float yl = dl[0].read (d);
        float yr = dl[1].read (d);

        // Tape tone: gentle low-pass and high-pass in the loop.
        const float lpC = 0.32f * (44100.0f / sr);
        const float hpC = 0.012f * (44100.0f / sr);
        lp[0] += (yl - lp[0]) * lpC; lp[1] += (yr - lp[1]) * lpC;
        hp[0] += (lp[0] - hp[0]) * hpC; hp[1] += (lp[1] - hp[1]) * hpC;
        const float fl = lp[0] - hp[0];
        const float fr = lp[1] - hp[1];

        // Ping-pong: left input feeds left line, which crosses to right on repeat.
        const float inMono = 0.5f * (l + r);
        dl[0].push (softClip (inMono + fr * feedback));
        dl[1].push (softClip (fl * feedback));

        l += fl * mix;
        r += fr * mix;
    }
};

//==============================================================================
// 8-line feedback-delay-network hall with input diffusion and damping.
struct HallReverb
{
    static constexpr int N = 8;
    DelayLine lines[N];
    DelayLine diff[4];
    float lens[N] {};
    float diffLens[4] {};
    float damp[N] {};
    float gains[N] {};
    float sr = 44100.0f;
    double modPh = 0.0;
    DelayLine predelay[2];

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        const float base[N] = { 1433, 1601, 1867, 2053, 2251, 2399, 2687, 2903 };
        const float dbase[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < N; ++i)
        {
            lens[i] = base[i] * sr / 44100.0f;
            lines[i].prepare ((int) (lens[i] * 2.2f) + 64);
        }
        for (int i = 0; i < 4; ++i)
        {
            diffLens[i] = dbase[i] * sr / 44100.0f;
            diff[i].prepare ((int) diffLens[i] + 8);
        }
        for (auto& p : predelay) p.prepare ((int) (sr * 0.1f));
    }

    static float allpass (DelayLine& d, float len, float x, float g = 0.62f)
    {
        const float delayed = d.read (len);
        const float v = x + g * delayed;
        d.push (v);
        return delayed - g * v;
    }

    void process (float& l, float& r, float size, float damping, float mix)
    {
        const float scale = 0.55f + size * 1.0f;
        const float rt60 = 1.2f + size * size * 38.0f; // up to ~40 s tail
        for (int i = 0; i < N; ++i)
            gains[i] = std::pow (10.0f, -3.0f * (lens[i] * scale) / (sr * rt60));

        predelay[0].push (l); predelay[1].push (r);
        float in = 0.5f * (predelay[0].read (0.025f * sr) + predelay[1].read (0.031f * sr));
        for (int i = 0; i < 4; ++i)
            in = allpass (diff[i], diffLens[i], in);

        modPh += 0.21 / sr; if (modPh >= 1.0) modPh -= 1.0;
        const float m = std::sin (kTwoPi * (float) modPh) * 9.0f;

        float o[N];
        for (int i = 0; i < N; ++i)
        {
            const float mod = (i & 1) ? m : -m;
            o[i] = lines[i].read (lens[i] * scale + (i < 2 ? mod : 0.0f));
        }

        // Fast Hadamard transform (8x8), normalised.
        float h[N];
        for (int i = 0; i < N; ++i) h[i] = o[i];
        for (int len = 1; len < N; len <<= 1)
            for (int i = 0; i < N; i += len << 1)
                for (int j = i; j < i + len; ++j)
                {
                    const float a = h[j], b = h[j + len];
                    h[j] = a + b; h[j + len] = a - b;
                }
        const float norm = 0.35355339f; // 1/sqrt(8)

        const float dampC = 1.0f - damping * 0.85f;
        for (int i = 0; i < N; ++i)
        {
            float v = h[i] * norm * gains[i];
            damp[i] += (v - damp[i]) * dampC;
            lines[i].push (damp[i] + ((i & 1) ? in : -in) * 0.5f);
        }

        const float wl = (o[0] + o[2] + o[4] + o[6]) * 0.45f;
        const float wr = (o[1] + o[3] + o[5] + o[7]) * 0.45f;
        l = l * (1.0f - mix * 0.5f) + wl * mix;
        r = r * (1.0f - mix * 0.5f) + wr * mix;
    }
};

} // namespace dw
