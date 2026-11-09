// Timewind's experimental layers: the notes the slime plays (SlimeVoices), the PULSE
// percussion (soft synthesised drums) and MEMORY (the instrument replaying its own past).
// All original code.
#pragma once

#include "DSP.h"
#include <cstdint>
#include <vector>

namespace dw
{
inline float uni (Random& r) { return std::min (0.9999f, r.next() * 0.5f + 0.5f); }

//==============================================================================
// SLIME VOICES: short calm notes, played when a vein of the organism reaches one of its ears.
// DROPS are water drops (a sine that falls into its note), BREATH is blown noise around the
// note, TINES are little metal tongues (kalimba-like, slightly inharmonic).
struct SlimeVoices
{
    static constexpr int V = 12;
    enum Type { Drop = 0, Breath = 1, Tine = 2 };
    struct Voice
    {
        bool on = false, reverse = false;
        int type = 0;
        float note = 60.0f, vel = 0.0f, pan = 0.0f, t = 0.0f, delay = 0.0f;
        float bendFrom = 0.0f, bendTime = 0.03f, glide = 0.0f, glideTime = 0.3f;
        float atk = 0.002f, len = 1.0f, env = 0.0f, nGain = 1.0f;
        double ph1 = 0.0, ph2 = 0.0, ph3 = 0.0;
        Svf bp1, bp2;
    };
    Voice v[V];
    Random rng { 0x5117E5u };
    float sr = 44100.0f;
    Svf toneL, toneR;
    int coefCounter = 0;

    void prepare (float sampleRate) { sr = sampleRate; for (auto& vo : v) vo.on = false; }

    bool anySounding() const { for (auto& vo : v) if (vo.on) return true; return false; }

    // note: MIDI. delaySec: when it starts. glideFrom: semitones it slides in from (0 = none).
    void trigger (int type, float note, float vel, float pan, float delaySec, float detuneCents, float glideFrom, bool reverse)
    {
        int best = 0; float quiet = 1.0e9f;
        for (int i = 0; i < V; ++i)
        {
            if (! v[i].on) { best = i; quiet = -1.0f; break; }
            const float lvl = v[i].env * v[i].vel;
            if (lvl < quiet) { quiet = lvl; best = i; }
        }
        auto& vo = v[best];
        vo = Voice {};
        vo.on = true;
        vo.type = type;
        vo.reverse = reverse;
        vo.note = note + detuneCents * 0.01f;
        vo.vel = vel;
        vo.pan = clampf (pan, -1.0f, 1.0f);
        vo.delay = delaySec;
        vo.glide = glideFrom;
        vo.glideTime = 0.25f + 0.5f * uni (rng);
        switch (type)
        {
            case Drop:   vo.bendFrom = 6.0f + 7.0f * uni (rng); vo.bendTime = 0.018f + 0.02f * uni (rng);
                         vo.atk = 0.0015f; vo.len = 0.35f + 0.6f * uni (rng); break;
            case Breath: vo.atk = 0.2f + 0.45f * uni (rng); vo.len = 1.4f + 1.4f * uni (rng); break;
            default:     vo.atk = 0.001f; vo.len = 1.2f + 1.6f * uni (rng); break;
        }
        if (reverse) vo.len = std::max (vo.len, 0.9f) * 1.3f;
        if (type == Breath)
        {
            const float f = midiToHz (vo.note);
            vo.bp1.set (f, 38.0f, sr);
            vo.bp2.set (f * 2.0f, 30.0f, sr);
            vo.nGain = 0.55f * std::sqrt (38.0f * sr * 0.5f / std::max (60.0f, f));
        }
    }

    void process (float tone, float& l, float& r)
    {
        if (coefCounter-- <= 0)
        {
            coefCounter = 64;
            const float cut = 700.0f * std::exp2 (tone * 4.6f);
            toneL.set (cut, 0.6f, sr); toneR.set (cut * 1.02f, 0.6f, sr);
        }
        const float dt = 1.0f / sr;
        float ol = 0.0f, orr = 0.0f;
        for (auto& vo : v)
        {
            if (! vo.on) continue;
            if (vo.delay > 0.0f) { vo.delay -= dt; continue; }
            vo.t += dt;

            // Envelope: a quick or slow rise, then a long fall; reversed notes swell up and stop.
            float e;
            if (vo.reverse)
            {
                const float x = vo.t / vo.len;
                if (x < 1.0f) e = x * x * x;
                else { e = std::exp (-(vo.t - vo.len) / 0.05f); if (vo.t - vo.len > 0.4f) { vo.on = false; continue; } }
            }
            else if (vo.t < vo.atk) { const float x = vo.t / vo.atk; e = x * x * (3.0f - 2.0f * x); }
            else
            {
                e = std::exp (-(vo.t - vo.atk) / (vo.len * 0.42f));
                if (e < 2.0e-4f) { vo.on = false; continue; }
            }
            vo.env = e;

            float pitch = vo.note;
            if (vo.bendFrom != 0.0f && ! vo.reverse) pitch += vo.bendFrom * std::exp (-vo.t / vo.bendTime);
            if (vo.glide != 0.0f) pitch += vo.glide * std::exp (-vo.t / vo.glideTime);
            const float f = midiToHz (pitch);
            float y = 0.0f;
            switch (vo.type)
            {
                case Drop:
                {
                    vo.ph1 += f / sr; if (vo.ph1 >= 1.0) vo.ph1 -= 1.0;
                    vo.ph2 += f * 2.01f / sr; if (vo.ph2 >= 1.0) vo.ph2 -= 1.0;
                    y = std::sin (kTwoPi * (float) vo.ph1) + 0.18f * e * std::sin (kTwoPi * (float) vo.ph2);
                    y *= e;
                    break;
                }
                case Breath:
                {
                    const float n = rng.next();
                    float lp;
                    const float b1 = vo.bp1.process (n, lp) / 38.0f;
                    const float b2 = vo.bp2.process (n, lp) / 30.0f;
                    vo.ph1 += f / sr; if (vo.ph1 >= 1.0) vo.ph1 -= 1.0;
                    y = (b1 + 0.35f * b2) * vo.nGain * 0.5f + 0.3f * std::sin (kTwoPi * (float) vo.ph1);
                    y *= e;
                    break;
                }
                default:
                {
                    vo.ph1 += f / sr; if (vo.ph1 >= 1.0) vo.ph1 -= 1.0;
                    vo.ph2 += f * 5.93f / sr; if (vo.ph2 >= 1.0) vo.ph2 -= 1.0;
                    vo.ph3 += f * 11.7f / sr; if (vo.ph3 >= 1.0) vo.ph3 -= 1.0;
                    const float e2 = vo.reverse ? e * e : std::exp (-vo.t / (vo.len * 0.06f));
                    const float e3 = vo.reverse ? e * e * e : std::exp (-vo.t / (vo.len * 0.025f));
                    y = 0.6f * (std::sin (kTwoPi * (float) vo.ph1) * e + 0.22f * std::sin (kTwoPi * (float) vo.ph2) * e2
                        + 0.07f * std::sin (kTwoPi * (float) vo.ph3) * e3);
                    break;
                }
            }
            y *= vo.vel;
            ol += y * std::sqrt (0.5f * (1.0f - vo.pan));
            orr += y * std::sqrt (0.5f * (1.0f + vo.pan));
        }
        l += toneL.lowpass (ol) * 0.2f;
        r += toneR.lowpass (orr) * 0.2f;
    }
};

//==============================================================================
// PULSE: soft percussion, synthesised. Five lanes: THUMP (a low heartbeat), SKIN (a hand
// drum), WOOD (a tuned block), BRUSH (brushed noise) and TICK (a tiny clock bell).
struct Pulse
{
    static constexpr int Lanes = 5, Steps = 16;
    enum Lane { Thump = 0, Skin = 1, Wood = 2, Brush = 3, Tick = 4 };
    struct Hit { int lane = 0; float vel = 0.0f, freq = 100.0f, pan = 0.0f; int wait = 0, variant = 0; };
    struct Drum
    {
        bool on = false;
        int variant = 0;
        float t = 0.0f, vel = 0.0f, f = 100.0f, pan = 0.0f;
        double ph = 0.0, ph2 = 0.0, ph3 = 0.0;
        Svf bp, bp2;
    };
    static constexpr int QN = 48;
    Hit queue[QN];
    int qn = 0;
    Drum d[Lanes];
    Random rng { 0xB0A7u };
    float sr = 44100.0f;
    Svf toneL, toneR;
    int coefCounter = 0;
    int hits = 0;                     // hits played so far (the editor flashes the lanes)
    int laneHits[Lanes] {};

    void prepare (float sampleRate) { sr = sampleRate; qn = 0; for (auto& x : d) x.on = false; }
    bool busy() const { if (qn > 0) return true; for (auto& x : d) if (x.on) return true; return false; }

    void schedule (int lane, float vel, float freq, float pan, int waitSamples, int variant)
    {
        if (qn >= QN) return;
        queue[qn++] = { lane, vel, freq, pan, std::max (0, waitSamples), variant };
    }

    void start (const Hit& h)
    {
        auto& x = d[h.lane];
        x.on = true; x.t = 0.0f; x.vel = h.vel; x.f = h.freq; x.pan = h.pan; x.variant = h.variant;
        x.ph = x.ph2 = x.ph3 = 0.0;
        if (h.lane == Brush)
        {
            x.bp.set (clampf (h.freq, 2500.0f, 12000.0f), 0.9f, sr);
            x.bp2.set (clampf (h.freq * 0.55f, 1500.0f, 8000.0f), 1.4f, sr);
        }
        if (h.lane == Skin) x.bp.set (h.variant == 1 ? 2100.0f : 900.0f, 1.2f, sr);
        ++hits; ++laneHits[h.lane];
    }

    void process (float tone, float& l, float& r)
    {
        for (int i = 0; i < qn;)
        {
            if (--queue[i].wait <= 0) { start (queue[i]); queue[i] = queue[--qn]; }
            else ++i;
        }
        if (coefCounter-- <= 0)
        {
            coefCounter = 64;
            const float cut = 1400.0f * std::exp2 (tone * 3.7f);
            toneL.set (cut, 0.6f, sr); toneR.set (cut * 1.02f, 0.6f, sr);
        }
        const float dt = 1.0f / sr;
        float ol = 0.0f, orr = 0.0f;
        for (int lane = 0; lane < Lanes; ++lane)
        {
            auto& x = d[lane];
            if (! x.on) continue;
            x.t += dt;
            const float t = x.t;
            float y = 0.0f;
            switch (lane)
            {
                case Thump:
                {
                    // A soft low heartbeat: a sine that sinks into its note.
                    const float f = x.f * (1.0f + 1.4f * std::exp (-t / 0.022f));
                    x.ph += f / sr; if (x.ph >= 1.0) x.ph -= 1.0;
                    const float atk = std::min (1.0f, t / 0.004f);
                    const float tau = x.variant == 1 ? 0.14f : 0.24f;
                    y = std::sin (kTwoPi * (float) x.ph) * atk * std::exp (-t / tau) * 0.95f;
                    if (t > tau * 9.0f) x.on = false;
                    break;
                }
                case Skin:
                {
                    // A hand drum: a low "doum" or a higher "tek", with a little slap of noise.
                    const bool tek = x.variant == 1;
                    const float f = x.f * (tek ? 2.1f : 1.0f) * (1.0f + 0.22f * std::exp (-t / 0.035f));
                    x.ph += f / sr; if (x.ph >= 1.0) x.ph -= 1.0;
                    x.ph2 += f * 1.59f / sr; if (x.ph2 >= 1.0) x.ph2 -= 1.0;
                    const float tau = tek ? 0.06f : 0.17f;
                    const float atk = std::min (1.0f, t / 0.0015f);
                    float lp;
                    const float slap = x.bp.process (rng.next(), lp) * std::exp (-t / 0.012f) * (tek ? 0.5f : 0.3f);
                    y = (std::sin (kTwoPi * (float) x.ph) * std::exp (-t / tau)
                         + 0.3f * std::sin (kTwoPi * (float) x.ph2) * std::exp (-t / (tau * 0.45f))) * atk * 0.7f + slap;
                    if (t > tau * 9.0f) x.on = false;
                    break;
                }
                case Wood:
                {
                    // A tuned wood block: three decaying modes.
                    x.ph += x.f / sr; if (x.ph >= 1.0) x.ph -= 1.0;
                    x.ph2 += x.f * 2.76f / sr; if (x.ph2 >= 1.0) x.ph2 -= 1.0;
                    x.ph3 += x.f * 5.4f / sr; if (x.ph3 >= 1.0) x.ph3 -= 1.0;
                    const float atk = std::min (1.0f, t / 0.0006f);
                    const float tau = x.variant == 1 ? 0.16f : 0.075f;
                    y = (std::sin (kTwoPi * (float) x.ph) * std::exp (-t / tau)
                         + 0.45f * std::sin (kTwoPi * (float) x.ph2) * std::exp (-t / (tau * 0.33f))
                         + 0.15f * std::sin (kTwoPi * (float) x.ph3) * std::exp (-t / (tau * 0.16f))) * atk * 0.6f;
                    if (t > tau * 9.0f) x.on = false;
                    break;
                }
                case Brush:
                {
                    // Brushed noise; variant 1 is a slow swish, variant 2 a reversed swell.
                    float env;
                    if (x.variant == 2) { const float rise = 0.16f; env = t < rise ? (t / rise) * (t / rise) * (t / rise) : std::exp (-(t - rise) / 0.012f); if (t > rise + 0.1f) x.on = false; }
                    else
                    {
                        const float atk = x.variant == 1 ? 0.035f : 0.0025f, tau = x.variant == 1 ? 0.11f : 0.045f;
                        env = t < atk ? t / atk : std::exp (-(t - atk) / tau);
                        if (t > atk + tau * 9.0f) x.on = false;
                    }
                    float lp;
                    const float n = rng.next();
                    y = (x.bp.process (n, lp) * 0.9f + x.bp2.process (n, lp) * 0.35f) * env * 0.55f;
                    break;
                }
                default:
                {
                    // A tiny bell tick.
                    x.ph += x.f / sr; if (x.ph >= 1.0) x.ph -= 1.0;
                    x.ph2 += x.f * 3.01f / sr; if (x.ph2 >= 1.0) x.ph2 -= 1.0;
                    const float atk = std::min (1.0f, t / 0.0008f);
                    const float tau = x.variant == 1 ? 0.09f : 0.028f;
                    y = std::sin (kTwoPi * (float) x.ph + 1.3f * std::exp (-t / 0.008f) * std::sin (kTwoPi * (float) x.ph2))
                        * atk * std::exp (-t / tau) * 0.4f;
                    if (t > tau * 9.0f) x.on = false;
                    break;
                }
            }
            y *= x.vel;
            ol += y * std::sqrt (0.5f * (1.0f - x.pan));
            orr += y * std::sqrt (0.5f * (1.0f + x.pan));
        }
        l += toneL.lowpass (ol) * 0.5f;
        r += toneR.lowpass (orr) * 0.5f;
    }
};

//==============================================================================
// MEMORY: everything Timewind plays is remembered (about three minutes, lo-fi), and two
// tape heads keep drifting back into it, playing old fragments slower, reversed, blurred.
// What they play can be remembered again, so old moments slowly dissolve.
struct Memory
{
    static constexpr int MSR = 16000;                    // memory sample rate
    static constexpr int Seconds = 200;
    static constexpr int LEN = MSR * Seconds;
    static constexpr int PeakRate = 2;                   // peaks per second, for the editor
    static constexpr int NumPeaks = Seconds * PeakRate;
    std::vector<int16_t> buf;
    long long written = 0;                               // samples written since the last FORGET
    double wacc = 0.0;
    float sr = 44100.0f;
    Svf aa1, aa2, postL, postR, postL2, postR2;
    float peakAcc = 0.0f; int peakCount = 0;
    float peaks[NumPeaks] {};
    int peakHead = 0;
    Random rng { 0x3E3021u };
    int coefCounter = 0;
    float wowPh = 0.0f, flutPh = 0.0f;

    struct Head
    {
        double pos = 0.0;      // absolute memory sample position
        double speed = 1.0;
        float t = 0.0f, dur = 4.0f, pan = 0.0f;
        bool live = false;     // false while the memory isn't old enough yet
        float window = 0.0f;
    };
    Head h[2];

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        if (buf.empty()) buf.assign ((size_t) LEN, 0);
        aa1.set (6500.0f, 0.54f, sr); aa2.set (6500.0f, 1.31f, sr);
        h[0].t = 0.0f; h[1].t = 0.0f; h[0].dur = h[1].dur = 0.0f;
    }

    void forget() { written = 0; std::fill (std::begin (peaks), std::end (peaks), 0.0f); }

    void write (float x)
    {
        const float y = aa2.lowpass (aa1.lowpass (x));
        wacc += (double) MSR / (double) sr;
        while (wacc >= 1.0)
        {
            wacc -= 1.0;
            const float c = softClip (y * 1.1f);
            buf[(size_t) (written % LEN)] = (int16_t) std::lrint (clampf (c, -1.0f, 1.0f) * 32000.0f);
            ++written;
            peakAcc = std::max (peakAcc, std::abs (c));
            if (++peakCount >= MSR / PeakRate)
            {
                peaks[peakHead] = peakAcc;
                peakHead = (peakHead + 1) % NumPeaks;
                peakAcc = 0.0f; peakCount = 0;
            }
        }
    }

    float read (double pos) const
    {
        const double fl = std::floor (pos);
        const long long i = (long long) fl;
        const float fr = (float) (pos - fl);
        auto at = [this] (long long k) { k %= LEN; if (k < 0) k += LEN; return (float) buf[(size_t) k] * (1.0f / 32000.0f); };
        // 4-point Hermite
        const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * fr + c2) * fr + c1) * fr + x0;
    }

    // speedMode: 0 1x, 1 1/2, 2 1/4, 3 REVERSE, 4 REV 1/2, 5 WANDER
    void restart (Head& hd, float agoSec, float fragSec, int speedMode, float scatter)
    {
        static const double speeds[] { 1.0, 0.5, 0.25, -1.0, -0.5 };
        int m = speedMode;
        if (m >= 5)
        {
            const float u = uni (rng);
            m = u < 0.3f ? 1 : (u < 0.55f ? 4 : (u < 0.75f ? 0 : (u < 0.9f ? 3 : 2)));
        }
        hd.speed = speeds[m];
        if (speedMode >= 5 && uni (rng) < 0.25f) hd.speed *= std::exp2 ((uni (rng) < 0.5f ? -5.0 : 7.0) / 12.0) ; // a fourth down or a fifth up
        hd.dur = fragSec * (0.85f + 0.3f * uni (rng));
        float ago = agoSec * (1.0f + scatter * (uni (rng) * 2.0f - 1.0f) * 0.85f);
        ago = clampf (ago, 1.5f, (float) Seconds - 2.0f - fragSec * 1.1f);
        const double span = (double) hd.dur * std::abs (hd.speed) * MSR;
        const double now = (double) written;
        double start = now - (double) ago * MSR;
        if (hd.speed < 0.0) start = std::min (now - 0.5 * MSR, start + span);   // reversed: play back toward the past
        hd.live = start - span > 0.0 && start > 0.0;
        hd.pos = start;
        hd.pan = (uni (rng) * 2.0f - 1.0f) * 0.75f;
        hd.t = 0.0f;
    }

    // Returns the raw memory signal (before LEVEL) so it can be remembered again.
    void process (bool on, float agoSec, float fragSec, int speedMode, float blur, float scatter, float& l, float& r)
    {
        if (coefCounter-- <= 0)
        {
            coefCounter = 64;
            const float cut = 6800.0f * std::exp2 (-blur * 3.6f);
            postL.set (cut, 0.6f, sr); postR.set (cut * 1.04f, 0.6f, sr);
            postL2.set (cut * 1.2f, 0.55f, sr); postR2.set (cut * 1.25f, 0.55f, sr);
        }
        if (! on) { l = r = 0.0f; h[0].dur = h[1].dur = 0.0f; return; }
        const float dt = 1.0f / sr;
        wowPh += 0.37f * dt; if (wowPh >= 1.0f) wowPh -= 1.0f;
        flutPh += 5.3f * dt; if (flutPh >= 1.0f) flutPh -= 1.0f;
        const double wow = 1.0 + (double) blur * (0.006 * std::sin (kTwoPi * wowPh) + 0.0015 * std::sin (kTwoPi * flutPh));
        float ol = 0.0f, orr = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            auto& hd = h[k];
            if (hd.dur <= 0.0f)
            {
                // first time: the second head starts half a fragment later, so they overlap evenly
                restart (hd, agoSec, fragSec, speedMode, scatter);
                if (k == 1) hd.t = hd.dur * 0.5f;
            }
            hd.t += dt;
            if (hd.t >= hd.dur) restart (hd, agoSec, fragSec, speedMode, scatter);
            const float x = hd.t / hd.dur;
            const float w = std::sin (kPi * x);
            hd.window = w * w;
            hd.pos += hd.speed * wow * (double) MSR / (double) sr;
            if (! hd.live) continue;
            const float s = read (hd.pos) * hd.window;
            const float smear = blur > 0.05f ? read (hd.pos - 0.021 * MSR * blur) * hd.window : 0.0f;
            const float y = s + smear * 0.45f * blur;
            ol += y * std::sqrt (0.5f * (1.0f - hd.pan));
            orr += y * std::sqrt (0.5f * (1.0f + hd.pan));
        }
        l = postL2.lowpass (postL.lowpass (ol));
        r = postR2.lowpass (postR.lowpass (orr));
    }

    float headAgoSeconds (int k) const { return (float) (((double) written - h[k].pos) / MSR); }
};

//==============================================================================
// TEXTURE: weather and creatures, all synthesised. Each slot plays one kind; IN KEY tunes the
// drops, bubbles, whistles and winds to the chord playing now.
struct Texture
{
    enum Kind { Rain = 0, Wind, Stream, Crackle, Creatures, Fire, Shore, NumKinds };

    struct Blip
    {
        bool on = false;
        int kind = 0;           // 0 sine (pitch moves from f to f*ratio), 1 noise burst, 2 click, 3 chirp (pulsed sine)
        float t = 0.0f, dur = 0.01f, f = 1000.0f, ratio = 1.0f, amp = 0.0f, pan = 0.0f, pulseRate = 30.0f, vib = 0.0f;
        double ph = 0.0;
        Svf bp;
    };

    struct Slot
    {
        static constexpr int NB = 28;
        Blip b[NB];
        Random rng { 0x7E47u };
        Svf nl1, nr1, nl2, nr2, lowL, lowR;
        float walk[4] {}, walkT[4] {};
        float gust = 0.0f, gustT = 0.5f, wavePh = 0.0f, wavePh2 = 0.37f, waveLen = 9.0f, waveLen2 = 11.0f;
        float lastKind = -1.0f, energy = 0.0f;
        int coef = 0, walkCount = 0;
        float cricketPh[3] {}, cricketRate[3] { 0.9f, 1.13f, 0.71f };
        int events = 0;

        Blip* spawn (int kind, float f, float ratio, float dur, float amp, float pan, float sr, float q = 2.0f)
        {
            int best = -1;
            for (int i = 0; i < NB; ++i) if (! b[i].on) { best = i; break; }
            if (best < 0) return nullptr;
            auto& x = b[best];
            x.on = true; x.kind = kind; x.t = 0.0f; x.f = f; x.ratio = ratio; x.dur = dur; x.amp = amp; x.pan = pan; x.ph = 0.0;
            x.vib = 0.0f;
            if (kind == 1 || kind == 2) x.bp.set (f, q, sr);
            ++events;
            return &x;
        }
    };
    Slot s[2];
    float sr = 44100.0f;
    float pitches[12] {};       // chord tones as pitch classes 0..11 (relative to C)
    int pitchCount = 0;

    void prepare (float sampleRate) { sr = sampleRate; s[1].rng = Random (0x1A71Eu); }

    // nearest chord tone (in Hz) to f, when key > a random draw
    float tune (float f, float key, Random& rng) const
    {
        if (pitchCount <= 0 || uni (rng) >= key) return f;
        const float m = 69.0f + 12.0f * std::log2 (std::max (20.0f, f) / 440.0f);
        float best = m, bestD = 100.0f;
        const float oct = std::floor (m / 12.0f) * 12.0f;
        for (int o = -1; o <= 1; ++o)
            for (int k = 0; k < pitchCount; ++k)
            {
                const float c = oct + (float) o * 12.0f + pitches[k];
                if (std::abs (c - m) < bestD) { bestD = std::abs (c - m); best = c; }
            }
        return midiToHz (best);
    }

    static float chance (float perSecond, float sr) { return perSecond / sr; }

    void processSlot (Slot& sl, int kind, float density, float tone, float motion, float key, float& l, float& r)
    {
        auto& rng = sl.rng;
        const float dt = 1.0f / sr;
        float ol = 0.0f, orr = 0.0f;
        if (sl.coef-- <= 0)
        {
            sl.coef = 32;
            // slow random walks (wind direction, fire roar, gusts)
            if (++sl.walkCount >= 16)
            {
                sl.walkCount = 0;
                for (auto& w : sl.walkT) if (uni (rng) < 0.04f + 0.1f * motion) w = uni (rng);
                if (uni (rng) < 0.03f + 0.12f * motion) sl.gustT = uni (rng);
            }
            const float k = onePoleCoef (1.5f - motion, sr / 32.0f);
            for (int i = 0; i < 4; ++i) sl.walk[i] += (sl.walkT[i] - sl.walk[i]) * k;
            sl.gust += (sl.gustT - sl.gust) * onePoleCoef (2.5f - 1.8f * motion, sr / 32.0f);
            const float bright = std::exp2 ((tone - 0.5f) * 2.4f);
            switch (kind)
            {
                case Rain:    sl.nl1.set (2600.0f * bright, 0.5f, sr); sl.nr1.set (2700.0f * bright, 0.5f, sr); break;
                case Wind:
                {
                    const float f1 = tune (180.0f + 900.0f * sl.walk[0] * bright, key * 0.8f, rng);
                    const float f2 = tune (300.0f + 1400.0f * sl.walk[1] * bright, key * 0.8f, rng);
                    const float q = 2.0f + 10.0f * key;
                    sl.nl1.set (f1, q, sr); sl.nr1.set (f2, q, sr);
                    sl.nl2.set (f2 * 1.5f, q, sr); sl.nr2.set (f1 * 1.5f, q, sr);
                    sl.lowL.set (140.0f, 0.6f, sr); sl.lowR.set (150.0f, 0.6f, sr);
                    break;
                }
                case Stream:  sl.nl1.set (900.0f * bright, 0.8f, sr); sl.nr1.set (1100.0f * bright, 0.8f, sr); break;
                case Crackle: sl.nl1.set (5200.0f * bright, 0.5f, sr); sl.nr1.set (5400.0f * bright, 0.5f, sr);
                              sl.lowL.set (90.0f, 0.7f, sr); sl.lowR.set (95.0f, 0.7f, sr); break;
                case Fire:    sl.nl1.set (260.0f + 200.0f * sl.walk[2], 0.7f, sr); sl.nr1.set (280.0f + 200.0f * sl.walk[3], 0.7f, sr);
                              sl.nl2.set (5000.0f * bright, 0.5f, sr); sl.nr2.set (5300.0f * bright, 0.5f, sr); break;
                case Shore:   break;
                default:      sl.nl1.set (6000.0f, 0.5f, sr); sl.nr1.set (6200.0f, 0.5f, sr); break;
            }
        }
        const float nL = rng.next(), nR = rng.next();
        const float bright = std::exp2 ((tone - 0.5f) * 2.4f);
        switch (kind)
        {
            case Rain:
            {
                ol += sl.nl1.lowpass (nL) * 0.11f * (0.5f + density); orr += sl.nr1.lowpass (nR) * 0.11f * (0.5f + density);
                const float rate = 6.0f + 90.0f * density * density + 30.0f * density * sl.gust * motion;
                if (uni (rng) < chance (rate, sr))
                {
                    const float pan = uni (rng) * 2.0f - 1.0f;
                    if (uni (rng) < 0.55f) sl.spawn (1, (2200.0f + 4000.0f * uni (rng)) * bright, 1.0f, 0.004f + 0.01f * uni (rng), 0.25f + 0.5f * uni (rng), pan, sr, 1.5f);
                    else sl.spawn (0, tune ((1400.0f + 2600.0f * uni (rng)) * bright, key, rng), 1.35f + 0.5f * uni (rng), 0.01f + 0.025f * uni (rng), 0.08f + 0.18f * uni (rng), pan, sr);
                }
                break;
            }
            case Wind:
            {
                const float g = 0.25f + 0.9f * sl.gust;
                float lp;
                const float k1 = 1.0f / (2.0f + 10.0f * key);
                ol += (sl.nl1.process (nL, lp) + 0.5f * sl.nl2.process (nL, lp)) * k1 * g * 0.9f + sl.lowL.lowpass (nL) * 0.25f * g;
                orr += (sl.nr1.process (nR, lp) + 0.5f * sl.nr2.process (nR, lp)) * k1 * g * 0.9f + sl.lowR.lowpass (nR) * 0.25f * g;
                if (uni (rng) < chance (0.15f + 0.6f * density * motion, sr))   // a whistle in the gap of a door
                    sl.spawn (0, tune ((500.0f + 900.0f * uni (rng)) * bright, key, rng), uni (rng) < 0.5f ? 1.06f : 0.94f, 1.5f + 2.0f * uni (rng), 0.05f + 0.05f * key, uni (rng) * 2.0f - 1.0f, sr);
                break;
            }
            case Stream:
            {
                float lp;
                ol += sl.nl1.process (nL, lp) * 0.09f * (0.4f + density); orr += sl.nr1.process (nR, lp) * 0.09f * (0.4f + density);
                const float rate = 25.0f + 220.0f * density;
                if (uni (rng) < chance (rate, sr))
                    sl.spawn (0, tune ((260.0f + 1300.0f * uni (rng) * uni (rng)) * bright, key, rng), 1.5f + 1.2f * uni (rng),
                              0.012f + 0.035f * uni (rng), 0.05f + 0.16f * uni (rng), (uni (rng) * 2.0f - 1.0f) * 0.8f, sr);
                break;
            }
            case Crackle:
            {
                ol += sl.nl1.lowpass (nL) * 0.012f + sl.lowL.lowpass (nL) * 0.06f;
                orr += sl.nr1.lowpass (nR) * 0.012f + sl.lowR.lowpass (nR) * 0.06f;
                const float rate = 2.0f + 70.0f * density * density;
                if (uni (rng) < chance (rate, sr))
                {
                    const float big = uni (rng);
                    sl.spawn (2, (1500.0f + 5000.0f * uni (rng)) * bright, 1.0f, 0.0006f + 0.002f * big, (0.15f + 0.85f * big * big) * 0.7f,
                              (uni (rng) * 2.0f - 1.0f) * 0.6f, sr, 0.7f);
                }
                break;
            }
            case Creatures:
            {
                // crickets: groups of quick chirps; a few birds and frogs now and then
                const int crickets = 1 + (int) (density * 2.99f);
                for (int c = 0; c < crickets; ++c)
                {
                    sl.cricketPh[c] += sl.cricketRate[c] * (0.8f + 0.4f * motion) * dt;
                    if (sl.cricketPh[c] >= 1.0f)
                    {
                        sl.cricketPh[c] -= 1.0f;
                        if (uni (rng) < 0.85f)
                            sl.spawn (3, tune (3900.0f + 900.0f * (float) c + 200.0f * uni (rng), key * 0.4f, rng), 1.0f, 0.09f + 0.05f * uni (rng),
                                      0.05f, c == 0 ? -0.6f : (c == 1 ? 0.55f : 0.0f), sr);
                    }
                }
                if (uni (rng) < chance (0.05f + 0.35f * density, sr))
                {
                    const float f = tune ((1400.0f + 1600.0f * uni (rng)) * bright, key, rng);
                    const float dir = uni (rng) < 0.5f ? 1.25f : 0.82f;
                    const int notes = 1 + (int) (uni (rng) * 3.0f);
                    const float pan = uni (rng) * 1.6f - 0.8f;
                    for (int n = 0; n < notes; ++n)
                        if (auto* x = sl.spawn (0, f * (n % 2 == 0 ? 1.0f : 1.12f), dir, 0.12f + 0.2f * uni (rng), 0.06f + 0.05f * uni (rng), pan, sr))
                            x->t = -0.28f * (float) n;          // the notes of one call follow each other
                }
                if (uni (rng) < chance (0.08f + 0.25f * density * motion, sr))
                    sl.spawn (3, tune (190.0f + 90.0f * uni (rng), key, rng), 0.97f, 0.18f + 0.15f * uni (rng), 0.12f, uni (rng) * 1.4f - 0.7f, sr);
                break;
            }
            case Fire:
            {
                const float roar = 0.4f + 0.6f * sl.gust;
                float lp;
                ol += sl.nl1.lowpass (nL) * 0.22f * roar + sl.nl2.process (nL, lp) * 0.006f;
                orr += sl.nr1.lowpass (nR) * 0.22f * roar + sl.nr2.process (nR, lp) * 0.006f;
                const float rate = 6.0f + 60.0f * density;
                if (uni (rng) < chance (rate, sr))
                    sl.spawn (2, (800.0f + 3500.0f * uni (rng)) * bright, 1.0f, 0.0008f + 0.003f * uni (rng), 0.1f + 0.5f * uni (rng) * uni (rng),
                              (uni (rng) * 2.0f - 1.0f) * 0.7f, sr, 1.2f);
                if (uni (rng) < chance (0.2f + 1.5f * density, sr))   // a big pop
                    sl.spawn (2, 600.0f + 1400.0f * uni (rng), 1.0f, 0.004f, 0.8f, (uni (rng) * 2.0f - 1.0f) * 0.5f, sr, 0.8f);
                break;
            }
            default:   // SHORE: slow waves arriving, the foam fizzing as they fall back
            {
                const float rateK = 0.7f + 0.6f * motion;
                sl.wavePh += dt * rateK / sl.waveLen; if (sl.wavePh >= 1.0f) { sl.wavePh -= 1.0f; sl.waveLen = 7.0f + 6.0f * uni (rng); }
                sl.wavePh2 += dt * rateK / sl.waveLen2; if (sl.wavePh2 >= 1.0f) { sl.wavePh2 -= 1.0f; sl.waveLen2 = 7.0f + 6.0f * uni (rng); }
                auto wave = [] (float x) { return x < 0.4f ? std::pow (x / 0.4f, 2.0f) : std::exp (-(x - 0.4f) * 5.0f); };
                const float a = wave (sl.wavePh), b2 = wave (sl.wavePh2);
                if (sl.coef == 0)
                {
                    sl.nl1.set ((250.0f + 2800.0f * a) * bright, 0.6f, sr); sl.nr1.set ((250.0f + 2800.0f * b2) * bright, 0.6f, sr);
                    sl.nl2.set (5500.0f * bright, 0.7f, sr); sl.nr2.set (5800.0f * bright, 0.7f, sr);
                }
                float lp;
                const float foamA = sl.wavePh > 0.4f ? a : 0.0f, foamB = sl.wavePh2 > 0.4f ? b2 : 0.0f;
                ol += sl.nl1.lowpass (nL) * (0.05f + 0.3f * a) * (0.6f + 0.6f * density) + sl.nl2.process (nL, lp) * 0.05f * foamA * density;
                orr += sl.nr1.lowpass (nR) * (0.05f + 0.3f * b2) * (0.6f + 0.6f * density) + sl.nr2.process (nR, lp) * 0.05f * foamB * density;
                break;
            }
        }

        // the little events
        for (auto& x : sl.b)
        {
            if (! x.on) continue;
            x.t += dt;
            if (x.t < 0.0f) continue;
            const float u = x.t / x.dur;
            if (u >= 1.0f) { x.on = false; continue; }
            float y = 0.0f;
            switch (x.kind)
            {
                case 0:
                {
                    const float f = x.f * (1.0f + (x.ratio - 1.0f) * u);
                    x.ph += f / sr; if (x.ph >= 1.0) x.ph -= 1.0;
                    const float env = std::min (1.0f, u * 12.0f) * (1.0f - u) * (1.0f - u);
                    y = std::sin (kTwoPi * (float) x.ph) * env;
                    break;
                }
                case 1: { float lp; y = x.bp.process (rng.next(), lp) * (1.0f - u) * (1.0f - u) * 0.5f; break; }
                case 2: { float lp; const float imp = x.t < dt * 1.5f ? 1.0f : 0.0f; y = x.bp.process (imp * 3.0f + rng.next() * (1.0f - u) * 0.6f, lp) * (1.0f - u); break; }
                default:
                {
                    x.ph += x.f / sr; if (x.ph >= 1.0) x.ph -= 1.0;
                    const float pulse = 0.5f - 0.5f * std::cos (kTwoPi * x.t * (x.f > 1000.0f ? 33.0f : 11.0f));
                    y = std::sin (kTwoPi * (float) x.ph) * pulse * std::sin (kPi * u);
                    break;
                }
            }
            y *= x.amp;
            ol += y * std::sqrt (0.5f * (1.0f - x.pan));
            orr += y * std::sqrt (0.5f * (1.0f + x.pan));
        }
        static const float kindGain[NumKinds] { 1.1f, 1.0f, 0.8f, 3.2f, 2.8f, 2.4f, 1.4f };
        ol *= kindGain[kind]; orr *= kindGain[kind];
        sl.energy += (std::abs (ol) + std::abs (orr) - sl.energy) * 0.0005f;
        l += ol; r += orr;
    }

    // kindB < 0: only slot A plays.
    void process (int kindA, int kindB, float blend, float density, float tone, float motion, float key, float& l, float& r)
    {
        float al = 0.0f, ar = 0.0f, bl = 0.0f, br = 0.0f;
        processSlot (s[0], kindA, density, tone, motion, key, al, ar);
        if (kindB >= 0) processSlot (s[1], kindB, density, tone, motion, key, bl, br);
        const float ga = kindB >= 0 ? std::sqrt (1.0f - blend) : 1.0f, gb = std::sqrt (blend);
        l += al * ga + bl * gb;
        r += ar * ga + br * gb;
    }
};

} // namespace dw
