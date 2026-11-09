// BASS: a monophonic bass voice (the arp's waves: sine, triangle, saw, square or a wavetable,
// with SHAPE; a sub, drive and a resonant low-pass with its own envelope). The processor decides
// the notes on the arp's clock.
#pragma once

#include "DSP.h"
#include "Waves.h"

namespace dw
{
struct BassVoice
{
    float sr = 44100.0f;
    WaveOsc osc;
    double subPh = 0.0;
    float pitch = 36.0f, target = 36.0f;    // MIDI notes: now, and where a slide is going
    float amp = 0.0f, fenv = 0.0f, accent = 0.0f;
    bool gate = false, sliding = false, attacking = false;
    int gateLeft = 0;
    Svf f1, f2;
    int coefTick = 0;

    void prepare (float sampleRate) { sr = sampleRate; amp = 0.0f; gate = false; f1.reset(); f2.reset(); }
    bool busy() const { return gate || amp > 1.0e-4f; }

    // slide: glide into the note without starting the envelopes again (a held, legato note).
    void noteOn (float midi, int gateSamples, bool slide, float acc)
    {
        target = midi;
        sliding = slide && gate;
        if (! sliding) { pitch = midi; fenv = 1.0f; attacking = true; coefTick = 0; }
        gate = true;
        gateLeft = std::max (16, gateSamples);
        accent = acc;
    }

    // wave: WaveOsc::Sine .. WaveOsc::Table; bank: the wavetable; shape: bends the wave (moves through the table).
    float process (int wave, int bank, float shape, float sub, float cutoff, float reso, float envAmt, float decay, float drive, float glide)
    {
        if (gate && --gateLeft <= 0) gate = false;
        // Envelopes: a quick attack, a decay to a sustain while the gate is held, a short release.
        const float sustain = 0.55f;
        if (attacking && (amp > 0.97f || ! gate)) attacking = false;
        const float aTarget = gate ? (attacking ? 1.0f : sustain) : 0.0f;
        const float tc = gate ? (attacking ? 0.002f : decay) : 0.035f;
        amp += (aTarget - amp) * (1.0f - std::exp (-1.0f / (tc * sr)));
        fenv *= std::exp (-1.0f / (std::max (0.02f, decay * 0.6f) * sr));
        if (sliding || glide > 0.0f)
        {
            const float gt = sliding ? 0.04f + glide * 0.25f : glide * 0.12f;
            pitch += (target - pitch) * (gt <= 0.0f ? 1.0f : 1.0f - std::exp (-1.0f / (gt * sr)));
        }
        else pitch = target;

        const double f = 440.0 * std::exp2 ((pitch - 69.0f) / 12.0f);
        subPh += f / sr * 0.5; if (subPh >= 1.0) subPh -= 1.0;
        const int wv = wave < WaveOsc::Sine ? WaveOsc::Sine : (wave > WaveOsc::Table ? WaveOsc::Table : wave);
        float x = osc.process (wv, f, sr, shape, 0.0f, bank);
        x += sub * std::sin (kTwoPi * (float) subPh);
        if (drive > 0.0f) x = softClip (x * (1.0f + 4.0f * drive)) / (1.0f + 1.5f * drive);

        if (coefTick-- <= 0)
        {
            coefTick = 16;
            const float cut = clampf (cutoff * std::exp2 (envAmt * fenv * 5.0f + accent * fenv * 1.5f), 30.0f, sr * 0.45f);
            f1.set (cut, 0.707f, sr);
            f2.set (cut, 0.6f + reso * 7.0f, sr);
        }
        const float y = f2.lowpass (f1.lowpass (x));
        return y * amp * (1.0f + 0.5f * accent);
    }
};
} // namespace dw
