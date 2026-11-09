// DREAM: an Eno-style loop bank. Five voices whose cycles have unrelated lengths, so their
// notes drift in and out of phase and never line up the same way twice. SOUND picks what
// every loop plays: twelve synthesised tones, or MY SOUND (a file you load, pitched to the chord).
#pragma once

#include "DSP.h"
#include "Waves.h"

namespace dw
{
struct DreamBank
{
    static constexpr int V = 5;
    enum Sound { Keys = 0, Pad, Choir, Strings, Flute, Organ, MusicBox, Vibes, Bells, GlassHarp, Waves, Breath, MySound, NumSounds };

    static const char* soundName (int s)
    {
        static const char* n[NumSounds] { "SOFT KEYS", "WARM PAD", "CHOIR", "STRINGS", "FLUTE", "ORGAN", "MUSIC BOX",
                                          "VIBES", "BELLS", "GLASS HARP", "WAVES", "BREATH", "MY SOUND" };
        return n[s < 0 ? 0 : (s >= NumSounds ? NumSounds - 1 : s)];
    }
    // Struck sounds start with a hit and ring out; the rest swell in and fade slowly.
    static bool isStruck (int s) { return s == MusicBox || s == Vibes || s == Bells || s == MySound; }

    struct Voice
    {
        double phase = 0.0, mphase = 0.0, cycle = 0.0;
        double ph[3] {};
        float env = 0.0f, envTarget = 0.0f;
        float gate = 0.0f;                  // fast gate for struck sounds and samples
        float a[3] {}, dec[3] {};           // partial amplitudes of a struck note, and their decay per sample
        float click = 0.0f;
        bool pendingStrike = false;
        float wobPh = 0.0f, drift = 0.0f, driftT = 0.0f;
        float vibPh = 0.0f, slowPh = 0.0f;
        float pan = 0.0f;
        int noteIdx = 0;
        Svf f[3];
        const SampleData* sd = nullptr;     // the file this voice is playing (kept alive by the processor)
        double spos = 0.0;
    } v[V];

    float sr = 44100.0f;
    Random rng { 0xD8EA3B11u };
    int lastSound = -1;
    int coefTick = 0;
    const float lengths[V] { 17.3f, 21.1f, 23.9f, 29.5f, 31.7f };
    const float offsets[V] { 0.0f, 0.37f, 0.71f, 0.19f, 0.55f };

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (int i = 0; i < V; ++i)
        {
            auto& vo = v[i];
            vo.cycle = offsets[i];
            vo.pan = -0.8f + 0.4f * (float) i;
            vo.wobPh = (float) i * 0.21f;
            vo.slowPh = (float) i * 0.37f;
            vo.vibPh = (float) i * 0.13f;
            vo.noteIdx = i;
            vo.pendingStrike = offsets[i] < 0.25f;
            vo.gate = 0.0f; vo.sd = nullptr;
            for (auto& f : vo.f) f.reset();
        }
        lastSound = -1;
    }

    // chord: semitone offsets (up to 6). mine: the file for MY SOUND (nullptr = none loaded,
    // which plays SOFT KEYS). Returns mono, adds stereo to l/r.
    float process (const int* chord, int chordSize, float baseNote, float rate, float tone, float warp,
                   int sound, const SampleData* mine, float& l, float& r)
    {
        sound = sound < 0 ? 0 : (sound >= NumSounds ? NumSounds - 1 : sound);
        if (sound == MySound && (mine == nullptr || mine->length() < 64)) sound = Keys;
        const bool struck = isStruck (sound);
        if (sound != lastSound)
        {   // Switching to a struck sound: voices in the middle of a note strike now rather than wait a whole loop.
            if (struck && lastSound >= 0)
                for (auto& vo : v) if (vo.cycle < 0.3) vo.pendingStrike = true;
            lastSound = sound;
        }
        const bool updateCoefs = (coefTick = (coefTick + 1) & 15) == 0;
        const float gateUp = 1.0f - std::exp (-1.0f / (0.004f * sr)), gateDown = 1.0f - std::exp (-1.0f / (1.2f * sr));

        float mono = 0.0f;
        for (int i = 0; i < V; ++i)
        {
            auto& vo = v[i];
            vo.cycle += rate / (lengths[i] * sr);
            if (vo.cycle >= 1.0)
            {
                vo.cycle -= 1.0;
                // Occasionally the loop picks a neighbouring chord tone, like a tape loop re-spliced.
                if (rng.next() > 0.55f) vo.noteIdx = (vo.noteIdx + (rng.next() > 0.0f ? 1 : chordSize - 1)) % imax (1, chordSize);
                vo.pendingStrike = true;
            }
            // Swell: the note sounds in the first ~40% of the cycle, slow rise, long fade.
            const float c = (float) vo.cycle;
            vo.envTarget = c < 0.08f ? 1.0f : (c < 0.42f ? 0.7f : 0.0f);
            const float coef = vo.envTarget > vo.env ? 1.0f / (sr * 1.6f) : 1.0f / (sr * 3.5f);
            vo.env += (vo.envTarget - vo.env) * coef * 4.0f;
            const float gateTarget = c < 0.42f ? 1.0f : 0.0f;
            vo.gate += (gateTarget - vo.gate) * (gateTarget > vo.gate ? gateUp : gateDown);

            // Warp: slow per-voice pitch wobble and drift (warped-cassette detune).
            vo.wobPh += (0.23f + 0.11f * (float) i) / sr; if (vo.wobPh >= 1.0f) vo.wobPh -= 1.0f;
            vo.slowPh += (0.031f + 0.017f * (float) i) / sr; if (vo.slowPh >= 1.0f) vo.slowPh -= 1.0f;
            vo.vibPh += (4.9f + 0.37f * (float) i) / sr; if (vo.vibPh >= 1.0f) vo.vibPh -= 1.0f;
            if (rng.next() > 0.9995f) vo.driftT = rng.next();
            vo.drift += (vo.driftT - vo.drift) * 0.00003f;
            const float cents = warp * (std::sin (kTwoPi * vo.wobPh) * 22.0f + vo.drift * 30.0f);

            const int octave = (i == 2 ? 24 : (i % 2 == 0 ? 12 : 0)) + (sound == MusicBox ? 12 : 0);
            const float note = baseNote + (float) chord[vo.noteIdx % imax (1, chordSize)] + (float) octave + cents * 0.01f;

            if (vo.pendingStrike)
            {
                vo.pendingStrike = false;
                if (struck) strike (vo, sound, tone, mine);
            }

            float amp;
            if (sound == MySound) amp = vo.sd != nullptr ? vo.gate : 0.0f;
            else if (struck) amp = vo.gate * (vo.a[0] + vo.a[1] + vo.a[2] + vo.click > 1.0e-4f ? 1.0f : 0.0f);
            else amp = vo.env;
            if (amp < 1.0e-4f)
            {
                if (! struck && vo.envTarget == 0.0f) vo.env = 0.0f;
                continue;
            }

            const double f = 440.0 * std::exp2 ((note - 69.0) / 12.0);
            const float x = voiceSample (vo, i, sound, f, tone, updateCoefs);
            const float out = x * amp * 0.16f * gain (sound);
            mono += out;
            l += out * (1.0f - vo.pan) * 0.5f;
            r += out * (1.0f + vo.pan) * 0.5f;
        }
        return mono;
    }

private:
    static int imax (int a, int b) { return a > b ? a : b; }
    static float sat (double ph) { return std::sin (kTwoPi * (float) ph); }
    static void adv (double& ph, double inc) { ph += inc; if (ph >= 1.0) ph -= std::floor (ph); }
    static float saw (double ph, double inc) { return (float) (2.0 * ph - 1.0) - BlepOsc::blep (ph, inc); }

    // Levels so every SOUND sits at about the same loudness as SOFT KEYS.
    static float gain (int s)
    {
        static const float g[NumSounds] { 1.0f, 2.2f, 8.0f, 2.6f, 1.1f, 1.5f, 1.8f, 1.9f, 1.6f, 1.4f, 2.3f, 4.2f, 2.8f };
        return g[s];
    }

    void strike (Voice& vo, int sound, float tone, const SampleData* mine)
    {
        float tau[3] { 1.0f, 1.0f, 1.0f };
        vo.click = 0.0f;
        switch (sound)
        {
            case MusicBox:
                vo.a[0] = 1.0f; vo.a[1] = 0.45f * (0.3f + tone); vo.a[2] = 0.22f * tone;
                tau[0] = 1.9f; tau[1] = 0.4f; tau[2] = 0.13f; vo.click = 0.25f;
                break;
            case Vibes:
                vo.a[0] = 1.0f; vo.a[1] = 0.35f * (0.4f + tone); vo.a[2] = 0.12f * tone;
                tau[0] = 3.2f; tau[1] = 0.9f; tau[2] = 0.25f; vo.click = 0.08f;
                break;
            case Bells:
                vo.a[0] = 1.0f;               // carrier
                vo.a[1] = 1.0f;               // FM index envelope
                vo.a[2] = 0.3f;               // hum an octave below
                tau[0] = 3.8f; tau[1] = 1.1f; tau[2] = 5.5f;
                break;
            case MySound:
                vo.a[0] = 1.0f; vo.a[1] = vo.a[2] = 0.0f; tau[0] = 1.0e6f;
                vo.sd = mine; vo.spos = 0.0;
                break;
            default: break;
        }
        for (int k = 0; k < 3; ++k) vo.dec[k] = std::exp (-1.0f / (tau[k] * sr));
        vo.ph[0] = vo.ph[1] = vo.ph[2] = 0.0;
        vo.phase = vo.mphase = 0.0;
    }

    float voiceSample (Voice& vo, int i, int sound, double f, float tone, bool updateCoefs)
    {
        const double inc = f / sr;
        const double nyq = 0.45 * sr;
        switch (sound)
        {
            case Keys:
            default:
            {   // Soft FM: sine carrier with a gentle 2:1 modulator, a glassy electric-piano / pad tone.
                adv (vo.phase, inc); adv (vo.mphase, 2.0 * inc);
                const float m = sat (vo.mphase) * (0.15f + tone * 1.6f) * vo.env;
                return std::sin (kTwoPi * (float) vo.phase + m);
            }
            case Pad:
            {   // Two detuned saws and a sub saw, through a gentle 4-pole low-pass that opens as it swells.
                const double d = 0.0035;
                adv (vo.ph[0], inc * (1.0 + d)); adv (vo.ph[1], inc * (1.0 - d)); adv (vo.ph[2], inc * 0.5);
                const float raw = 0.5f * (saw (vo.ph[0], inc * (1.0 + d)) + saw (vo.ph[1], inc * (1.0 - d))) + 0.35f * saw (vo.ph[2], inc * 0.5);
                if (updateCoefs)
                {
                    const float fc = (float) f * (1.5f + tone * 9.0f) * (0.55f + 0.45f * vo.env);
                    vo.f[0].set (fc, 0.62f, sr); vo.f[1].set (fc, 0.55f, sr);
                }
                return vo.f[1].lowpass (vo.f[0].lowpass (raw));
            }
            case Choir:
            {   // A vibrato saw through three vowel formants; TONE moves oo > oh > ah > eh.
                const float vib = 1.0f + 0.006f * std::sin (kTwoPi * vo.vibPh);
                adv (vo.ph[0], inc * vib); adv (vo.ph[1], inc * vib * 1.0047);
                const float src = 0.6f * saw (vo.ph[0], inc) + 0.4f * saw (vo.ph[1], inc) + 0.04f * rng.next();
                if (updateCoefs)
                {
                    float fm[3];
                    vowelFormants (clampf (tone * 0.9f + 0.12f * std::sin (kTwoPi * (vo.slowPh + 0.2f * (float) i)), 0.0f, 1.0f), fm);
                    vo.f[0].set (fm[0], 9.0f, sr); vo.f[1].set (fm[1], 11.0f, sr); vo.f[2].set (fm[2], 13.0f, sr);
                }
                float lp;
                return vo.f[0].process (src, lp) * 0.11f + vo.f[1].process (src, lp) * 0.06f + vo.f[2].process (src, lp) * 0.03f;
            }
            case Strings:
            {   // Two bowed saws, each with its own vibrato, low-passed, with a slow swell in bow pressure.
                const float v1 = 1.0f + 0.0045f * std::sin (kTwoPi * vo.vibPh);
                const float v2 = 1.0f + 0.0045f * std::sin (kTwoPi * (vo.vibPh * 1.09f + 0.3f));
                adv (vo.ph[0], inc * v1 * 1.003); adv (vo.ph[1], inc * v2 * 0.997);
                const float raw = 0.5f * (saw (vo.ph[0], inc) + saw (vo.ph[1], inc));
                if (updateCoefs)
                {
                    const float fc = (float) f * (2.2f + tone * 7.0f);
                    vo.f[0].set (fc, 0.7f, sr); vo.f[1].set (fc * 1.3f, 0.6f, sr);
                }
                const float bow = 0.85f + 0.15f * std::sin (kTwoPi * vo.slowPh * 3.0f);
                return vo.f[1].lowpass (vo.f[0].lowpass (raw)) * bow;
            }
            case Flute:
            {   // Sine with a little second harmonic, breath noise around the note, and vibrato that grows in.
                const float vib = 1.0f + 0.005f * vo.env * std::sin (kTwoPi * vo.vibPh);
                adv (vo.ph[0], inc * vib);
                if (updateCoefs) vo.f[0].set ((float) f, 5.0f, sr);
                float lp;
                const float breath = vo.f[0].process (rng.next(), lp) * (0.05f + 0.12f * tone);
                return sat (vo.ph[0]) + 0.12f * sat (2.0 * vo.ph[0]) + 0.05f * tone * sat (3.0 * vo.ph[0]) + breath;
            }
            case Organ:
            {   // Drawbars: 8', 4', 2 2/3', 2', with TONE pulling out the higher ones. Slow rotary swirl.
                adv (vo.ph[0], inc);
                static const float h[6] { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f };
                const float amps[6] { 1.0f, 0.55f, 0.35f, 0.3f * (0.3f + tone), 0.2f * tone, 0.14f * tone };
                float s = 0.0f;
                for (int k = 0; k < 6; ++k)
                    if (f * h[k] < nyq) s += amps[k] * sat (vo.ph[0] * h[k]);
                const float swirl = 1.0f + 0.12f * std::sin (kTwoPi * (vo.slowPh * 25.0f + 0.25f * (float) i));
                return s * 0.6f * swirl;
            }
            case MusicBox:
            {   // A comb tine: fundamental plus two inharmonic partials that die away fast, and a tiny click.
                static const double ratio[3] { 1.0, 4.07, 9.2 };
                float s = 0.0f;
                for (int k = 0; k < 3; ++k)
                {
                    adv (vo.ph[k], inc * ratio[k]);
                    if (f * ratio[k] < nyq) s += vo.a[k] * sat (vo.ph[k]);
                    vo.a[k] *= vo.dec[k];
                }
                s += vo.click * rng.next();
                vo.click *= 0.985f;
                return s;
            }
            case Vibes:
            {   // A tuned bar (1 : 4 : 10) with a slow motor tremolo.
                static const double ratio[3] { 1.0, 4.0, 10.0 };
                float s = 0.0f;
                for (int k = 0; k < 3; ++k)
                {
                    adv (vo.ph[k], inc * ratio[k]);
                    if (f * ratio[k] < nyq) s += vo.a[k] * sat (vo.ph[k]);
                    vo.a[k] *= vo.dec[k];
                }
                s += vo.click * rng.next();
                vo.click *= 0.97f;
                const float trem = 1.0f - 0.28f * (0.5f + 0.5f * std::sin (kTwoPi * vo.vibPh * 1.08f));
                return s * trem;
            }
            case Bells:
            {   // FM bell: an inharmonic 1 : 1.4 modulator whose brightness dies faster than the ring, plus a low hum.
                adv (vo.phase, inc); adv (vo.mphase, inc * 1.4); adv (vo.ph[2], inc * 0.5);
                const float index = (1.2f + 3.0f * tone) * vo.a[1];
                const float s = vo.a[0] * std::sin (kTwoPi * (float) vo.phase + index * sat (vo.mphase)) + vo.a[2] * sat (vo.ph[2]);
                for (int k = 0; k < 3; ++k) vo.a[k] *= vo.dec[k];
                return s;
            }
            case GlassHarp:
            {   // Rubbed glass: two sines a hair apart, beating slowly, with faint upper partials.
                adv (vo.ph[0], inc); adv (vo.ph[1], inc * 1.0011);
                return 0.55f * (sat (vo.ph[0]) + sat (vo.ph[1]))
                     + (0.06f + 0.14f * tone) * sat (2.0 * vo.ph[0]) + 0.06f * tone * sat (3.0 * vo.ph[1]);
            }
            case Waves:
            {   // Wavetables (VOWELS and GLASS) whose position wanders slowly, a different path for each loop.
                adv (vo.ph[0], inc);
                const float pos = clampf (tone + 0.28f * std::sin (kTwoPi * vo.slowPh * 2.0f), 0.0f, 1.0f);
                if (updateCoefs) vo.f[0].set (clampf ((float) f * (2.5f + 9.0f * tone), 200.0f, 16000.0f), 0.6f, sr);
                return vo.f[0].lowpass (Wavetables::get().read (i % 2 == 0 ? 1 : 2, pos, vo.ph[0], f, sr));
            }
            case Breath:
            {   // Noise through a very narrow band-pass at the note: an airy, whistling tone over a faint sine.
                adv (vo.ph[0], inc);
                const float q = 18.0f + 40.0f * (1.0f - tone);
                if (updateCoefs) vo.f[0].set ((float) f, q, sr);
                float lp;
                const float band = vo.f[0].process (rng.next(), lp);
                return band * (0.9f / std::sqrt (q)) * 0.6f + 0.25f * sat (vo.ph[0]);
            }
            case MySound:
            {
                const SampleData* s = vo.sd;
                if (s == nullptr) return 0.0f;
                const int len = s->length();
                const int i0 = (int) vo.spos;
                if (i0 >= len - 1) { vo.sd = nullptr; return 0.0f; }
                const float fr = (float) (vo.spos - (double) i0);
                const float a0 = s->left[(size_t) i0], a1 = s->left[(size_t) i0 + 1];   // mono: loadDreamSound mixes it down
                // Pitch: the file's own note moves to the chord note, folded into a usable range.
                const float root = s->rootMidi >= 0.0f ? s->rootMidi : 60.0f;
                float diff = (float) (69.0 + 12.0 * std::log2 (f / 440.0)) - root;
                while (diff > 19.0f) diff -= 12.0f;
                while (diff < -24.0f) diff += 12.0f;
                vo.spos += std::exp2 (diff / 12.0f) * s->sampleRate / (double) sr;
                // Fade the last 20 ms so a file that ends mid-note never clicks.
                const float tail = clampf ((float) (len - 1 - i0) / (0.02f * (float) s->sampleRate), 0.0f, 1.0f);
                const float x = (a0 + (a1 - a0) * fr) * tail;
                if (tone > 0.97f) return x;
                if (updateCoefs) vo.f[0].set (1500.0f * std::exp2 (tone * 4.0f), 0.6f, sr);
                return vo.f[0].lowpass (x);
            }
        }
    }
};
} // namespace dw
