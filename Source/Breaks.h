// BREAK: a drum loop cut into sixteenth-note slices and played back on Timewind's beat:
// in order, chopped into new orders, rolled, reversed, re-pitched and dusted.
// Your own loop, or SYNTH BREAK (an original break played by the drum synth, made at start-up).
#pragma once

#include "DSP.h"
#include "Weird.h"
#include <memory>
#include <string>
#include <vector>

namespace dw
{
struct BreakLoop
{
    const SampleData* audio = nullptr;       // owned by the processor, never freed while it lives
    double bpm = 140.0;
    int bars = 2, steps = 32;                // steps = sixteenths in the loop
    std::vector<int> slice;                  // steps + 1 start positions (samples); the last is the end
    std::vector<float> peaks;                // for the editor
    std::string name;
    bool builtIn = false;
    int barsAsked = 0;                       // what BARS asked for (0 = AUTO)

    static constexpr int NumPeaks = 512;

    // barsWanted: 0 = work it out from the length (a tempo between 70 and 190 BPM).
    void analyse (int barsWanted)
    {
        const int len = audio != nullptr ? audio->length() : 0;
        if (len < 64) { steps = 0; return; }
        const double secs = (double) len / audio->sampleRate;
        if (barsWanted <= 0)
        {
            int best = 1; double bestErr = 1.0e9;
            for (int n : { 1, 2, 4, 8 })
            {
                const double t = 240.0 * n / secs;
                const double err = (t < 70.0 || t > 190.0 ? 1000.0 : 0.0) + std::abs (std::log2 (t / 135.0));
                if (err < bestErr) { bestErr = err; best = n; }
            }
            barsWanted = best;
        }
        bars = barsWanted;
        steps = 16 * bars;
        bpm = 240.0 * bars / secs;

        // Onset strength: rises in high-passed energy over 5 ms frames.
        const int hop = std::max (16, (int) (audio->sampleRate * 0.005));
        const int frames = len / hop;
        std::vector<float> energy ((size_t) frames + 1, 0.0f), flux ((size_t) frames + 1, 0.0f);
        float prev = 0.0f;
        for (int f = 0; f < frames; ++f)
        {
            double e = 0.0;
            for (int i = f * hop; i < (f + 1) * hop && i < len; ++i)
            {
                const float x = 0.5f * (audio->left[(size_t) i] + audio->right[(size_t) i]);
                const float hp = x - prev; prev = x;
                e += (double) (x * x) + 4.0 * (double) (hp * hp);
            }
            energy[(size_t) f] = (float) std::sqrt (e / hop);
        }
        float maxFlux = 0.0f;
        for (int f = 1; f < frames; ++f)
        {
            flux[(size_t) f] = std::max (0.0f, energy[(size_t) f] - energy[(size_t) f - 1]);
            maxFlux = std::max (maxFlux, flux[(size_t) f]);
        }
        // Slices on the grid, each moved to the strongest hit near it (within a third of a step).
        slice.assign ((size_t) steps + 1, 0);
        const double stepLen = (double) len / steps;
        for (int k = 0; k < steps; ++k)
        {
            const double g = k * stepLen;
            int pos = (int) g;
            if (k > 0 && maxFlux > 0.0f)
            {
                const int f0 = std::max (1, (int) ((g - stepLen * 0.33) / hop)), f1 = std::min (frames - 1, (int) ((g + stepLen * 0.33) / hop));
                int bestF = -1; float bestV = maxFlux * 0.12f;
                for (int f = f0; f <= f1; ++f) if (flux[(size_t) f] > bestV) { bestV = flux[(size_t) f]; bestF = f; }
                if (bestF >= 0) pos = std::max (0, (bestF - 1) * hop);
            }
            slice[(size_t) k] = std::max (k > 0 ? slice[(size_t) k - 1] + hop : 0, pos);
        }
        slice[(size_t) steps] = len;
        for (int k = steps - 1; k > 0; --k) slice[(size_t) k] = std::min (slice[(size_t) k], slice[(size_t) k + 1] - hop);

        peaks.assign (NumPeaks, 0.0f);
        for (int i = 0; i < len; ++i)
        {
            auto& pk = peaks[(size_t) ((long long) i * NumPeaks / len)];
            pk = std::max (pk, std::max (std::abs (audio->left[(size_t) i]), std::abs (audio->right[(size_t) i])));
        }
    }
};

// SYNTH BREAK: an original two-bar funk break (kick, snare with ghost notes, hats) played by
// the DUSTY kit at 140 BPM, with a little room, so it slices and chops like a sampled one.
inline std::unique_ptr<SampleData> makeSynthBreak (double sampleRate = 44100.0)
{
    const double bpm = 140.0, step = sampleRate * 60.0 / bpm / 4.0;
    const int steps = 32, len = (int) (step * steps);
    Pulse drums;
    drums.prepare ((float) sampleRate);
    drums.kit = 2; drums.punch = 0.45f;
    // lane, step, velocity, variant
    struct N { int lane, step; float vel; int variant; };
    static const N notes[] {
        { 0, 0, 1.0f, 0 },  { 0, 2, 0.75f, 1 }, { 1, 4, 0.95f, 0 }, { 1, 7, 0.3f, 1 }, { 1, 9, 0.35f, 1 }, { 0, 10, 0.9f, 0 },
        { 1, 12, 0.95f, 0 }, { 1, 15, 0.3f, 1 },
        { 0, 16, 1.0f, 0 }, { 0, 18, 0.7f, 1 }, { 1, 20, 0.95f, 0 }, { 1, 23, 0.32f, 1 }, { 0, 25, 0.8f, 1 }, { 0, 26, 0.85f, 0 },
        { 1, 28, 0.95f, 0 }, { 1, 30, 0.4f, 1 }, { 1, 31, 0.5f, 1 } };
    const float kickHz = 52.0f, snareHz = 196.0f;
    for (const auto& n : notes)
        drums.schedule (n.lane, n.vel, n.lane == 0 ? kickHz : snareHz, n.lane == 0 ? 0.0f : -0.1f, (int) (n.step * step) + 1, n.variant);
    for (int s = 0; s < steps; s += 2)   // hats on the eighths, the offbeat a little louder, one open-ish
        drums.schedule (3, (s % 4 == 2 ? 0.7f : 0.5f), 6200.0f, 0.2f, (int) (s * step) + 1, s == 14 || s == 30 ? 1 : 0);
    for (int s = 1; s < steps; s += 4)   // a quiet shaker in between
        drums.schedule (4, 0.25f, 5200.0f, 0.35f, (int) (s * step) + 1, 0);

    auto sd = std::make_unique<SampleData>();
    sd->sampleRate = sampleRate;
    sd->left.resize ((size_t) len); sd->right.resize ((size_t) len);
    // A small room: two short, damped echoes per side.
    const int d1 = (int) (0.013 * sampleRate), d2 = (int) (0.021 * sampleRate);
    float peak = 0.0f;
    for (int i = 0; i < len; ++i)
    {
        float l = 0.0f, r = 0.0f;
        drums.process (0.55f, l, r);
        sd->left[(size_t) i] = l; sd->right[(size_t) i] = r;
    }
    for (int i = len - 1; i >= d2; --i)
    {
        sd->left[(size_t) i]  += 0.22f * sd->right[(size_t) (i - d1)] + 0.12f * sd->left[(size_t) (i - d2)];
        sd->right[(size_t) i] += 0.22f * sd->left[(size_t) (i - d2)] + 0.12f * sd->right[(size_t) (i - d1)];
    }
    for (int i = 0; i < len; ++i) peak = std::max ({ peak, std::abs (sd->left[(size_t) i]), std::abs (sd->right[(size_t) i]) });
    if (peak > 1.0e-4f)
        for (int i = 0; i < len; ++i) { sd->left[(size_t) i] *= 0.9f / peak; sd->right[(size_t) i] *= 0.9f / peak; }
    return sd;
}

// Plays slices on the beat. The processor calls step() once per sixteenth and process() per sample.
struct BreakPlayer
{
    struct Voice
    {
        bool on = false;
        const SampleData* a = nullptr;   // the loop's audio (a new loop may arrive while this one fades)
        double pos = 0.0, inc = 1.0;
        int lo = 0, hi = 0;          // the slice it plays (samples)
        float gain = 1.0f, fade = 1.0f, fadeStep = 0.0f;
    };
    Voice v[3];
    int cur = -1;                    // the voice playing now
    const BreakLoop* loop = nullptr;
    float sr = 44100.0f;
    Random rng { 0xB8EA4u };

    // Chop table: for each beat of a two-bar phrase, which beat of the loop plays, and how.
    enum Mode { Straight = 0, Backwards, Stab, Half };
    struct Beat { int src = -1; int mode = Straight; };
    Beat table[8];
    int lastSlice = -9, lastDir = 1;
    double lastRate = 0.0;
    // A roll in progress: the same slice again every 'rollEvery' samples.
    int rollLeft = 0, rollEvery = 0, rollCount = 0, rollSlice = 0;
    double rollRate = 1.0; float rollSlide = 0.0f, rollGain = 1.0f;
    double rollClock = 0.0;
    // DUST
    float crushPh = 0.0f, heldL = 0.0f, heldR = 0.0f;
    // What the editor shows
    int sliceNow = -1;
    float posNow = 0.0f;
    int hits = 0;

    void prepare (float sampleRate) { sr = sampleRate; for (auto& x : v) x.on = false; cur = -1; rollLeft = 0; }
    // A roll counts as busy even between its hits: process() is what moves it on.
    bool busy() const { if (rollLeft > 0) return true; for (auto& x : v) if (x.on) return true; return false; }

    // k: the sixteenth (absolute); stepSamples: its length now; rate: playback speed.
    // inSync: the rate makes one slice last exactly one step (FOLLOW TEMPO, no PITCH), so slices
    // that follow each other can simply run on.
    void step (const BreakLoop* lp, long long k, double stepSamples, double rate, bool inSync, float chop, float stutter, float reverse)
    {
        loop = lp;
        if (lp == nullptr || lp->steps <= 0) return;
        const int steps = lp->steps;
        const int inPhrase = (int) (k % 32), beat = inPhrase / 4, w = inPhrase % 4;
        if (inPhrase == 0 && (chop <= 0.0f || uni (rng) < 0.35f + 0.5f * chop))
        {   // a new chop table for the next two bars (sometimes the old one stays, so grooves repeat)
            const int loopBeats = std::max (1, steps / 4);
            for (int b = 0; b < 8; ++b)
            {
                table[b] = {};
                if (chop <= 0.0f || uni (rng) >= chop * 0.8f) continue;
                table[b].src = (int) (uni (rng) * (float) loopBeats) % loopBeats;
                const float m = uni (rng);
                table[b].mode = m < reverse * 0.8f ? Backwards : (m < reverse * 0.8f + chop * 0.25f ? Stab : (m < reverse * 0.8f + chop * 0.4f ? Half : Straight));
            }
        }
        const auto& t = table[beat];
        int sl = (int) (k % steps);
        int dir = 1;
        if (t.src >= 0)
        {
            switch (t.mode)
            {
                case Backwards: sl = t.src * 4 + (3 - w); dir = -1; break;
                case Stab:      sl = t.src * 4 + (w % 2 == 0 ? 0 : 1); break;          // the first eighth, twice
                case Half:      sl = t.src * 4 + w / 2; break;                         // half speed through the beat
                default:        sl = t.src * 4 + w; break;
            }
            sl %= steps;
        }
        // A single reversed slice now and then, even without the chop table.
        if (dir > 0 && reverse > 0.0f && uni (rng) < reverse * 0.12f) dir = -1;

        // STUTTER: a roll of this slice, faster and faster or sliding in pitch.
        if (rollLeft <= 0 && stutter > 0.0f && uni (rng) < stutter * 0.22f)
        {
            static const int divs[] { 2, 3, 4, 6, 8 };
            const int div = divs[std::min (4, (int) (uni (rng) * (2.0f + 3.0f * stutter)))];
            rollEvery = std::max (64, (int) (stepSamples / div));
            rollLeft = std::min (div * 4, 2 + (int) (uni (rng) * (float) div * (1.0f + 2.0f * stutter)));
            rollCount = 0; rollSlice = sl; rollRate = rate; rollClock = 0.0;
            rollSlide = (uni (rng) < 0.5f ? 1.0f : -1.0f) * (uni (rng) < 0.5f ? 1.0f : 0.0f);
            rollGain = 1.0f;
            trigger (sl, rate, dir, 0.9f);
            return;
        }
        if (rollLeft > 0) return;   // the roll carries on through this step

        // Straight through the loop in time: let the slice run on (no seam at all). It may run a
        // slice ahead, since the slices are moved onto the hits and are not all the same length.
        const bool continues = inSync && cur >= 0 && v[cur].on && v[cur].fadeStep >= 0.0f && v[cur].a == lp->audio
                               && dir > 0 && lastDir > 0 && sl == (lastSlice + 1) % steps && sl != 0;
        if (continues)
        {
            v[cur].hi = lp->slice[(size_t) std::min (steps, sl + 2)];
            v[cur].inc = lp->audio->sampleRate / (double) sr * rate;   // follows small tempo changes
            lastSlice = sl; lastRate = rate;
            sliceNow = sl;
            return;
        }
        trigger (sl, rate, dir, 1.0f);
        if (inSync && dir > 0 && cur >= 0) v[cur].hi = lp->slice[(size_t) std::min (steps, sl + 2)];
    }

    void trigger (int sl, double rate, int dir, float gain)
    {
        if (loop == nullptr || loop->audio == nullptr || sl < 0 || sl >= loop->steps) return;
        if (cur >= 0 && v[cur].on) { v[cur].fadeStep = -1.0f / (0.003f * sr); }   // the old slice fades in 3 ms
        int n = 0;
        while (n < 3 && v[n].on && n != cur) ++n;
        if (n >= 3) n = (cur + 1) % 3;
        auto& x = v[n];
        x.on = true;
        x.a = loop->audio;
        x.lo = loop->slice[(size_t) sl];
        x.hi = loop->slice[(size_t) sl + 1];
        x.inc = loop->audio->sampleRate / (double) sr * rate * (dir < 0 ? -1.0 : 1.0);
        x.pos = dir < 0 ? (double) (x.hi - 1) : (double) x.lo;
        x.gain = gain;
        x.fade = 0.0f; x.fadeStep = 1.0f / (0.0007f * sr);
        cur = n;
        lastSlice = sl; lastDir = dir; lastRate = rate;
        sliceNow = sl;
        ++hits;
    }

    void process (float dust, float& l, float& r)
    {
        if (rollLeft > 0 && loop != nullptr)
        {
            rollClock += 1.0;
            if (rollClock >= rollEvery)
            {
                rollClock -= rollEvery;
                ++rollCount; --rollLeft;
                const double rr = rollRate * std::exp2 (rollSlide * (double) rollCount / 12.0);
                rollGain *= 0.93f;
                if (rollLeft > 0) trigger (rollSlice, rr, 1, rollGain);
            }
        }
        float ol = 0.0f, orr = 0.0f;
        for (auto& x : v)
        {
            if (! x.on) continue;
            const int i0 = (int) x.pos;
            const bool out = x.inc > 0.0 ? i0 >= x.hi - 1 : i0 < x.lo;
            if (out && x.fadeStep >= 0.0f) x.fadeStep = -1.0f / (0.002f * sr);   // the slice ran out: a tiny fade
            x.fade += x.fadeStep;
            if (x.fade >= 1.0f) { x.fade = 1.0f; x.fadeStep = 0.0f; }
            if (x.fade <= 0.0f && x.fadeStep < 0.0f) { x.on = false; continue; }
            const auto& a = *x.a;
            const int j = std::max (0, std::min (a.length() - 2, i0));
            const float fr = (float) (x.pos - std::floor (x.pos));
            const float g = x.gain * x.fade;
            ol += (a.left[(size_t) j] + (a.left[(size_t) j + 1] - a.left[(size_t) j]) * fr) * g;
            orr += (a.right[(size_t) j] + (a.right[(size_t) j + 1] - a.right[(size_t) j]) * fr) * g;
            x.pos += x.inc;   // keeps moving through its fade, so the end never freezes into a click
        }
        if (cur >= 0 && v[cur].a != nullptr) posNow = (float) (v[cur].pos / std::max (1, v[cur].a->length()));
        // DUST: fewer samples and fewer bits, like an old sampler.
        if (dust > 0.0f)
        {
            crushPh += (44100.0f - 32000.0f * dust) / sr;
            if (crushPh >= 1.0f)
            {
                crushPh -= 1.0f;
                const float q = std::exp2 (12.0f - 6.0f * dust);
                heldL = std::round (ol * q) / q; heldR = std::round (orr * q) / q;
            }
            const float m = std::min (1.0f, dust * 1.5f);
            ol += (heldL - ol) * m; orr += (heldR - orr) * m;
        }
        l += ol; r += orr;
    }
};
} // namespace dw
