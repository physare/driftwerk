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
// note, TINES are little metal tongues (slightly inharmonic). GLASS is a struck glass (FM),
// PLUCK a plucked string, MARIMBA a wooden bar, VOICE a sung vowel, BOWED a slow bowed swell,
// BUBBLES rise and pop, CHIRPS are little birds, and GRAIN plays the GRAIN sound at the note.
struct SlimeVoices
{
    static constexpr int V = 16, KS = 8192;
    enum Type { Drop = 0, Breath, Tine, Glass, Pluck, Marimba, Vowel, Bowed, Bubble, Chirp, Grain, NumTypes };
    struct Voice
    {
        bool on = false, reverse = false;
        int type = 0;
        float note = 60.0f, vel = 0.0f, pan = 0.0f, t = 0.0f, delay = 0.0f;
        float bendFrom = 0.0f, bendTime = 0.03f, glide = 0.0f, glideTime = 0.3f;
        float atk = 0.002f, len = 1.0f, env = 0.0f, nGain = 1.0f;
        double ph1 = 0.0, ph2 = 0.0, ph3 = 0.0;
        Svf bp1, bp2, bp3;
        float fm = 0.0f, lp = 0.0f, prev = 0.0f, damp = 0.998f, bright = 0.5f, dcx = 0.0f, dcy = 0.0f;
        int ksW = 0, excite = 0;
        const SampleData* sd = nullptr;
        double spos = 0.0;
    };
    Voice v[V];
    std::vector<float> ks[V];          // PLUCK strings (allocated in prepare)
    Random rng { 0x5117E5u };
    float sr = 44100.0f;
    Svf toneL, toneR;
    int coefCounter = 0;
    const SampleData* grainSound = nullptr;   // set by the processor: the GRAIN sound (kept alive by it)
    float grainStart = 0.3f;                  // where in it GRAIN notes start (0..1)

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (auto& vo : v) vo.on = false;
        for (auto& b : ks) b.assign ((size_t) KS, 0.0f);
    }

    bool anySounding() const { for (auto& vo : v) if (vo.on) return true; return false; }

    static float blep (double ph, double inc)
    {
        float t = (float) ph, dt = (float) inc;
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
    static float saw (double ph, double inc) { return (float) (2.0 * ph - 1.0) - blep (ph, inc); }

    // note: MIDI. delaySec: when it starts. glideFrom: semitones it slides in from (0 = none).
    void trigger (int type, float note, float vel, float pan, float delaySec, float detuneCents, float glideFrom, bool reverse)
    {
        int best = 0; float quiet = 1.0e9f;
        for (int i = 0; i < V; ++i)
        {
            if (! v[i].on) { best = i; quiet = -1.0f; break; }
            const float lvl = v[i].delay > 0.0f ? v[i].vel : v[i].env * v[i].vel;   // a note still waiting to start is kept
            if (lvl < quiet) { quiet = lvl; best = i; }
        }
        auto& vo = v[best];
        vo = Voice {};
        vo.on = true;
        vo.type = type < 0 || type >= NumTypes ? 0 : type;
        vo.reverse = reverse;
        vo.note = note + detuneCents * 0.01f;
        vo.vel = vel;
        vo.pan = clampf (pan, -1.0f, 1.0f);
        vo.delay = delaySec;
        vo.glide = glideFrom;
        vo.glideTime = 0.25f + 0.5f * uni (rng);
        const float f = midiToHz (vo.note);
        switch (vo.type)
        {
            case Drop:    vo.bendFrom = 6.0f + 7.0f * uni (rng); vo.bendTime = 0.018f + 0.02f * uni (rng);
                          vo.atk = 0.0015f; vo.len = 0.35f + 0.6f * uni (rng); break;
            case Breath:  vo.atk = 0.2f + 0.45f * uni (rng); vo.len = 1.4f + 1.4f * uni (rng); break;
            case Glass:   vo.atk = 0.0015f; vo.len = 1.6f + 1.6f * uni (rng); vo.fm = 1.2f + 1.4f * uni (rng); break;
            case Pluck:
            {
                vo.atk = 0.001f; vo.len = 2.0f + 1.5f * uni (rng);
                vo.bright = 0.35f + 0.5f * uni (rng);
                // how long the string rings: about len seconds whatever the note
                const float loops = f * vo.len * 0.6f;
                vo.damp = std::pow (0.001f, 1.0f / std::max (1.0f, loops));
                vo.excite = (int) clampf (sr / f, 2.0f, (float) KS - 4.0f);
                std::fill (ks[best].begin(), ks[best].end(), 0.0f);
                break;
            }
            case Marimba: vo.atk = 0.001f; vo.len = 0.8f + 0.7f * uni (rng); break;
            case Vowel:
            {
                vo.atk = 0.25f + 0.3f * uni (rng); vo.len = 1.6f + 1.2f * uni (rng);
                float fm3[3]; vowelFormants (uni (rng) * 0.75f, fm3);
                vo.bp1.set (fm3[0], 9.0f, sr); vo.bp2.set (fm3[1], 11.0f, sr); vo.bp3.set (fm3[2], 14.0f, sr);
                break;
            }
            case Bowed:   vo.atk = 0.5f + 0.6f * uni (rng); vo.len = 2.6f + 1.6f * uni (rng); vo.bp1.set (std::min (f * 5.0f, 9000.0f), 0.8f, sr); break;
            case Bubble:  vo.atk = 0.001f; vo.len = 0.09f + 0.12f * uni (rng); break;
            case Chirp:   vo.atk = 0.004f; vo.len = 0.12f + 0.16f * uni (rng); vo.fm = 18.0f + 22.0f * uni (rng); break;
            case Grain:
                vo.sd = grainSound;
                vo.atk = 0.006f; vo.len = 1.0f + 1.0f * uni (rng);
                if (vo.sd != nullptr && vo.sd->length() > 16)
                    vo.spos = (double) clampf (grainStart + (uni (rng) - 0.5f) * 0.06f, 0.0f, 0.95f) * (double) vo.sd->length();
                break;
            default:      vo.atk = 0.001f; vo.len = 1.2f + 1.6f * uni (rng); break;
        }
        if (reverse) vo.len = std::max (vo.len, 0.9f) * 1.3f;
        if (vo.type == Breath)
        {
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
        for (int vi = 0; vi < V; ++vi)
        {
            auto& vo = v[vi];
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
            if ((vo.type == Vowel || vo.type == Bowed) && vo.t > 0.3f)   // a slow vibrato once the note settles
                pitch += 0.12f * std::min (1.0f, (vo.t - 0.3f) * 1.5f) * std::sin (kTwoPi * 5.1f * vo.t + (float) vi);
            if (vo.type == Bubble) pitch += -7.0f + 19.0f * (1.0f - std::exp (-vo.t / (vo.len * 0.45f)));   // bubbles rise
            if (vo.type == Chirp)                                                                         // a bird: a quick fall, then a trill
                pitch += 24.0f + 9.0f * std::exp (-vo.t / 0.025f) + 1.2f * std::sin (kTwoPi * vo.fm * vo.t);
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
                case Glass:   // a struck glass: an inharmonic FM shimmer that fades into a pure tone
                {
                    vo.ph1 += f / sr; if (vo.ph1 >= 1.0) vo.ph1 -= 1.0;
                    vo.ph2 += f * 3.53f / sr; if (vo.ph2 >= 1.0) vo.ph2 -= 1.0;
                    vo.ph3 += f * 2.76f / sr; if (vo.ph3 >= 1.0) vo.ph3 -= 1.0;
                    const float idx = vo.fm * (vo.reverse ? e : std::exp (-vo.t / (vo.len * 0.12f)));
                    y = std::sin (kTwoPi * (float) vo.ph1 + idx * std::sin (kTwoPi * (float) vo.ph2))
                        + 0.12f * std::sin (kTwoPi * (float) vo.ph3) * (vo.reverse ? e : std::exp (-vo.t / (vo.len * 0.2f)));
                    y *= e * 0.75f;
                    break;
                }
                case Pluck:   // a plucked string (Karplus-Strong)
                {
                    auto& b = ks[vi];
                    const float d = clampf (sr / f - 0.5f, 2.0f, (float) KS - 4.0f);
                    float rp = (float) vo.ksW - d;
                    while (rp < 0.0f) rp += (float) KS;
                    const int i0 = (int) rp; const float fr = rp - (float) i0;
                    const float out = b[(size_t) i0] + (b[(size_t) ((i0 + 1) & (KS - 1))] - b[(size_t) i0]) * fr;
                    float nx = vo.damp * 0.5f * (out + vo.prev);
                    vo.prev = out;
                    if (vo.excite > 0)
                    {
                        --vo.excite;
                        vo.lp += (rng.next() - vo.lp) * vo.bright;   // a softer or brighter pluck
                        nx += vo.lp;
                    }
                    b[(size_t) vo.ksW] = nx;
                    vo.ksW = (vo.ksW + 1) & (KS - 1);
                    vo.dcy = out - vo.dcx + 0.995f * vo.dcy; vo.dcx = out;   // no DC rumble from the string
                    y = vo.dcy * (vo.reverse ? e : std::min (1.0f, e * 1.6f)) * 3.6f;
                    break;
                }
                case Marimba: // a wooden bar: a round fundamental, a quick fourth harmonic and a tick
                {
                    vo.ph1 += f / sr; if (vo.ph1 >= 1.0) vo.ph1 -= 1.0;
                    vo.ph2 += f * 3.99f / sr; if (vo.ph2 >= 1.0) vo.ph2 -= 1.0;
                    vo.ph3 += f * 9.85f / sr; if (vo.ph3 >= 1.0) vo.ph3 -= 1.0;
                    const float e2 = vo.reverse ? e * e : std::exp (-vo.t / (vo.len * 0.09f));
                    const float e3 = vo.reverse ? e * e * e : std::exp (-vo.t / (vo.len * 0.025f));
                    y = std::sin (kTwoPi * (float) vo.ph1) * e + 0.35f * std::sin (kTwoPi * (float) vo.ph2) * e2
                        + 0.12f * std::sin (kTwoPi * (float) vo.ph3) * e3;
                    if (vo.t < 0.004f) y += rng.next() * 0.25f * (1.0f - vo.t / 0.004f);
                    y *= 0.65f;
                    break;
                }
                case Vowel:   // a sung vowel: a soft buzz through three formants
                {
                    const double inc = f / sr;
                    vo.ph1 += inc; if (vo.ph1 >= 1.0) vo.ph1 -= 1.0;
                    const float src = saw (vo.ph1, inc) + 0.05f * rng.next();
                    float lp;
                    const float fo = vo.bp1.process (src, lp) / 9.0f + 0.6f * vo.bp2.process (src, lp) / 11.0f
                                   + 0.25f * vo.bp3.process (src, lp) / 14.0f;
                    vo.ph2 += f / sr; if (vo.ph2 >= 1.0) vo.ph2 -= 1.0;
                    y = (fo * 2.2f + 0.25f * std::sin (kTwoPi * (float) vo.ph2)) * e;
                    break;
                }
                case Bowed:   // a bowed string swelling: two saws, a little apart, through a soft filter
                {
                    const double i1 = f / sr, i2 = f * 1.0035 / sr;
                    vo.ph1 += i1; if (vo.ph1 >= 1.0) vo.ph1 -= 1.0;
                    vo.ph2 += i2; if (vo.ph2 >= 1.0) vo.ph2 -= 1.0;
                    const float src = saw (vo.ph1, i1) + saw (vo.ph2, i2) + 0.08f * rng.next() * e;
                    y = vo.bp1.lowpass (src) * 0.42f * e;
                    break;
                }
                case Bubble:
                {
                    vo.ph1 += f / sr; if (vo.ph1 >= 1.0) vo.ph1 -= 1.0;
                    y = std::sin (kTwoPi * (float) vo.ph1) * e * 1.1f;
                    break;
                }
                case Chirp:
                {
                    vo.ph1 += f / sr; if (vo.ph1 >= 1.0) vo.ph1 -= 1.0;
                    y = std::sin (kTwoPi * (float) vo.ph1) * e * 1.7f;
                    break;
                }
                case Grain:   // the GRAIN sound, played at the note
                {
                    const auto* sd = vo.sd;
                    const int n = sd != nullptr ? sd->length() : 0;
                    if (n < 16) { vo.on = false; continue; }
                    const float root = sd->rootMidi >= 0.0f ? sd->rootMidi : 60.0f;
                    vo.spos += std::exp2 ((pitch - root) / 12.0f) * sd->sampleRate / sr;
                    if (vo.spos >= (double) (n - 2)) { vo.on = false; continue; }
                    const int i0 = (int) vo.spos; const float fr = (float) (vo.spos - (double) i0);
                    const float a = 0.5f * (sd->left[(size_t) i0] + (sd->right.empty() ? sd->left[(size_t) i0] : sd->right[(size_t) i0]));
                    const float b = 0.5f * (sd->left[(size_t) i0 + 1] + (sd->right.empty() ? sd->left[(size_t) i0 + 1] : sd->right[(size_t) i0 + 1]));
                    y = (a + (b - a) * fr) * e * 4.5f;
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
// PULSE: percussion, all synthesised. Five lanes (low, backbeat, tuned accent, noise, tiny high)
// and six KITS that decide what each lane is: SOFT (thump, hand drum, wood, brush, tick),
// DEEP (boom, snare, rim, hat, bell), DUSTY (lo-fi kick, snare, rim, hat, shaker), GLASS (gong,
// glass, chime, shimmer, spark), HANDS (djembe bass and tone, tabla, shaker, clap), GLITCH and IDM
// (crisp and digital, made for STUTTER rolls).
struct Pulse
{
    static constexpr int Lanes = 5, Steps = 16, NumKits = 7;
    enum Lane { Thump = 0, Skin = 1, Wood = 2, Brush = 3, Tick = 4 };

    // How one lane of one kit is made: a tone (with a pitch drop, FM and two more modes),
    // filtered noise, a click and some drive.
    struct VoiceDef
    {
        float tone, bend, bendT, tau;          // tone level, pitch drop (x the note), its time, decay (s)
        float r2, a2, k2, r3, a3, k3;          // two more modes: ratio, level, decay (x tau)
        float fmR, fmI, fmT;                   // FM ratio, index, index decay (s)
        float noise, nf, nq, ntau, natk;       // noise level, centre (Hz), Q, decay (s), attack (s)
        float click, drive, gain;
        float altPitch, altTau;                // variant 1: pitch and decay multipliers
        int bursts;                            // a clap: the noise starts this many times
    };
    static const VoiceDef& def (int kit, int lane)
    {
        static const VoiceDef defs[NumKits][Lanes] {
            {   // SOFT
                { 1.0f, 1.4f, 0.022f, 0.24f,  2.0f, 0.18f, 0.5f,  0, 0, 1,  0, 0, 1,  0.0f, 900, 1, 0.01f, 0.001f,  0.15f, 0.25f, 1.0f,  1.0f, 0.6f, 1 },
                { 0.7f, 0.22f, 0.035f, 0.17f, 1.59f, 0.3f, 0.45f, 0, 0, 1,  0, 0, 1,  0.3f, 900, 1.2f, 0.012f, 0.0005f, 0.1f, 0.1f, 1.0f,  2.1f, 0.35f, 1 },
                { 0.6f, 0.0f, 0.01f, 0.075f,  2.76f, 0.45f, 0.33f, 5.4f, 0.15f, 0.16f, 0, 0, 1,  0.0f, 900, 1, 0.01f, 0.001f, 0.05f, 0.0f, 1.0f,  1.0f, 2.1f, 1 },
                { 0.0f, 0.0f, 0.01f, 0.05f,   1, 0, 1, 0, 0, 1,  0, 0, 1,  0.55f, 6200, 0.9f, 0.045f, 0.0025f, 0.0f, 0.0f, 1.0f,  1.0f, 2.4f, 1 },
                { 0.4f, 0.0f, 0.01f, 0.028f,  1, 0, 1, 0, 0, 1,  3.01f, 1.3f, 0.008f,  0.0f, 900, 1, 0.01f, 0.001f, 0.0f, 0.0f, 1.0f,  1.0f, 3.2f, 1 } },
            {   // DEEP: a long boom, a snappy snare, a rim, a closed hat, a bell
                { 1.0f, 2.2f, 0.035f, 0.75f,  2.0f, 0.1f, 0.4f,  0, 0, 1,  0, 0, 1,  0.0f, 900, 1, 0.01f, 0.001f,  0.35f, 0.45f, 1.1f,  1.0f, 0.45f, 1 },
                { 0.5f, 0.6f, 0.02f, 0.09f,   1.48f, 0.4f, 0.6f,  0, 0, 1,  0, 0, 1,  0.85f, 2600, 0.7f, 0.15f, 0.0005f, 0.3f, 0.3f, 1.0f,  1.0f, 0.5f, 1 },
                { 0.7f, 0.1f, 0.005f, 0.035f, 2.3f, 0.5f, 0.6f, 4.1f, 0.2f, 0.4f, 0, 0, 1,  0.15f, 3000, 1.5f, 0.01f, 0.0003f, 0.3f, 0.2f, 1.0f,  1.0f, 2.5f, 1 },
                { 0.0f, 0.0f, 0.01f, 0.03f,   1, 0, 1, 0, 0, 1,  0, 0, 1,  0.6f, 9000, 0.8f, 0.03f, 0.0008f, 0.1f, 0.1f, 1.1f,  1.0f, 5.0f, 1 },
                { 0.5f, 0.0f, 0.01f, 0.35f,   1.48f, 0.7f, 0.9f, 0, 0, 1,  2.0f, 0.6f, 0.05f,  0.0f, 900, 1, 0.01f, 0.001f, 0.05f, 0.25f, 0.9f,  1.0f, 0.3f, 1 } },
            {   // DUSTY: a lo-fi break
                { 1.0f, 1.8f, 0.028f, 0.26f,  2.0f, 0.2f, 0.4f,  0, 0, 1,  0, 0, 1,  0.05f, 1500, 0.8f, 0.01f, 0.0005f, 0.4f, 0.55f, 1.1f,  1.0f, 0.6f, 1 },
                { 0.45f, 0.4f, 0.02f, 0.07f,  1.6f, 0.3f, 0.6f,  0, 0, 1,  0, 0, 1,  0.9f, 1900, 0.65f, 0.17f, 0.0008f, 0.25f, 0.5f, 1.0f,  1.0f, 0.55f, 1 },
                { 0.6f, 0.05f, 0.005f, 0.04f, 2.6f, 0.4f, 0.5f,  0, 0, 1,  0, 0, 1,  0.25f, 2500, 1.2f, 0.015f, 0.0003f, 0.3f, 0.35f, 1.0f,  1.0f, 2.0f, 1 },
                { 0.0f, 0.0f, 0.01f, 0.04f,   1, 0, 1, 0, 0, 1,  0, 0, 1,  0.6f, 7200, 0.7f, 0.05f, 0.001f, 0.05f, 0.3f, 1.0f,  1.0f, 3.0f, 1 },
                { 0.0f, 0.0f, 0.01f, 0.05f,   1, 0, 1, 0, 0, 1,  0, 0, 1,  0.55f, 5200, 1.1f, 0.06f, 0.012f, 0.0f, 0.2f, 1.0f,  1.0f, 1.8f, 1 } },
            {   // GLASS: everything rings
                { 0.8f, 0.0f, 0.01f, 1.4f,    2.76f, 0.35f, 0.4f, 5.4f, 0.15f, 0.2f,  1.41f, 0.9f, 0.4f,  0.0f, 900, 1, 0.01f, 0.001f, 0.0f, 0.0f, 0.9f,  1.0f, 0.5f, 1 },
                { 0.6f, 0.0f, 0.01f, 0.5f,    2.0f, 0.3f, 0.5f, 3.0f, 0.12f, 0.3f,  3.5f, 0.6f, 0.08f,  0.0f, 900, 1, 0.01f, 0.001f, 0.05f, 0.0f, 0.9f,  1.5f, 0.6f, 1 },
                { 0.5f, 0.0f, 0.01f, 0.9f,    2.0f, 0.25f, 0.6f, 4.0f, 0.1f, 0.3f,  1.0f, 0.4f, 0.2f,  0.0f, 900, 1, 0.01f, 0.001f, 0.0f, 0.0f, 0.9f,  2.0f, 0.5f, 1 },
                { 0.2f, 0.0f, 0.01f, 0.3f,    1.5f, 0.5f, 1.0f, 2.25f, 0.4f, 1.0f,  7.1f, 2.0f, 0.1f,  0.3f, 8000, 2.0f, 0.3f, 0.06f, 0.0f, 0.0f, 0.9f,  1.0f, 1.8f, 1 },
                { 0.35f, 0.0f, 0.01f, 0.25f,  2.0f, 0.3f, 0.6f, 0, 0, 1,  4.0f, 1.0f, 0.03f,  0.0f, 900, 1, 0.01f, 0.001f, 0.0f, 0.0f, 0.9f,  1.5f, 1.5f, 1 } },
            {   // HANDS: djembe bass and tone, tabla, shaker, clap
                { 0.9f, 0.5f, 0.03f, 0.2f,    1.5f, 0.25f, 0.5f,  0, 0, 1,  0, 0, 1,  0.2f, 700, 1.0f, 0.02f, 0.0005f, 0.15f, 0.2f, 1.0f,  1.0f, 0.6f, 1 },
                { 0.7f, 0.15f, 0.02f, 0.12f,  1.59f, 0.4f, 0.5f, 2.14f, 0.2f, 0.3f, 0, 0, 1,  0.45f, 2200, 1.0f, 0.02f, 0.0004f, 0.15f, 0.15f, 1.0f,  1.0f, 0.4f, 1 },
                { 0.6f, -0.12f, 0.12f, 0.3f,  2.0f, 0.35f, 0.5f, 3.0f, 0.15f, 0.3f,  0, 0, 1,  0.1f, 3000, 1.5f, 0.008f, 0.0003f, 0.1f, 0.05f, 1.0f,  1.5f, 0.3f, 1 },
                { 0.0f, 0.0f, 0.01f, 0.05f,   1, 0, 1, 0, 0, 1,  0, 0, 1,  0.55f, 6000, 1.3f, 0.07f, 0.015f, 0.0f, 0.1f, 1.0f,  1.0f, 1.6f, 1 },
                { 0.0f, 0.0f, 0.01f, 0.05f,   1, 0, 1, 0, 0, 1,  0, 0, 1,  0.9f, 1400, 0.9f, 0.09f, 0.0005f, 0.0f, 0.2f, 1.1f,  1.0f, 0.5f, 3 } },
            {   // GLITCH: blips, crushed noise, zaps and static
                { 1.0f, 6.0f, 0.012f, 0.18f,  1, 0, 1, 0, 0, 1,  0, 0, 1,  0.0f, 900, 1, 0.01f, 0.001f, 0.2f, 0.6f, 1.0f,  1.0f, 0.4f, 1 },
                { 0.3f, 3.0f, 0.004f, 0.05f,  1, 0, 1, 0, 0, 1,  2.5f, 3.0f, 0.02f,  0.8f, 3000, 0.6f, 0.06f, 0.0002f, 0.4f, 0.7f, 1.0f,  0.5f, 1.5f, 1 },
                { 0.6f, -0.8f, 0.03f, 0.08f,  1, 0, 1, 0, 0, 1,  1.5f, 4.0f, 0.04f,  0.0f, 900, 1, 0.01f, 0.001f, 0.1f, 0.3f, 0.9f,  2.0f, 0.5f, 1 },
                { 0.0f, 0.0f, 0.01f, 0.03f,   1, 0, 1, 0, 0, 1,  0, 0, 1,  0.6f, 4000, 3.0f, 0.02f, 0.0002f, 0.3f, 0.5f, 1.1f,  1.0f, 3.0f, 4 },
                { 0.4f, 0.0f, 0.01f, 0.02f,   1, 0, 1, 0, 0, 1,  1.0f, 2.5f, 0.01f,  0.0f, 900, 1, 0.01f, 0.001f, 0.0f, 0.4f, 1.0f,  2.0f, 2.0f, 1 } },
            {   // IDM: a tight kick, a bright snare, a zap, a needle hat, a blip
                { 1.0f, 3.5f, 0.015f, 0.16f,  2.0f, 0.15f, 0.3f,  0, 0, 1,  0, 0, 1,  0.1f, 4000, 0.7f, 0.004f, 0.0001f, 0.5f, 0.6f, 1.05f,  1.3f, 0.6f, 1 },
                { 0.5f, 1.0f, 0.01f, 0.06f,   1.7f, 0.4f, 0.6f,  0, 0, 1,  0, 0, 1,  0.9f, 4200, 0.8f, 0.09f, 0.0003f, 0.35f, 0.55f, 1.0f,  1.5f, 0.5f, 1 },
                { 0.7f, 8.0f, 0.006f, 0.05f,  1, 0, 1, 0, 0, 1,  2.01f, 3.0f, 0.01f,  0.0f, 900, 1, 0.01f, 0.001f, 0.2f, 0.5f, 0.9f,  0.5f, 1.5f, 1 },
                { 0.0f, 0.0f, 0.01f, 0.012f,  1, 0, 1, 0, 0, 1,  0, 0, 1,  0.65f, 10000, 1.2f, 0.012f, 0.0001f, 0.1f, 0.3f, 1.1f,  1.0f, 4.0f, 1 },
                { 0.5f, 0.0f, 0.01f, 0.03f,   1, 0, 1, 0, 0, 1,  3.0f, 1.5f, 0.005f,  0.0f, 900, 1, 0.01f, 0.001f, 0.0f, 0.2f, 0.9f,  2.0f, 1.0f, 1 } } };
        return defs[clampKit (kit)][lane < 0 ? 0 : (lane >= Lanes ? Lanes - 1 : lane)];
    }
    static int clampKit (int k) { return k < 0 ? 0 : (k >= NumKits ? NumKits - 1 : k); }
    static const char* laneName (int kit, int lane)
    {
        static const char* names[NumKits][Lanes] {
            { "THUMP", "SKIN", "WOOD", "BRUSH", "TICK" }, { "BOOM", "SNARE", "RIM", "HAT", "BELL" },
            { "KICK", "SNARE", "RIM", "HAT", "SHAKER" }, { "GONG", "GLASS", "CHIME", "SHIMMER", "SPARK" },
            { "BASS", "TONE", "TABLA", "SHAKER", "CLAP" }, { "DROP", "CRUSH", "ZAP", "STATIC", "BLIP" },
            { "KICK", "SNARE", "ZAP", "NEEDLE", "BLIP" } };
        return names[clampKit (kit)][lane < 0 ? 0 : (lane >= Lanes ? Lanes - 1 : lane)];
    }

    // rate: a pitch change on top of the drum's own (a roll sliding, a fill climbing), used by
    // your own samples, which ignore the key.
    struct Hit { int lane = 0; float vel = 0.0f, freq = 100.0f, pan = 0.0f; int wait = 0, variant = 0; float rate = 1.0f; };
    struct Drum
    {
        bool on = false;
        int variant = 0, kit = 0;
        float t = 0.0f, vel = 0.0f, f = 100.0f, pan = 0.0f, tauMul = 1.0f, noiseMul = 1.0f, life = 1.0f;
        double ph = 0.0, ph2 = 0.0, ph3 = 0.0, mph = 0.0;
        Svf bp;
        const SampleData* sd = nullptr;     // your own sound for this drum (kept alive by the processor)
        double spos = 0.0, sinc = 1.0;
        float sTau = 0.0f;                  // its decay (0 = the whole sound)
    };
    static constexpr int QN = 160;
    Hit queue[QN];
    int qn = 0;
    Drum d[Lanes];
    Random rng { 0xB0A7u };
    float sr = 44100.0f;
    Svf toneL, toneR;
    int coefCounter = 0;
    int hits = 0;                     // hits played so far (the editor flashes the lanes)
    int laneHits[Lanes] {};
    // Set by the processor each block
    int kit = 0;
    float punch = 0.3f;
    float laneLevel[Lanes] { 1, 1, 1, 1, 1 }, lanePitch[Lanes] {}, laneDecay[Lanes] { 1, 1, 1, 1, 1 };
    const SampleData* laneSample[Lanes] {};   // your own one-shots, nullptr = the kit's drum
    float crushL = 0.0f, crushR = 0.0f, crushPh = 0.0f;

    void prepare (float sampleRate) { sr = sampleRate; qn = 0; for (auto& x : d) x.on = false; }
    static float kitGainOf (int k)
    {
        static const float kitGain[NumKits] { 1.0f, 1.25f, 1.4f, 0.55f, 1.15f, 1.9f, 1.5f };
        return kitGain[clampKit (k)];
    }
    static constexpr float sampleGain = 0.75f;   // your own one-shots (normalised to a 0.9 peak) next to the kit
    bool busy() const { if (qn > 0) return true; for (auto& x : d) if (x.on) return true; return false; }

    void schedule (int lane, float vel, float freq, float pan, int waitSamples, int variant, float rate = 1.0f)
    {
        if (qn >= QN) return;
        queue[qn++] = { lane, vel, freq, pan, std::max (0, waitSamples), variant, rate };
    }

    void start (const Hit& h)
    {
        auto& x = d[h.lane];
        const auto& v = def (kit, h.lane);
        x.on = true; x.t = 0.0f; x.vel = h.vel * laneLevel[h.lane]; x.pan = h.pan; x.variant = h.variant; x.kit = kit;
        const float pitch = std::exp2 (lanePitch[h.lane] / 12.0f);
        x.sd = laneSample[h.lane];
        if (x.sd != nullptr && x.sd->length() > 16)
        {   // Your own sound: PITCH and the hit's slide change its speed, DECAY shortens it, variant 2 plays it backwards.
            const double rate = clampf (h.rate * pitch, 0.25f, 4.0f);
            x.sinc = x.sd->sampleRate / (double) sr * rate;
            x.spos = h.variant == 2 ? (double) (x.sd->length() - 2) : 0.0;
            if (h.variant == 2) x.sinc = -x.sinc;
            const float dec = laneDecay[h.lane];
            x.sTau = dec >= 0.99f ? 0.0f : 0.4f * dec * dec;
            x.life = (float) ((double) x.sd->length() / std::abs (x.sinc) / (double) sr) + 0.01f;
            if (x.sTau > 0.0f) x.life = std::min (x.life, x.sTau * 9.0f + 0.01f);
            ++hits; ++laneHits[h.lane];
            return;
        }
        x.sd = nullptr;
        x.f = h.freq * pitch * (h.variant == 1 ? v.altPitch : 1.0f);
        x.tauMul = laneDecay[h.lane] * (h.variant == 1 ? v.altTau : 1.0f);
        x.noiseMul = 1.0f;
        x.ph = x.ph2 = x.ph3 = x.mph = 0.0;
        if (v.noise > 0.0f) x.bp.set (clampf (v.nf * pitch * (h.lane == Brush ? h.freq / 6200.0f : 1.0f), 200.0f, 15000.0f), v.nq, sr);
        // how long it can ring before it is silent
        x.life = std::max (v.tau * x.tauMul * std::max (1.0f, std::max (v.k2, v.k3)), v.ntau * x.tauMul + v.natk) * 9.0f + 0.02f;
        if (h.variant == 2) x.life = std::max (x.life, 0.3f);
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
            if (t > x.life) { x.on = false; continue; }
            if (x.sd != nullptr)
            {
                const auto& sd = *x.sd;
                const int i0 = (int) x.spos;
                if (i0 < 0 || i0 >= sd.length() - 1) { x.on = false; continue; }
                const float fr = (float) (x.spos - (double) i0);
                float yl = sd.left[(size_t) i0] + (sd.left[(size_t) i0 + 1] - sd.left[(size_t) i0]) * fr;
                float yr = sd.right[(size_t) i0] + (sd.right[(size_t) i0 + 1] - sd.right[(size_t) i0]) * fr;
                x.spos += x.sinc;
                float env = x.sTau > 0.0f ? std::exp (-t / x.sTau) : 1.0f;
                if (x.sinc < 0.0 && t < 0.004f) env *= t / 0.004f;          // backwards: no click where it starts
                const float drive = punch * 0.8f;
                if (drive > 0.0f)
                {
                    yl = softClip (yl * (1.0f + 3.0f * drive)) / (1.0f + 1.4f * drive);
                    yr = softClip (yr * (1.0f + 3.0f * drive)) / (1.0f + 1.4f * drive);
                }
                // The same loudness whatever the kit (the kit gain below is undone).
                const float g = x.vel * env * sampleGain / kitGainOf (kit);
                ol += yl * g * std::min (1.0f, 1.0f - x.pan);
                orr += yr * g * std::min (1.0f, 1.0f + x.pan);
                continue;
            }
            const auto& v = def (x.kit, lane);
            const float tau = v.tau * x.tauMul;
            float y = 0.0f;
            if (v.tone > 0.0f)
            {
                const float f = x.f * (1.0f + v.bend * std::exp (-t / v.bendT));
                x.ph += f / sr; if (x.ph >= 1.0) x.ph -= 1.0;
                float phase = kTwoPi * (float) x.ph;
                if (v.fmI > 0.0f)
                {
                    x.mph += f * v.fmR / sr; if (x.mph >= 1.0) x.mph -= 1.0;
                    phase += v.fmI * std::exp (-t / v.fmT) * std::sin (kTwoPi * (float) x.mph);
                }
                float tn = std::sin (phase) * std::exp (-t / tau);
                if (v.a2 > 0.0f) { x.ph2 += f * v.r2 / sr; if (x.ph2 >= 1.0) x.ph2 -= 1.0; tn += v.a2 * std::sin (kTwoPi * (float) x.ph2) * std::exp (-t / (tau * v.k2)); }
                if (v.a3 > 0.0f) { x.ph3 += f * v.r3 / sr; if (x.ph3 >= 1.0) x.ph3 -= 1.0; tn += v.a3 * std::sin (kTwoPi * (float) x.ph3) * std::exp (-t / (tau * v.k3)); }
                y += tn * v.tone * std::min (1.0f, t / 0.0012f);
            }
            if (v.noise > 0.0f)
            {
                float env;
                const float ntau = v.ntau * x.tauMul;
                if (x.variant == 2)   // a reversed swell
                {
                    const float rise = 0.16f;
                    env = t < rise ? (t / rise) * (t / rise) * (t / rise) : std::exp (-(t - rise) / 0.012f);
                }
                else if (v.bursts > 1 && t < 0.011f * (float) v.bursts)   // a clap: a few quick bursts
                    env = std::exp (-std::fmod (t, 0.011f) / 0.0035f);
                else
                {
                    const float t0 = v.bursts > 1 ? t - 0.011f * (float) v.bursts : t;
                    env = (v.natk > 0.0f ? std::min (1.0f, t / v.natk) : 1.0f) * std::exp (-std::max (0.0f, t0) / ntau);
                }
                float lp;
                y += x.bp.process (rng.next(), lp) / std::max (0.3f, v.nq) * 1.6f * env * v.noise;
            }
            if (v.click > 0.0f && t < 0.002f) y += v.click * (1.0f - t / 0.002f) * (rng.next() * 0.5f + 0.7f);
            const float drive = v.drive + punch * 0.8f;
            if (drive > 0.0f) y = softClip (y * (1.0f + 3.0f * drive)) / (1.0f + 1.4f * drive);
            y *= x.vel * v.gain;
            ol += y * std::sqrt (0.5f * (1.0f - x.pan));
            orr += y * std::sqrt (0.5f * (1.0f + x.pan));
        }
        // DUSTY and GLITCH are crushed a little: fewer samples, fewer bits
        if (kit == 2 || kit == 5)
        {
            crushPh += (kit == 2 ? 14000.0f : 7000.0f) / sr;
            if (crushPh >= 1.0f)
            {
                crushPh -= 1.0f;
                const float q = kit == 2 ? 512.0f : 48.0f;
                crushL = std::round (ol * q) / q; crushR = std::round (orr * q) / q;
            }
            ol = ol * 0.35f + crushL * 0.65f; orr = orr * 0.35f + crushR * 0.65f;
        }
        // The kits are balanced so that switching kit keeps the level about the same.
        ol *= kitGainOf (kit); orr *= kitGainOf (kit);
        // PUNCH: the whole kit pushed into a soft clipper, so it comes forward without spiking
        const float pg = 1.0f + 2.5f * punch;
        l += softClip (toneL.lowpass (ol) * pg) * (1.0f + 0.5f * punch) / pg;
        r += softClip (toneR.lowpass (orr) * pg) * (1.0f + 0.5f * punch) / pg;
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
