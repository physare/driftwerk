// Built-in sounds for the GRAIN module, and a pitch finder for your own files.
// Every built-in sound is synthesised here (no samples) and tuned to A, using only
// the root and the fifth, so GRAIN can move it into any key and stay in tune.
#pragma once

#include "DSP.h"
#include <memory>

namespace dw
{
constexpr int numBuiltInSounds = 8;
inline const char* const builtInSoundNames[numBuiltInSounds] {
    "GLASS CHOIR", "BOWED METAL", "TAPE PIANO", "VOX PAD", "MUSIC BOX", "WIND ORGAN", "STRING DUST", "TUNED RAIN" };
constexpr float builtInRoot = 57.0f;   // A3: all built-in sounds are rooted here

namespace sounds
{
constexpr double fs = 44100.0;

inline std::unique_ptr<SampleData> blank (double seconds)
{
    auto sd = std::make_unique<SampleData>();
    sd->sampleRate = fs;
    sd->rootMidi = builtInRoot;
    const auto n = (size_t) (fs * seconds);
    sd->left.assign (n, 0.0f);
    sd->right.assign (n, 0.0f);
    return sd;
}

// Fade the ends, bring every sound to the same loudness, and round off any spiky peaks.
inline void finish (SampleData& sd, float rmsTarget = 0.15f)
{
    const int n = sd.length();
    const int fade = (int) (fs * 0.2);
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float e = std::min (1.0f, (float) std::min (i, n - 1 - i) / (float) fade);
        sd.left[(size_t) i] *= e; sd.right[(size_t) i] *= e;
        sum += (double) sd.left[(size_t) i] * sd.left[(size_t) i] + (double) sd.right[(size_t) i] * sd.right[(size_t) i];
    }
    const float rms = (float) std::sqrt (sum / std::max (1, 2 * n));
    if (rms < 1.0e-6f) return;
    const float g = rmsTarget / rms;
    auto limit = [] (float x)
    {
        const float a = std::abs (x);
        return a <= 0.6f ? x : std::copysign (0.6f + 0.3f * std::tanh ((a - 0.6f) / 0.3f), x);
    };
    for (int i = 0; i < n; ++i) { sd.left[(size_t) i] = limit (sd.left[(size_t) i] * g); sd.right[(size_t) i] = limit (sd.right[(size_t) i] * g); }
}

inline float hz (float midi) { return midiToHz (midi); }

// 0: stacked soft sines that swell at different speeds (the original GRAIN sound).
inline std::unique_ptr<SampleData> glassChoir()
{
    auto sd = blank (6.0);
    const float notes[] { 57.0f, 64.0f, 69.0f, 76.0f, 81.0f, 88.0f };
    const float amps[] { 0.12f, 0.12f, 0.12f, 0.1f, 0.08f, 0.05f };
    Random rng (0xFEEDu);
    float nlp = 0.0f;
    for (int i = 0; i < sd->length(); ++i)
    {
        const float t = (float) i / (float) fs;
        float l = 0.0f, r = 0.0f;
        for (int k = 0; k < 6; ++k)
        {
            const float f = hz (notes[k]);
            const float swell = 0.5f - 0.5f * std::cos (kTwoPi * t / (2.0f + (float) k * 0.7f));
            l += std::sin (kTwoPi * f * t) * swell * amps[k];
            r += std::sin (kTwoPi * f * 1.0015f * t + (float) k) * swell * amps[k];
        }
        nlp += (rng.next() - nlp) * 0.05f;
        sd->left[(size_t) i] = l + nlp * 0.05f;
        sd->right[(size_t) i] = r + nlp * 0.05f;
    }
    finish (*sd);
    return sd;
}

// 1: a bowed string/metal bar: rich harmonic partials that come and go, plus bow noise.
inline std::unique_ptr<SampleData> bowedMetal()
{
    auto sd = blank (8.0);
    Random rng (0xB0E5u);
    struct Part { double ph = 0, inc = 0; float amp = 0, rate = 0, off = 0; float pan = 0; };
    std::vector<Part> parts;
    for (const float root : { 45.0f, 52.0f })
        for (int n = 1; n <= (root < 50 ? 16 : 10); ++n)
        {
            Part p;
            p.inc = (double) hz (root) * n / fs;
            if (p.inc > 0.4) continue;
            p.amp = (root < 50 ? 0.22f : 0.12f) / std::pow ((float) n, 0.85f);
            p.rate = 0.07f + 0.25f * (rng.next() * 0.5f + 0.5f);
            p.off = rng.next() * kPi;
            p.pan = rng.next() * 0.7f;
            parts.push_back (p);
        }
    Svf bowL, bowR;
    bowL.set (1800.0f, 1.2f, (float) fs); bowR.set (2300.0f, 1.2f, (float) fs);
    for (int i = 0; i < sd->length(); ++i)
    {
        const float t = (float) i / (float) fs;
        float l = 0.0f, r = 0.0f;
        for (auto& p : parts)
        {
            const float a = p.amp * (0.55f + 0.45f * std::sin (kTwoPi * p.rate * t + p.off));
            const float s = std::sin (kTwoPi * (float) p.ph) * a;
            p.ph += p.inc; if (p.ph >= 1.0) p.ph -= 1.0;
            l += s * (1.0f - p.pan); r += s * (1.0f + p.pan);
        }
        const float pressure = 0.6f + 0.4f * std::sin (kTwoPi * 0.31f * t);
        float lp;
        l += bowL.process (rng.next(), lp) * 0.05f * pressure;
        r += bowR.process (rng.next(), lp) * 0.05f * pressure;
        sd->left[(size_t) i] = l; sd->right[(size_t) i] = r;
    }
    finish (*sd);
    return sd;
}

// 2: a soft, worn piano: struck notes on A and E with decaying partials and a little wow.
inline std::unique_ptr<SampleData> tapePiano()
{
    auto sd = blank (8.0);
    struct Hit { float t, note, vel, pan; };
    const Hit hits[] { { 0.05f, 57, 1.0f, -0.2f }, { 0.9f, 64, 0.7f, 0.3f }, { 1.8f, 69, 0.8f, 0.0f }, { 2.9f, 76, 0.55f, 0.4f },
                       { 3.6f, 64, 0.6f, -0.4f }, { 4.4f, 57, 0.9f, 0.1f }, { 4.45f, 64, 0.5f, -0.1f }, { 5.6f, 81, 0.45f, 0.5f },
                       { 6.3f, 69, 0.65f, -0.3f }, { 7.0f, 76, 0.4f, 0.2f } };
    Random rng (0x91A0u);
    for (const auto& h : hits)
    {
        const int start = (int) (h.t * fs);
        const float f0 = hz (h.note);
        for (int n = 1; n <= 9; ++n)
        {
            const float f = f0 * (float) n;
            if (f > 9000.0f) break;
            const float amp = h.vel * 0.25f / std::pow ((float) n, 1.4f);
            const float decay = 0.6f + 0.55f * (float) n;
            double ph = rng.next();
            for (int i = start; i < sd->length(); ++i)
            {
                const float t = (float) (i - start) / (float) fs;
                const float env = std::exp (-t * decay) * std::min (1.0f, t * 300.0f);
                if (env < 1.0e-4f && t > 0.1f) break;
                const float wow = 1.0f + 0.0025f * std::sin (kTwoPi * 0.55f * (float) i / (float) fs);
                ph += f * wow / fs; if (ph >= 1.0) ph -= 1.0;
                const float s = std::sin (kTwoPi * (float) ph) * amp * env;
                sd->left[(size_t) i] += s * (1.0f - h.pan); sd->right[(size_t) i] += s * (1.0f + h.pan);
            }
        }
    }
    // Tape: gentle hiss and a soft top.
    float al = 0, ar = 0;
    for (int i = 0; i < sd->length(); ++i)
    {
        al += (sd->left[(size_t) i] - al) * 0.35f; ar += (sd->right[(size_t) i] - ar) * 0.35f;
        sd->left[(size_t) i] = al + rng.next() * 0.004f; sd->right[(size_t) i] = ar + rng.next() * 0.004f;
    }
    finish (*sd);
    return sd;
}

// Band-limited saw voices through moving vowel formants.
inline void voxVoices (SampleData& sd, const float* notes, int count, float vibCents, float vowelRate, uint32_t seed)
{
    Random rng (seed);
    std::vector<BlepOsc> osc ((size_t) count * 2);
    std::vector<float> vibPh ((size_t) count * 2), vibRate ((size_t) count * 2);
    for (size_t k = 0; k < osc.size(); ++k) { osc[k].phase = rng.next() * 0.5 + 0.5; vibPh[k] = rng.next(); vibRate[k] = 4.6f + rng.next() * 0.8f; }
    Svf fL[3], fR[3];
    const float gains[3] { 1.0f, 0.55f, 0.22f };
    float form[3];
    for (int i = 0; i < sd.length(); ++i)
    {
        const float t = (float) i / (float) fs;
        if ((i & 31) == 0)
        {
            vowelFormants (0.5f - 0.5f * std::cos (kTwoPi * vowelRate * t), form);
            for (int k = 0; k < 3; ++k) { fL[k].set (form[k], 9.0f, (float) fs); fR[k].set (form[k] * 1.03f, 9.0f, (float) fs); }
        }
        float l = 0.0f, r = 0.0f;
        for (int k = 0; k < count * 2; ++k)
        {
            vibPh[(size_t) k] += vibRate[(size_t) k] / (float) fs; if (vibPh[(size_t) k] >= 1.0f) vibPh[(size_t) k] -= 1.0f;
            const float cents = vibCents * std::sin (kTwoPi * vibPh[(size_t) k]) + (k % 2 ? 6.0f : -6.0f);
            const float s = osc[(size_t) k].process (BlepOsc::Saw, hz (notes[k / 2] + cents * 0.01f), fs);
            (k % 2 ? r : l) += s;
        }
        float lo = 0.0f, ro = 0.0f, lp;
        for (int k = 0; k < 3; ++k) { lo += fL[k].process (l, lp) * gains[k]; ro += fR[k].process (r, lp) * gains[k]; }
        sd.left[(size_t) i] = lo + l * 0.02f; sd.right[(size_t) i] = ro + r * 0.02f;
    }
}

// 3: a slow "aah / ooh" choir on A, E and A.
inline std::unique_ptr<SampleData> voxPad()
{
    auto sd = blank (8.0);
    const float notes[] { 57.0f, 64.0f, 69.0f };
    voxVoices (*sd, notes, 3, 12.0f, 0.12f, 0x70C5u);
    finish (*sd);
    return sd;
}

// 4: a music box: small bright plinks of A and E scattered in time.
inline std::unique_ptr<SampleData> musicBox()
{
    auto sd = blank (8.0);
    Random rng (0x3B0Cu);
    const float notes[] { 69.0f, 76.0f, 81.0f, 88.0f, 93.0f };
    for (float t0 = 0.05f; t0 < 7.6f; t0 += 0.22f + 0.18f * (rng.next() * 0.5f + 0.5f))
    {
        const float note = notes[(int) ((rng.next() * 0.5f + 0.5f) * 4.999f)];
        const float pan = rng.next() * 0.8f, vel = 0.5f + 0.5f * (rng.next() * 0.5f + 0.5f);
        const int start = (int) (t0 * fs);
        const float f = hz (note);
        for (int i = start; i < sd->length(); ++i)
        {
            const float t = (float) (i - start) / (float) fs;
            const float env = std::exp (-t * 2.6f) * std::min (1.0f, t * 900.0f);
            if (env < 1.0e-4f && t > 0.01f) break;
            const float s = (std::sin (kTwoPi * f * t) + 0.3f * std::exp (-t * 6.0f) * std::sin (kTwoPi * 3.0f * f * t)
                             + 0.12f * std::exp (-t * 9.0f) * std::sin (kTwoPi * 4.0f * f * t)) * 0.12f * vel * env;
            sd->left[(size_t) i] += s * (1.0f - pan); sd->right[(size_t) i] += s * (1.0f + pan);
        }
    }
    finish (*sd);
    return sd;
}

// 5: a breathy pipe organ: drawbars on A, E, A with wind noise and a chiff on each swell.
inline std::unique_ptr<SampleData> windOrgan()
{
    auto sd = blank (8.0);
    Random rng (0x0A6Eu);
    const float notes[] { 45.0f, 57.0f, 64.0f, 69.0f };
    const float bars[] { 1.0f, 0.75f, 0.4f, 0.35f, 0.0f, 0.15f, 0.0f, 0.12f };   // harmonics 1..8
    double ph[4] {};
    Svf wind, chiff;
    wind.set (1400.0f, 1.5f, (float) fs); chiff.set (3200.0f, 3.0f, (float) fs);
    for (int i = 0; i < sd->length(); ++i)
    {
        const float t = (float) i / (float) fs;
        const float swell = 0.55f + 0.45f * std::sin (kTwoPi * t / 4.0f - kPi * 0.5f);
        const float trem = 1.0f + 0.06f * std::sin (kTwoPi * 5.6f * t);
        float s = 0.0f;
        for (int k = 0; k < 4; ++k)
        {
            ph[k] += hz (notes[k]) / fs; if (ph[k] >= 1.0) ph[k] -= 1.0;
            for (int n = 0; n < 8; ++n)
                if (bars[n] > 0.0f) s += std::sin (kTwoPi * (float) ph[k] * (float) (n + 1)) * bars[n] * (k == 0 ? 0.6f : 0.35f);
        }
        const float cyc = std::fmod (t, 4.0f);
        float lp;
        const float ch = chiff.process (rng.next(), lp) * 0.25f * std::exp (-cyc * 7.0f);
        const float w = wind.process (rng.next(), lp) * 0.06f * swell;
        const float out = s * 0.1f * swell * trem + w + ch;
        sd->left[(size_t) i] = out * (1.0f + 0.15f * std::sin (kTwoPi * 0.7f * t));
        sd->right[(size_t) i] = out * (1.0f - 0.15f * std::sin (kTwoPi * 0.7f * t));
    }
    finish (*sd);
    return sd;
}

// 6: an old string machine through a dusty record: detuned saws, soft top, crackle.
inline std::unique_ptr<SampleData> stringDust()
{
    auto sd = blank (8.0);
    Random rng (0xD057u);
    const float notes[] { 45.0f, 52.0f, 57.0f, 64.0f };
    BlepOsc osc[8];
    for (auto& o : osc) o.phase = rng.next() * 0.5 + 0.5;
    Svf lpL, lpR, crackle;
    lpL.set (1700.0f, 0.7f, (float) fs); lpR.set (1900.0f, 0.7f, (float) fs);
    crackle.set (2500.0f, 2.0f, (float) fs);
    float pop = 0.0f;
    for (int i = 0; i < sd->length(); ++i)
    {
        const float t = (float) i / (float) fs;
        float l = 0.0f, r = 0.0f;
        for (int k = 0; k < 8; ++k)
        {
            const float cents = (k % 2 ? 9.0f : -9.0f) + 4.0f * std::sin (kTwoPi * (0.21f + 0.05f * (float) k) * t);
            const float s = osc[k].process (BlepOsc::Saw, hz (notes[k / 2] + cents * 0.01f), fs) * (k < 2 ? 0.3f : 0.2f);
            (k % 2 ? r : l) += s;
        }
        if (rng.next() > 0.9993f) pop = rng.next();
        pop *= 0.93f;
        float lp;
        const float dust = crackle.process (pop, lp) * 1.2f + rng.next() * 0.003f;
        const float swell = 0.7f + 0.3f * std::sin (kTwoPi * t / 8.0f - kPi * 0.5f);
        sd->left[(size_t) i] = lpL.lowpass (l) * swell + dust;
        sd->right[(size_t) i] = lpR.lowpass (r) * swell + dust * 0.8f;
    }
    finish (*sd);
    return sd;
}

// 7: rain drops ringing through resonators tuned to A and E.
inline std::unique_ptr<SampleData> tunedRain()
{
    auto sd = blank (8.0);
    Random rng (0x7A1Bu);
    struct Comb
    {
        std::vector<float> buf; int w = 0; double delay = 100; float fb = 0.97f, damp = 0.0f;
        float process (float x)
        {
            const int n = (int) buf.size();
            double rp = (double) w - delay; while (rp < 0) rp += n;
            int i0 = (int) rp; const float fr = (float) (rp - i0);
            if (i0 >= n) i0 -= n;
            const float y = buf[(size_t) i0] + (buf[(size_t) ((i0 + 1) % n)] - buf[(size_t) i0]) * fr;
            damp += (y - damp) * 0.7f;
            buf[(size_t) w] = x + damp * fb;
            w = (w + 1) % n;
            return y;
        }
    };
    const float notes[] { 45.0f, 52.0f, 57.0f, 64.0f, 69.0f, 76.0f };
    std::vector<Comb> combs (6);
    for (size_t k = 0; k < combs.size(); ++k)
    {
        // The damping filter in the loop adds a little delay; compensate so the pitch is exact.
        combs[k].delay = fs / hz (notes[k]) - 0.43;
        combs[k].buf.assign ((size_t) (fs / hz (notes[k])) + 4, 0.0f);
        combs[k].fb = 0.985f - 0.004f * (float) k;
    }
    float drop = 0.0f;
    for (int i = 0; i < sd->length(); ++i)
    {
        const float t = (float) i / (float) fs;
        const float rate = 0.9994f - 0.0004f * (0.5f + 0.5f * std::sin (kTwoPi * t / 8.0f));
        if (rng.next() > rate) drop = rng.next();
        drop *= 0.6f;
        const float x = drop * 0.3f + rng.next() * 0.0015f;
        float l = 0.0f, r = 0.0f;
        for (size_t k = 0; k < combs.size(); ++k) (k % 2 ? r : l) += combs[k].process (x) * 0.25f;
        sd->left[(size_t) i] = l + r * 0.3f; sd->right[(size_t) i] = r + l * 0.3f;
    }
    finish (*sd);
    return sd;
}

} // namespace sounds

inline std::unique_ptr<SampleData> makeBuiltInSound (int index)
{
    switch (index)
    {
        case 1: return sounds::bowedMetal();
        case 2: return sounds::tapePiano();
        case 3: return sounds::voxPad();
        case 4: return sounds::musicBox();
        case 5: return sounds::windOrgan();
        case 6: return sounds::stringDust();
        case 7: return sounds::tunedRain();
        default: return sounds::glassChoir();
    }
}

//==============================================================================
// Finds the main pitch of a sound: a pitch-class profile from the spectrum of a few
// loud moments, scored as root + fifth (so a note's own overtones point at its root),
// plus how far the sound sits from concert tuning. Returns a MIDI note with cents,
// or -1 when nothing stands out (noise, drums, speech).
namespace pitchfind
{
inline int clampOct (int o) { return o < 0 ? 0 : (o > 9 ? 9 : o); }
inline void fft (std::vector<float>& re, std::vector<float>& im)
{
    const size_t n = re.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * 3.14159265358979323846 / (double) len;
        const float wr = (float) std::cos (ang), wi = (float) std::sin (ang);
        for (size_t i = 0; i < n; i += len)
        {
            float cr = 1.0f, ci = 0.0f;
            for (size_t k = 0; k < len / 2; ++k)
            {
                const size_t a = i + k, b = i + k + len / 2;
                const float tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr; im[a] += ti;
                const float nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}
} // namespace pitchfind

inline float detectRootMidi (const SampleData& sd)
{
    const int N = 8192;
    const int n = sd.length();
    if (n < N || sd.sampleRate <= 0.0) return -1.0f;

    double chroma[12] {}, octaveEnergy[12][10] {};
    double tuneRe = 0.0, tuneIm = 0.0;
    std::vector<float> re ((size_t) N), im ((size_t) N), mag ((size_t) N / 2);
    const int windows = std::min (16, std::max (1, n / (N / 2)));
    for (int w = 0; w < windows; ++w)
    {
        const int start = windows == 1 ? 0 : (int) ((long long) (n - N) * w / (windows - 1));
        double energy = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.5f * (sd.left[(size_t) (start + i)] + sd.right[(size_t) (start + i)]);
            const float hann = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) N);
            re[(size_t) i] = x * hann; im[(size_t) i] = 0.0f;
            energy += (double) x * x;
        }
        if (energy / N < 1.0e-5) continue;   // silence
        pitchfind::fft (re, im);
        for (int k = 0; k < N / 2; ++k) mag[(size_t) k] = std::sqrt (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]);
        float maxMag = 0.0f;
        for (float m : mag) maxMag = std::max (maxMag, m);
        // Spectral peaks between 50 Hz and 5 kHz.
        for (int k = 2; k < N / 2 - 1; ++k)
        {
            const float m = mag[(size_t) k];
            if (m < maxMag * 0.02f || m < mag[(size_t) k - 1] || m < mag[(size_t) k + 1]) continue;
            const float a = std::log (mag[(size_t) k - 1] + 1.0e-9f), b = std::log (m + 1.0e-9f), c = std::log (mag[(size_t) k + 1] + 1.0e-9f);
            const float den = a - 2.0f * b + c;
            const float bin = (float) k + (std::abs (den) > 1.0e-9f ? 0.5f * (a - c) / den : 0.0f);
            const float f = bin * (float) sd.sampleRate / (float) N;
            if (f < 50.0f || f > 5000.0f) continue;
            const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
            const int note = (int) std::lround (midi);
            const double wgt = (double) m * m;
            const int pc = ((note % 12) + 12) % 12;
            chroma[pc] += wgt;
            octaveEnergy[pc][pitchfind::clampOct (note / 12)] += wgt;
            const double ang = 2.0 * 3.14159265358979323846 * (midi - (float) note);
            tuneRe += wgt * std::cos (ang); tuneIm += wgt * std::sin (ang);
        }
    }
    double total = 0.0;
    for (double c : chroma) total += c;
    if (total <= 0.0) return -1.0f;
    int best = 0; double bestScore = -1.0;
    for (int pc = 0; pc < 12; ++pc)
    {
        const double score = chroma[pc] + 0.5 * chroma[(pc + 7) % 12];
        if (score > bestScore) { bestScore = score; best = pc; }
    }
    if (chroma[best] / total < 0.14) return -1.0f;   // nothing stands out
    int oct = 4;
    double octBest = -1.0;
    for (int o = 0; o < 10; ++o) if (octaveEnergy[best][o] > octBest) { octBest = octaveEnergy[best][o]; oct = o; }
    const float cents = (float) (std::atan2 (tuneIm, tuneRe) / (2.0 * 3.14159265358979323846));   // -0.5..0.5 semitone
    return (float) (oct * 12 + best) + cents;
}

} // namespace dw
