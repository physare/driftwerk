#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace dw;

namespace
{
const juce::StringArray clockDivNames { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
const double clockDivBeats[] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

const juce::StringArray echoDivNames { "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2", "FREE" };
const double echoDivBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 0.0 };

const juce::StringArray scaleNames { "MINOR", "DORIAN", "PHRYG", "PENTA", "HARM", "WHOLE", "MAJOR", "LYDIAN" };
const std::vector<std::vector<int>> scales {
    { 0, 2, 3, 5, 7, 8, 10 },
    { 0, 2, 3, 5, 7, 9, 10 },
    { 0, 1, 3, 5, 7, 8, 10 },
    { 0, 3, 5, 7, 10 },
    { 0, 2, 3, 5, 7, 8, 11 },
    { 0, 2, 4, 6, 8, 10 },
    { 0, 2, 4, 5, 7, 9, 11 },
    { 0, 2, 4, 6, 7, 9, 11 },
};

// Dream pad voicings (semitones above the shared root). The arp's PADS mode plays only these notes.
const int dreamChords[6][6] {
    { 0, 4, 7, 11, 14, 19 }, { 0, 3, 7, 10, 14, 17 }, { 0, 2, 7, 12, 14, 19 },
    { 0, 4, 6, 11, 14, 18 }, { 0, 4, 7, 14, 16, 19 }, { 0, 5, 10, 15, 19, 22 } };
const juce::StringArray arpFollowNames { "SCALE", "PADS", "ROOTS" };

const juce::StringArray chordNames { "UNISON", "FIFTH", "OCTAVE", "MINOR", "SUS", "CLUSTER" };
const int chords[6][6] {
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 7, 7, 12, -12 },
    { 0, 0, 12, 12, -12, 24 },
    { 0, 3, 7, 12, 0, 7 },
    { 0, 5, 7, 12, 0, 10 },
    { 0, 2, 3, 7, 10, 14 },
};

const juce::StringArray lfoShapeNames { "SINE", "TRI", "S&H", "DRIFT" };

using NRange = juce::NormalisableRange<float>;

NRange skewed (float lo, float hi, float centre)
{
    NRange r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

struct Preset
{
    const char* name;
    const char* patch;
    std::vector<std::pair<const char*, float>> values; // real (un-normalised) values
    const char* mods = nullptr;                         // knob modulation, see setModFromString
};

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Drift Cathedral", nullptr, {} },
        { "Mirror Sequence",
          "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.saw>mix.in1;vco2.saw>mix.in2;"
          "mix.out>vcf1.in;arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;lfo1.out>vcf1.cv2;"
          "vcf1.out>vca1.in;vca1.out>out.in1;drn.out>vcf2.in;lfo2.out>vcf2.cv1;vcf2.out>out.in2;"
          "clk.div4>nz.clk;nz.snh>vco1.pwm",
          { { "clk.div", 3 }, { "arp.mutate", 0.06f }, { "arp.walk", 0.8f }, { "vcf1.res", 0.72f },
            { "vcf1.cut", 380 }, { "vcf1.cv2", 0.7f }, { "lfo1.rate", 0.02f }, { "drn.chord", 2 },
            { "echo.div", 2 }, { "echo.fb", 0.68f }, { "echo.mix", 0.45f }, { "vco2.fine", 0.12f } } },
        { "Black Tide",
          "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.sub>mix.in1;vco2.pls>mix.in2;"
          "mix.out>vcf1.in;arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;lfo2.out>vcf1.cv2;"
          "vcf1.out>vca1.in;vca1.out>out.in1;drn.out>vcf2.in;lfo1.out>vcf2.cv1;env2.out>vcf2.cv2;"
          "clk.div4>env2.gate;vcf2.out>out.in2",
          { { "clk.div", 1 }, { "arp.root", 31 }, { "arp.scale", 2 }, { "arp.density", 0.6f },
            { "drn.chord", 5 }, { "drn.oct", -2 }, { "drn.detune", 0.7f }, { "hall.size", 0.95f },
            { "hall.mix", 0.6f }, { "env2.a", 3 }, { "env2.r", 10 }, { "vcf2.cv2", 0.35f }, { "lfo1.shape", 3 } } },
        { "Pulse Engine",
          "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.pls>mix.in1;vco2.saw>mix.in2;"
          "mix.out>vcf1.in;arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;nz.snh>vcf1.cv2;"
          "clk.out>nz.clk;vcf1.out>vca1.in;vca1.out>out.in1;drn.out>vcf2.in;lfo1.out>vcf2.cv1;"
          "lfo2.out>vco1.pwm;vcf2.out>out.in2",
          { { "clk.div", 4 }, { "arp.scale", 2 }, { "arp.range", 3 }, { "arp.mutate", 0.25f },
            { "vco2.oct", 1 }, { "vcf1.cv2", 0.35f }, { "nz.slew", 0.1f }, { "echo.div", 1 },
            { "drn.level", 0.45f }, { "env1.d", 0.18f } } },
        { "Hand Weather (camera)",
          "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.saw>mix.in1;vco2.pls>mix.in2;"
          "mix.out>vcf1.in;arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;cam.y>vcf1.cv2;"
          "lfo2.out>vco2.pwm;vcf1.out>vca1.in;vca1.out>out.in1;drn.out>vcf2.in;cam.x>vcf2.cv1;"
          "cam.trig>env2.gate;env2.out>vcf2.cv2;vcf2.out>out.in2",
          { { "cam.on", 1 }, { "vcf1.cv2", 0.6f }, { "vcf2.cv1", 0.55f }, { "vcf2.cv2", 0.5f },
            { "env2.a", 0.05f }, { "env2.d", 2.5f }, { "env2.s", 0.0f }, { "env2.r", 3.0f } },
          "hall.mix<cam.y@0.45;echo.fb<cam.mot@0.35;arp.mutate<cam.mot@0.5" },
        { "Hand Garden (camera play)", nullptr,
          { { "cam.on", 1 }, { "play.on", 1 }, { "play.depth", 0.85f }, { "dream.level", 0.7f }, { "grain.level", 0.5f },
            { "tape.wow", 0.5f }, { "tape.age", 0.5f }, { "hall.shimmer", 0.4f }, { "out.lvl1", 0.3f } } },
        { "Airport Tapes", "drn.out>vcf2.in;lfo1.out>vcf2.cv1;vcf2.out>out.in2;clk.div4>nz.clk;nz.snh>vcf2.cv2",
          { { "dream.level", 0.9f }, { "dream.chord", 0 }, { "dream.rate", 0.7f }, { "dream.warp", 0.35f },
            { "drn.level", 0.35f }, { "drn.chord", 2 }, { "grain.level", 0.3f }, { "grain.size", 0.6f }, { "grain.density", 4 },
            { "tape.wow", 0.35f }, { "tape.age", 0.4f }, { "tape.hiss", 0.25f }, { "hall.mix", 0.6f }, { "hall.shimmer", 0.5f },
            { "echo.mix", 0.2f } } },
        { "Warped Cassette", nullptr,
          { { "dream.level", 0.75f }, { "dream.chord", 1 }, { "dream.warp", 0.95f }, { "grain.level", 0.6f }, { "grain.pitch", -12 },
            { "grain.reverse", 0.6f }, { "tape.wow", 0.85f }, { "tape.age", 0.8f }, { "tape.hiss", 0.5f }, { "tape.crush", 0.35f },
            { "out.lvl1", 0.35f }, { "clk.div", 1 }, { "vcf1.cut", 300 }, { "hall.mix", 0.4f } } },
    };
    return list;
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DriftwerkProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto fl = [&] (const char* id, const char* name, NRange r, float def)
    { ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def)); };
    auto in = [&] (const char* id, const char* name, int lo, int hi, int def)
    { ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto ch = [&] (const char* id, const char* name, const StringArray& c, int def)
    { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };
    auto bo = [&] (const char* id, const char* name, bool def)
    { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    // Clock
    ch ("clk.div", "Clock Division", clockDivNames, 3);
    fl ("clk.bpm", "Clock BPM (no host)", NRange (40.0f, 200.0f, 0.1f), 100.0f);
    bo ("clk.run", "Clock Run", true);
    fl ("out.tune", "Master Tune", NRange (-12.0f, 12.0f), 0.0f);
    // Random arpeggiator
    ch ("arp.scale", "Arp Scale", scaleNames, 0);
    ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { "arp.root", 1 }, "Root Note", 24, 60, 36,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
        { return MidiMessage::getMidiNoteName (v, true, true, 3); })));
    in ("arp.range", "Arp Range", 1, 4, 2);
    bo ("arp.on", "Arp On", true);
    ch ("arp.follow", "Arp Harmony", arpFollowNames, 1);
    in ("arp.len", "Arp Length", 1, 16, 16);
    fl ("arp.density", "Arp Density", NRange (0.0f, 1.0f), 0.82f);
    fl ("arp.mutate", "Arp Mutate", NRange (0.0f, 1.0f), 0.12f);
    fl ("arp.walk", "Arp Walk", NRange (0.0f, 1.0f), 0.55f);
    fl ("arp.gate", "Arp Gate", NRange (0.05f, 1.0f), 0.5f);
    // VCOs
    for (int v = 1; v <= 2; ++v)
    {
        auto id = [v] (const char* s) { return String ("vco") + String (v) + "." + s; };
        auto nm = [v] (const char* s) { return String ("VCO ") + String (v) + " " + s; };
        ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { id ("oct"), 1 }, nm ("Octave"), -2, 2, 0));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("fine"), 1 }, nm ("Fine"), NRange (-1.0f, 1.0f), v == 2 ? 0.08f : 0.0f));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("pw"), 1 }, nm ("Pulse Width"), NRange (0.05f, 0.95f), 0.5f));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("pwm"), 1 }, nm ("PWM Amount"), NRange (0.0f, 1.0f), 0.5f));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("fm"), 1 }, nm ("FM Amount"), NRange (0.0f, 1.0f), 0.0f));
    }
    // Drone bank
    ch ("drn.chord", "Drone Chord", chordNames, 1);
    in ("drn.oct", "Drone Octave", -3, 1, -1);
    fl ("drn.detune", "Drone Detune", NRange (0.0f, 1.0f), 0.45f);
    fl ("drn.drift", "Drone Drift", NRange (0.0f, 1.0f), 0.5f);
    fl ("drn.level", "Drone Level", NRange (0.0f, 1.0f), 0.7f);
    // Noise / S&H
    fl ("nz.slew", "S&H Slew", NRange (0.0f, 1.0f), 0.4f);
    // LFOs
    fl ("lfo1.rate", "LFO 1 Rate", skewed (0.005f, 8.0f, 0.15f), 0.03f);
    ch ("lfo1.shape", "LFO 1 Shape", lfoShapeNames, 0);
    fl ("lfo2.rate", "LFO 2 Rate", skewed (0.005f, 8.0f, 0.15f), 0.07f);
    ch ("lfo2.shape", "LFO 2 Shape", lfoShapeNames, 1);
    // Envelopes
    const float envDefs[2][4] { { 0.003f, 0.32f, 0.0f, 0.3f }, { 2.0f, 4.0f, 0.7f, 6.0f } };
    for (int e = 1; e <= 2; ++e)
    {
        auto id = [e] (const char* s) { return String ("env") + String (e) + "." + s; };
        auto nm = [e] (const char* s) { return String ("Env ") + String (e) + " " + s; };
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("a"), 1 }, nm ("Attack"), skewed (0.001f, 20.0f, 0.5f), envDefs[e - 1][0]));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("d"), 1 }, nm ("Decay"), skewed (0.005f, 20.0f, 0.8f), envDefs[e - 1][1]));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("s"), 1 }, nm ("Sustain"), NRange (0.0f, 1.0f), envDefs[e - 1][2]));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("r"), 1 }, nm ("Release"), skewed (0.005f, 30.0f, 1.0f), envDefs[e - 1][3]));
    }
    // Mixer
    const float mixDefs[4] { 0.7f, 0.55f, 0.5f, 0.5f };
    for (int m = 1; m <= 4; ++m)
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { String ("mix.l") + String (m), 1 },
                                                             String ("Mixer Level ") + String (m), NRange (0.0f, 1.0f), mixDefs[m - 1]));
    // Filters
    const float vcfDefs[2][5] { { 420.0f, 0.55f, 0.3f, 0.55f, 0.45f }, { 700.0f, 0.3f, 0.2f, 0.5f, 0.12f } };
    for (int f = 1; f <= 2; ++f)
    {
        auto id = [f] (const char* s) { return String ("vcf") + String (f) + "." + s; };
        auto nm = [f] (const char* s) { return String ("VCF ") + String (f) + " " + s; };
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("cut"), 1 }, nm ("Cutoff"), skewed (20.0f, 18000.0f, 900.0f), vcfDefs[f - 1][0]));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("res"), 1 }, nm ("Resonance"), NRange (0.0f, 1.0f), vcfDefs[f - 1][1]));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("drive"), 1 }, nm ("Drive"), NRange (0.0f, 1.0f), vcfDefs[f - 1][2]));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("cv1"), 1 }, nm ("CV1 Amount"), NRange (-1.0f, 1.0f), vcfDefs[f - 1][3]));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id ("cv2"), 1 }, nm ("CV2 Amount"), NRange (-1.0f, 1.0f), vcfDefs[f - 1][4]));
    }
    // VCAs
    fl ("vca1.gain", "VCA 1 Base", NRange (0.0f, 1.0f), 0.0f);
    fl ("vca1.cv", "VCA 1 CV Amount", NRange (0.0f, 1.0f), 1.0f);
    fl ("vca2.gain", "VCA 2 Base", NRange (0.0f, 1.0f), 1.0f);
    fl ("vca2.cv", "VCA 2 CV Amount", NRange (0.0f, 1.0f), 1.0f);
    // Output
    fl ("out.lvl1", "Out 1 Level", NRange (0.0f, 1.0f), 0.75f);
    fl ("out.lvl2", "Out 2 Level", NRange (0.0f, 1.0f), 0.6f);
    fl ("out.pan1", "Out 1 Pan", NRange (-1.0f, 1.0f), 0.0f);
    fl ("out.pan2", "Out 2 Pan", NRange (-1.0f, 1.0f), 0.0f);
    fl ("out.ens", "Ensemble", NRange (0.0f, 1.0f), 0.45f);
    fl ("out.vol", "Master Volume", NRange (-40.0f, 6.0f, 0.1f), -3.0f);
    // Echo
    ch ("echo.div", "Echo Time", echoDivNames, 2);
    fl ("echo.ms", "Echo Free Time", skewed (20.0f, 2000.0f, 400.0f), 420.0f);
    fl ("echo.fb", "Echo Feedback", NRange (0.0f, 0.97f), 0.55f);
    fl ("echo.mix", "Echo Mix", NRange (0.0f, 1.0f), 0.35f);
    fl ("echo.wow", "Echo Wow", NRange (0.0f, 1.0f), 0.3f);
    // Hall
    fl ("hall.size", "Hall Size", NRange (0.0f, 1.0f), 0.78f);
    fl ("hall.damp", "Hall Damping", NRange (0.0f, 1.0f), 0.45f);
    fl ("hall.mix", "Hall Mix", NRange (0.0f, 1.0f), 0.42f);
    // Dream loops
    ch ("dream.chord", "Dream Chord", StringArray { "MAJ 7", "MIN 9", "SUS 2", "LYDIAN", "ADD 9", "QUARTAL" }, 1);
    fl ("dream.rate", "Dream Speed", skewed (0.2f, 4.0f, 1.0f), 1.0f);
    fl ("dream.tone", "Dream Tone", NRange (0.0f, 1.0f), 0.3f);
    fl ("dream.warp", "Dream Warp", NRange (0.0f, 1.0f), 0.45f);
    fl ("dream.level", "Dream Level", NRange (0.0f, 1.0f), 0.55f);
    // Grain cloud
    fl ("grain.pos", "Grain Position", NRange (0.0f, 1.0f), 0.3f);
    fl ("grain.spray", "Grain Spray", NRange (0.0f, 1.0f), 0.3f);
    fl ("grain.size", "Grain Size", skewed (0.01f, 1.5f, 0.15f), 0.18f);
    fl ("grain.density", "Grain Density", skewed (0.5f, 80.0f, 10.0f), 10.0f);
    fl ("grain.pitch", "Grain Pitch", NRange (-24.0f, 24.0f), 0.0f);
    fl ("grain.reverse", "Grain Reverse", NRange (0.0f, 1.0f), 0.2f);
    fl ("grain.scan", "Grain Scan", NRange (-1.0f, 1.0f), 0.08f);
    fl ("grain.level", "Grain Level", NRange (0.0f, 1.0f), 0.35f);
    // Tape
    fl ("tape.wow", "Tape Wow", NRange (0.0f, 1.0f), 0.3f);
    fl ("tape.age", "Tape Age", NRange (0.0f, 1.0f), 0.3f);
    fl ("tape.hiss", "Tape Hiss", NRange (0.0f, 1.0f), 0.15f);
    fl ("tape.crush", "Tape Crush", NRange (0.0f, 1.0f), 0.0f);
    fl ("hall.shimmer", "Hall Shimmer", NRange (0.0f, 1.0f), 0.3f);
    // Hand play
    bo ("play.on", "Hand Play", false);
    fl ("play.depth", "Hand Play Depth", NRange (0.0f, 1.0f), 0.8f);
    // Camera
    bo ("cam.on", "Camera On", false);
    fl ("cam.sens", "Camera Sensitivity", NRange (0.0f, 1.0f), 0.5f);
    fl ("cam.slew", "Camera Smoothing", NRange (0.0f, 1.0f), 0.3f);

    return { ps.begin(), ps.end() };
}

//==============================================================================
DriftwerkProcessor::DriftwerkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DRIFTWERK", createLayout())
{
    for (auto& m : monitor) m.store (0.0f);
    setPatchFromString (defaultPatch);

    for (int i = 0; i < 16; ++i)
    {
        arp.degree[i] = (int) ((arpRng.next() * 0.5f + 0.5f) * 9.0f);
        arp.rnd[i] = arpRng.next() * 0.5f + 0.5f;
        arp.gate[i] = true;
    }
    for (int i = 0; i < 6; ++i)
        droneOsc[i].phase = (droneRng.next() * 0.5 + 0.5);

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            paramIndexMap[rp->getParameterID().toStdString()] = (int) paramList.size();
            paramList.push_back (rp);
            mods.push_back (std::make_unique<ModSlot>());
        }
    effective.resize (paramList.size(), 0.0f);
    makeDefaultSample();

    cameraPoller = std::make_unique<CameraPoller> (*this);
}

int DriftwerkProcessor::paramIndex (const juce::String& id) const
{
    const auto it = paramIndexMap.find (id.toStdString());
    return it == paramIndexMap.end() ? -1 : it->second;
}

//==============================================================================
void DriftwerkProcessor::makeDefaultSample()
{
    // A few seconds of synthetic "glass choir": stacked soft partials with slow beating
    // and a breath of noise, so GRAIN has something to chew on before you load a file.
    auto sd = std::make_unique<SampleData>();
    const double fs = 44100.0;
    const int n = (int) (fs * 6.0);
    sd->sampleRate = fs;
    sd->left.resize ((size_t) n); sd->right.resize ((size_t) n);
    const float notes[] { 57.0f, 64.0f, 69.0f, 71.0f, 76.0f, 81.0f };
    Random rng (0xFEEDu);
    float nlp = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) fs;
        float l = 0.0f, r = 0.0f;
        for (int k = 0; k < 6; ++k)
        {
            const float f = midiToHz (notes[k]);
            const float swell = 0.5f - 0.5f * std::cos (kTwoPi * t / (2.0f + (float) k * 0.7f));
            l += std::sin (kTwoPi * f * t) * swell * 0.12f;
            r += std::sin (kTwoPi * f * 1.003f * t + (float) k) * swell * 0.12f;
        }
        nlp += (rng.next() - nlp) * 0.05f;
        const float edge = std::min (1.0f, std::min (t, 6.0f - t) * 4.0f);
        sd->left[(size_t) i] = (l + nlp * 0.05f) * edge;
        sd->right[(size_t) i] = (r + nlp * 0.05f) * edge;
    }
    currentSample.store (sd.get());
    sampleStore.push_back (std::move (sd));
}

bool DriftwerkProcessor::loadSample (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) return false;

    const int maxLen = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 90.0));
    juce::AudioBuffer<float> buf ((int) juce::jmax (1u, reader->numChannels), maxLen);
    reader->read (&buf, 0, maxLen, 0, true, true);

    auto sd = std::make_unique<SampleData>();
    sd->sampleRate = reader->sampleRate;
    sd->left.assign (buf.getReadPointer (0), buf.getReadPointer (0) + maxLen);
    const int rc = buf.getNumChannels() > 1 ? 1 : 0;
    sd->right.assign (buf.getReadPointer (rc), buf.getReadPointer (rc) + maxLen);
    // Normalise to a comfortable peak.
    float peak = 0.0f;
    for (int i = 0; i < maxLen; ++i) peak = std::max ({ peak, std::abs (sd->left[(size_t) i]), std::abs (sd->right[(size_t) i]) });
    if (peak > 0.0001f)
        for (int i = 0; i < maxLen; ++i) { sd->left[(size_t) i] *= 0.7f / peak; sd->right[(size_t) i] *= 0.7f / peak; }

    currentSample.store (sd.get());
    sampleStore.push_back (std::move (sd));
    // Free samples older than the previous one; the audio thread only ever holds the current or previous.
    while (sampleStore.size() > 3) sampleStore.erase (sampleStore.begin());
    samplePath = f.getFullPathName();
    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
    return true;
}

void DriftwerkProcessor::rollScene()
{
    juce::Random r;
    auto set = [this] (const char* id, float real)
    {
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (real));
    };
    const int roots[] { 33, 35, 36, 38, 40, 41, 43, 45 };
    set ("arp.root", (float) roots[r.nextInt (8)]);
    // Pick one harmonic world so arp, pads and drone agree.
    if (r.nextBool())
    {   // minor: MINOR / DORIAN / PENTA with MIN 9, SUS 2 or QUARTAL pads, FIFTH / OCTAVE / MINOR / SUS drone
        const int sc[] { 0, 1, 3 }, dc[] { 1, 2, 5 }, dr[] { 1, 2, 3, 4 };
        set ("arp.scale", (float) sc[r.nextInt (3)]);
        set ("dream.chord", (float) dc[r.nextInt (3)]);
        set ("drn.chord", (float) dr[r.nextInt (4)]);
    }
    else
    {   // major: MAJOR / LYDIAN with MAJ 7, LYDIAN, SUS 2 or ADD 9 pads, FIFTH / OCTAVE / SUS drone
        const int sc[] { 6, 7 }, dc[] { 0, 3, 2, 4 }, dr[] { 1, 2, 4 };
        set ("arp.scale", (float) sc[r.nextInt (2)]);
        set ("dream.chord", (float) dc[r.nextInt (4)]);
        set ("drn.chord", (float) dr[r.nextInt (3)]);
    }
    set ("lfo1.rate", 0.01f + r.nextFloat() * r.nextFloat() * 0.3f);
    set ("lfo2.rate", 0.02f + r.nextFloat() * r.nextFloat() * 1.5f);
    set ("lfo1.shape", (float) r.nextInt (4));
    const float pitches[] { -12, -7, -5, 0, 0, 5, 7, 12 };
    set ("grain.pitch", pitches[r.nextInt (8)]);
    set ("grain.pos", r.nextFloat());
    set ("grain.reverse", r.nextFloat() * 0.7f);
    set ("echo.div", (float) r.nextInt (6));
    set ("vco2.oct", (float) (r.nextInt (3) - 1));
    set ("arp.len", (float) (4 + r.nextInt (13)));
    randomisePatch();
    sceneCounter.fetch_add (1);
}

juce::String DriftwerkProcessor::getModString() const
{
    juce::StringArray items;
    for (size_t k = 0; k < paramList.size(); ++k)
    {
        const int src = mods[k]->source.load();
        if (src >= 0 && src < NumOuts)
            items.add (paramList[k]->getParameterID() + "<" + outDefs[src].id + "@" + juce::String (mods[k]->depth.load(), 3));
    }
    return items.joinIntoString (";");
}

void DriftwerkProcessor::setModFromString (const juce::String& s)
{
    for (auto& m : mods) m->source.store (-1);
    juce::StringArray items;
    items.addTokens (s, ";", "");
    for (auto& item : items)
    {
        const int pi = paramIndex (item.upToFirstOccurrenceOf ("<", false, false).trim());
        const auto rest = item.fromFirstOccurrenceOf ("<", false, false);
        const int src = findOut (rest.upToFirstOccurrenceOf ("@", false, false).trim().toRawUTF8());
        const float depth = rest.fromFirstOccurrenceOf ("@", false, false).getFloatValue();
        if (pi >= 0 && src >= 0)
        {
            mods[(size_t) pi]->depth.store (juce::jlimit (-1.0f, 1.0f, depth));
            mods[(size_t) pi]->source.store (src);
        }
    }
}

DriftwerkProcessor::~DriftwerkProcessor()
{
    cameraPoller = nullptr;
    camera.close();
}

void DriftwerkProcessor::CameraPoller::timerCallback()
{
    // In hand-play mode a sudden gesture rolls a new random scene (at most every 2 s).
    const int tc = owner.camera.triggerCount.load();
    if (tc != owner.lastCamTrigForScene)
    {
        owner.lastCamTrigForScene = tc;
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (owner.apvts.getRawParameterValue ("play.on")->load() > 0.5f && now - owner.lastSceneTime > 2.0)
        {
            owner.lastSceneTime = now;
            owner.rollScene();
        }
    }
    const bool want = owner.apvts.getRawParameterValue ("cam.on")->load() > 0.5f;
    owner.camera.sensitivity.store (owner.apvts.getRawParameterValue ("cam.sens")->load());
    if (want && ! owner.camera.isOpen())
    {
        startTimer (owner.camera.open() ? 250 : 3000); // retry slowly if busy or missing
    }
    else if (! want && owner.camera.isOpen())
        owner.camera.close();
}

bool DriftwerkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DriftwerkProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    vcf1.sampleRate = vcf2.sampleRate = (float) sr;
    vcf1.reset(); vcf2.reset();
    ensemble.prepare ((float) sr);
    echo.prepare ((float) sr);
    reverb.prepare ((float) sr);
    dream.prepare ((float) sr);
    grains.prepare ((float) sr);
    tape.prepare ((float) sr);
    arp.clockPeriod = (int) (sr * 0.15);
}

//==============================================================================
void DriftwerkProcessor::setPatchFromString (const juce::String& s)
{
    for (auto& jack : patch) jack.store (-1);
    juce::StringArray pairs;
    pairs.addTokens (s, ";", "");
    for (auto& pair : pairs)
    {
        const auto o = pair.upToFirstOccurrenceOf (">", false, false).trim();
        const auto i = pair.fromFirstOccurrenceOf (">", false, false).trim();
        const int oi = findOut (o.toRawUTF8());
        const int ii = findIn (i.toRawUTF8());
        if (oi >= 0 && ii >= 0) patch[(size_t) ii].store (oi);
    }
}

juce::String DriftwerkProcessor::getPatchString() const
{
    juce::StringArray pairs;
    for (int i = 0; i < NumIns; ++i)
    {
        const int o = patch[(size_t) i].load();
        if (o >= 0) pairs.add (juce::String (outDefs[o].id) + ">" + inDefs[i].id);
    }
    return pairs.joinIntoString (";");
}

void DriftwerkProcessor::randomisePatch()
{
    juce::Random r;
    setPatchFromString (defaultPatch);
    const int modSources[] { O_LFO1, O_LFO2, O_ENV1, O_ENV2, O_SNH, O_ARP_RND, O_CLK4 };
    const int modTargets[] { I_VCO1_PWM, I_VCO2_PWM, I_VCO1_FM, I_VCO2_FM, I_VCF1_CV2, I_VCF2_CV1,
                             I_VCF2_CV2, I_LFO1_RATE, I_LFO2_RATE, I_VCA2_CV, I_DRONE_PITCH };
    for (int t : modTargets)
    {
        if (r.nextFloat() < 0.55f)
            patch[(size_t) t].store (modSources[r.nextInt ((int) std::size (modSources))]);
        else
            patch[(size_t) t].store (-1);
    }
    // Keep the drone in tune when something modulates its pitch: use only slow sources.
    if (patch[I_DRONE_PITCH].load() >= 0)
        patch[I_DRONE_PITCH].store (r.nextBool() ? O_LFO1 : -1);
    const int envGates[] { O_ARP_GATE, O_CLK4, O_CLK };
    patch[I_ENV2_GATE].store (envGates[r.nextInt (3)]);
    patch[I_MIX3].store (r.nextFloat() < 0.4f ? O_NOISE : -1);
    if (r.nextBool()) patch[I_MIX1].store (r.nextBool() ? O_VCO1_SUB : O_VCO1_PULSE);
    if (r.nextBool()) patch[I_MIX2].store (r.nextBool() ? O_VCO2_SAW : O_VCO2_SUB);
}

//==============================================================================
// The notes the arp may play inside one octave: the chosen scale, the pad chord's
// pitch classes (always consonant with DREAM), or just root and fifth.
int DriftwerkProcessor::arpPitchSet (int* out) const
{
    if (p.arpFollow == 1)
    {
        bool has[12] {};
        for (int k = 0; k < 6; ++k) has[dreamChords[juce::jlimit (0, 5, p.dreamChord)][k] % 12] = true;
        int n = 0;
        for (int k = 0; k < 12; ++k) if (has[k]) out[n++] = k;
        return n;
    }
    if (p.arpFollow == 2) { out[0] = 0; out[1] = 7; return 2; }
    const auto& sc = scales[(size_t) juce::jlimit (0, (int) scales.size() - 1, p.scale)];
    for (size_t k = 0; k < sc.size(); ++k) out[k] = sc[k];
    return (int) sc.size();
}

int DriftwerkProcessor::scaleSemitone (int degree) const
{
    int set[12];
    const int n = arpPitchSet (set);
    if (degree < 0) degree = 0;
    return set[degree % n] + 12 * (degree / n);
}

void DriftwerkProcessor::arpClock()
{
    const int len = juce::jlimit (1, 16, p.length);
    const int prevStep = arp.step;
    arp.step = (arp.step + 1) % len;

    int set[12];
    const int n = arpPitchSet (set);
    const int maxDeg = n * p.range;

    auto u = [this] { return arpRng.next() * 0.5f + 0.5f; };

    if (u() < p.mutate)
    {
        int d;
        if (u() < p.walk)
        {
            const int moves[] { -2, -1, -1, 1, 1, 2, 0, n };
            d = arp.degree[prevStep] + moves[(int) (u() * 7.999f)];
            if (d < 0) d = -d;
            if (d > maxDeg) d = 2 * maxDeg - d;
        }
        else
            d = (int) (u() * (float) (maxDeg + 1));
        arp.degree[arp.step] = juce::jlimit (0, maxDeg, d);
        arp.rnd[arp.step] = u();
    }

    const int deg = juce::jmin (arp.degree[arp.step], maxDeg);
    // Step 0 always plays, giving the loop an anchor; others follow density.
    const bool play = p.arpOn && (arp.step == 0 || arp.rnd[arp.step] < p.density);

    if (play)
    {
        arp.pitchOut = (float) scaleSemitone (deg) / 12.0f;
        arp.gateOut = true;
        arp.gateLeft = juce::jmax (8, (int) ((float) arp.clockPeriod * p.gateLen));
    }
    arp.rndOut = arp.rnd[arp.step] * 2.0f - 1.0f;
}

// Hand play: the camera morphs the whole instrument between three sound worlds.
//   hand X  : calm airport ambience  <->  warped tape memories  <->  cosmic sequence
//   hand Y  : darker / brighter, drier / more space
//   motion  : denser, more chaotic, more feedback
void DriftwerkProcessor::applyPlayMode()
{
    const int onIdx = paramIndex ("play.on");
    if (onIdx < 0 || effective[(size_t) onIdx] < 0.5f) return;
    const float depth = effective[(size_t) paramIndex ("play.depth")];
    const float x = camX, y = camY, m = camMot;

    struct World { const char* id; float a, b, c; };
    static const World worlds[] {
        { "dream.level", 0.85f, 0.65f, 0.15f }, { "drn.level", 0.3f, 0.4f, 0.75f },
        { "out.lvl1", 0.0f, 0.3f, 0.8f },       { "grain.level", 0.35f, 0.6f, 0.15f },
        { "tape.wow", 0.12f, 0.8f, 0.15f },     { "tape.age", 0.2f, 0.75f, 0.2f },
        { "tape.hiss", 0.08f, 0.45f, 0.05f },   { "tape.crush", 0.0f, 0.3f, 0.0f },
        { "hall.mix", 0.6f, 0.38f, 0.45f },     { "hall.shimmer", 0.5f, 0.15f, 0.1f },
        { "echo.mix", 0.28f, 0.35f, 0.42f },    { "dream.warp", 0.15f, 0.9f, 0.3f },
        { "dream.rate", 0.6f, 1.0f, 1.5f },     { "grain.size", 0.5f, 0.14f, 0.07f },
        { "out.ens", 0.6f, 0.3f, 0.45f },
    };
    const float t = juce::jlimit (0.0f, 2.0f, x + 1.0f); // 0..2 across the three worlds
    for (const auto& w : worlds)
    {
        const int k = paramIndex (w.id);
        if (k < 0) continue;
        auto* rp = paramList[(size_t) k];
        const float na = rp->convertTo0to1 (w.a), nb = rp->convertTo0to1 (w.b), nc = rp->convertTo0to1 (w.c);
        const float target = t < 1.0f ? na + (nb - na) * t : nb + (nc - nb) * (t - 1.0f);
        const float knob = rp->convertTo0to1 (effective[(size_t) k]);
        effective[(size_t) k] = rp->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, knob + (target - knob) * depth));
    }
    auto nudge = [this, depth] (const char* id, float amount)
    {
        const int k = paramIndex (id);
        if (k < 0) return;
        auto* rp = paramList[(size_t) k];
        const float n = rp->convertTo0to1 (effective[(size_t) k]) + amount * depth;
        effective[(size_t) k] = rp->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, n));
    };
    nudge ("vcf1.cut", 0.22f * y);  nudge ("vcf2.cut", 0.25f * y);
    nudge ("dream.tone", 0.35f * y); nudge ("hall.size", 0.12f * y); nudge ("hall.mix", -0.12f * y);
    nudge ("arp.density", 0.45f * m); nudge ("arp.mutate", 0.5f * m);
    nudge ("grain.density", 0.45f * m); nudge ("grain.spray", 0.5f * m);
    nudge ("echo.fb", 0.2f * m); nudge ("tape.wow", 0.25f * m);
}

void DriftwerkProcessor::updateBlockParams()
{
    // Apply knob modulation once per block: effective = knob position + depth * source.
    for (size_t k = 0; k < paramList.size(); ++k)
    {
        auto* rp = paramList[k];
        float n = rp->getValue();
        const int src = mods[k]->source.load (std::memory_order_relaxed);
        if (src >= 0 && src < NumOuts)
        {
            n = juce::jlimit (0.0f, 1.0f, n + mods[k]->depth.load (std::memory_order_relaxed) * outs[(size_t) src]);
            mods[k]->now.store (n, std::memory_order_relaxed);
        }
        effective[k] = rp->convertFrom0to1 (n);
    }
    applyPlayMode();
    auto f = [this] (const char* id) { const int k = paramIndex (id); return k >= 0 ? effective[(size_t) k] : 0.0f; };
    auto i = [&f] (const char* id) { return (int) std::lround (f (id)); };

    p.clkDiv = i ("clk.div"); p.clkBpm = f ("clk.bpm"); p.clkRun = f ("clk.run") > 0.5f;
    p.scale = i ("arp.scale"); p.root = i ("arp.root"); p.range = i ("arp.range"); p.length = i ("arp.len");
    p.density = f ("arp.density"); p.mutate = f ("arp.mutate"); p.walk = f ("arp.walk"); p.gateLen = f ("arp.gate");
    p.arpOn = f ("arp.on") > 0.5f; p.arpFollow = i ("arp.follow");
    p.vco1Oct = i ("vco1.oct"); p.vco1Fine = f ("vco1.fine"); p.vco1Pw = f ("vco1.pw"); p.vco1Pwm = f ("vco1.pwm"); p.vco1Fm = f ("vco1.fm");
    p.vco2Oct = i ("vco2.oct"); p.vco2Fine = f ("vco2.fine"); p.vco2Pw = f ("vco2.pw"); p.vco2Pwm = f ("vco2.pwm"); p.vco2Fm = f ("vco2.fm");
    p.droneChord = i ("drn.chord"); p.droneOct = i ("drn.oct"); p.droneDetune = f ("drn.detune");
    p.droneDriftAmt = f ("drn.drift"); p.droneLevel = f ("drn.level");
    p.snhSlew = f ("nz.slew");
    p.lfo1Rate = f ("lfo1.rate"); p.lfo1Shape = i ("lfo1.shape"); p.lfo2Rate = f ("lfo2.rate"); p.lfo2Shape = i ("lfo2.shape");
    p.e1a = f ("env1.a"); p.e1d = f ("env1.d"); p.e1s = f ("env1.s"); p.e1r = f ("env1.r");
    p.e2a = f ("env2.a"); p.e2d = f ("env2.d"); p.e2s = f ("env2.s"); p.e2r = f ("env2.r");
    p.mix[0] = f ("mix.l1"); p.mix[1] = f ("mix.l2"); p.mix[2] = f ("mix.l3"); p.mix[3] = f ("mix.l4");
    p.f1Cut = f ("vcf1.cut"); p.f1Res = f ("vcf1.res"); p.f1Drive = f ("vcf1.drive"); p.f1Cv1 = f ("vcf1.cv1"); p.f1Cv2 = f ("vcf1.cv2");
    p.f2Cut = f ("vcf2.cut"); p.f2Res = f ("vcf2.res"); p.f2Drive = f ("vcf2.drive"); p.f2Cv1 = f ("vcf2.cv1"); p.f2Cv2 = f ("vcf2.cv2");
    p.a1Gain = f ("vca1.gain"); p.a1Cv = f ("vca1.cv"); p.a2Gain = f ("vca2.gain"); p.a2Cv = f ("vca2.cv");
    p.lvl1 = f ("out.lvl1"); p.lvl2 = f ("out.lvl2"); p.pan1 = f ("out.pan1"); p.pan2 = f ("out.pan2");
    p.ens = f ("out.ens"); p.vol = juce::Decibels::decibelsToGain (f ("out.vol"), -40.0f);
    p.echoFb = f ("echo.fb"); p.echoMix = f ("echo.mix"); p.echoWow = f ("echo.wow");
    const int ed = i ("echo.div");
    p.echoTime = echoDivBeats[ed] > 0.0 ? (float) (echoDivBeats[ed] * samplesPerBeat / sr) : f ("echo.ms") * 0.001f;
    p.echoTime = juce::jlimit (0.02f, 2.4f, p.echoTime);
    p.camSlew = f ("cam.slew");
    p.tune = f ("out.tune");
    p.dreamChord = i ("dream.chord"); p.dreamRate = f ("dream.rate"); p.dreamTone = f ("dream.tone");
    p.dreamWarp = f ("dream.warp"); p.dreamLevel = f ("dream.level");
    p.gPos = f ("grain.pos"); p.gSpray = f ("grain.spray"); p.gSize = f ("grain.size"); p.gDensity = f ("grain.density");
    p.gPitch = f ("grain.pitch"); p.gRev = f ("grain.reverse"); p.gScan = f ("grain.scan"); p.gLevel = f ("grain.level");
    p.tWow = f ("tape.wow"); p.tAge = f ("tape.age"); p.tHiss = f ("tape.hiss"); p.tCrush = f ("tape.crush");
    p.shimmer = f ("hall.shimmer");
    p.revSize = f ("hall.size"); p.revDamp = f ("hall.damp"); p.revMix = f ("hall.mix");

    env1.set (p.e1a, p.e1d, p.e1s, p.e1r, (float) sr);
    env2.set (p.e2a, p.e2d, p.e2s, p.e2r, (float) sr);

    for (int k = 0; k < NumIns; ++k)
        patchCache[(size_t) k] = patch[(size_t) k].load (std::memory_order_relaxed);
}

//==============================================================================
void DriftwerkProcessor::renderSample (float& outL, float& outR)
{
    const float fsr = (float) sr;

    // ---- CLOCK
    double beat;
    if (hostSynced)
        beat = hostPpqAtBlock + (double) blockSampleIndex / samplesPerBeat;
    else
    {
        beatPos += 1.0 / samplesPerBeat;
        beat = beatPos;
    }
    if (p.clkRun)
    {
        const double div = clockDivBeats[juce::jlimit (0, 5, p.clkDiv)];
        const double t = beat / div;
        const double t4 = beat / (div * 4.0);
        clockHigh = (t - std::floor (t)) < 0.5;
        clock4High = (t4 - std::floor (t4)) < 0.5;
    }
    else
        clockHigh = clock4High = false;
    outs[O_CLK] = clockHigh ? 1.0f : 0.0f;
    outs[O_CLK4] = clock4High ? 1.0f : 0.0f;

    // ---- LFOs
    outs[O_LFO1] = lfo1.process (p.lfo1Shape, p.lfo1Rate * std::exp2 (3.0f * in (I_LFO1_RATE)), fsr);
    outs[O_LFO2] = lfo2.process (p.lfo2Shape, p.lfo2Rate * std::exp2 (3.0f * in (I_LFO2_RATE)), fsr);

    // ---- NOISE / S&H
    const float noise = noiseRng.next();
    outs[O_NOISE] = noise * 0.5f;
    {
        const bool clk = in (I_SNH_CLK, outs[O_CLK4]) > 0.5f;
        if (clk && ! snhLastClk) snhHeld = noise;
        snhLastClk = clk;
        snhOut += (snhHeld - snhOut) * onePoleCoef (0.0005f + p.snhSlew * p.snhSlew * 1.5f, fsr);
        outs[O_SNH] = snhOut;
    }

    // ---- RANDOM ARP
    {
        const bool clk = in (I_ARP_CLK, outs[O_CLK]) > 0.5f;
        ++arp.samplesSinceClock;
        if (clk && ! arp.lastClk)
        {
            if (arp.samplesSinceClock > 16 && arp.samplesSinceClock < (int) (sr * 4.0))
                arp.clockPeriod = arp.samplesSinceClock;
            arp.samplesSinceClock = 0;
            arpClock();
        }
        arp.lastClk = clk;
        if (arp.gateOut && --arp.gateLeft <= 0) arp.gateOut = false;
        outs[O_ARP_PITCH] = arp.pitchOut + in (I_ARP_TRANS);
        outs[O_ARP_GATE] = arp.gateOut ? 1.0f : 0.0f;
        outs[O_ARP_RND] = arp.rndOut;
    }

    const float baseNote = (float) (p.root + midiTranspose) + p.tune;

    // ---- DRONE BANK
    {
        if (++droneDriftCounter >= 1024)
        {
            droneDriftCounter = 0;
            for (auto& t : droneDriftTarget)
                if (droneRng.next() > 0.6f) t = droneRng.next();
        }
        const float driftCoef = 1024.0f / (fsr * 2.5f) / 1024.0f;
        const float detuneCents[6] { -1.0f, 1.0f, -0.55f, 0.55f, -0.25f, 0.3f };
        const float dronePitch = baseNote + 12.0f * ((float) p.droneOct + in (I_DRONE_PITCH));
        const int* chord = chords[juce::jlimit (0, 5, p.droneChord)];
        float sum = 0.0f;
        for (int k = 0; k < 6; ++k)
        {
            droneDrift[k] += (droneDriftTarget[k] - droneDrift[k]) * driftCoef;
            const float cents = detuneCents[k] * p.droneDetune * 22.0f + droneDrift[k] * p.droneDriftAmt * 18.0f;
            const float note = dronePitch + (float) chord[k] + cents * 0.01f;
            sum += droneOsc[k].process (BlepOsc::Saw, midiToHz (note), sr);
        }
        outs[O_DRONE] = sum * 0.2f * p.droneLevel;
    }

    // ---- VCOs
    {
        const float n1 = baseNote + 12.0f * (float) p.vco1Oct + p.vco1Fine
                       + 12.0f * (in (I_VCO1_PITCH) + in (I_VCO1_FM) * p.vco1Fm * 2.0f);
        const double f1 = midiToHz (n1);
        const float pw1 = p.vco1Pw + in (I_VCO1_PWM) * p.vco1Pwm * 0.42f;
        outs[O_VCO1_SAW] = 0.5f * vco1Saw.process (BlepOsc::Saw, f1, sr);
        outs[O_VCO1_PULSE] = 0.5f * vco1Pulse.process (BlepOsc::Square, f1, sr, pw1);
        outs[O_VCO1_SUB] = 0.5f * vco1Sub.process (BlepOsc::Square, f1 * 0.5, sr);

        const float n2 = baseNote + 12.0f * (float) p.vco2Oct + p.vco2Fine
                       + 12.0f * (in (I_VCO2_PITCH) + in (I_VCO2_FM) * p.vco2Fm * 2.0f);
        const double f2 = midiToHz (n2);
        const float pw2 = p.vco2Pw + in (I_VCO2_PWM) * p.vco2Pwm * 0.42f;
        outs[O_VCO2_SAW] = 0.5f * vco2Saw.process (BlepOsc::Saw, f2, sr);
        outs[O_VCO2_PULSE] = 0.5f * vco2Pulse.process (BlepOsc::Square, f2, sr, pw2);
        outs[O_VCO2_SUB] = 0.5f * vco2Sub.process (BlepOsc::Square, f2 * 0.5, sr);
    }

    // ---- ENVELOPES
    {
        const bool g1 = in (I_ENV1_GATE) > 0.5f;
        if (g1 && ! env1LastGate) env1.noteOn();
        if (! g1 && env1LastGate) env1.noteOff();
        env1LastGate = g1;
        outs[O_ENV1] = env1.process();

        const bool g2 = in (I_ENV2_GATE) > 0.5f;
        if (g2 && ! env2LastGate) env2.noteOn();
        if (! g2 && env2LastGate) env2.noteOff();
        env2LastGate = g2;
        outs[O_ENV2] = env2.process();
    }

    // ---- MIXER
    outs[O_MIX] = in (I_MIX1) * p.mix[0] + in (I_MIX2) * p.mix[1] + in (I_MIX3) * p.mix[2] + in (I_MIX4) * p.mix[3];

    // ---- FILTERS
    {
        const float c1 = p.f1Cut * std::exp2 (5.0f * (in (I_VCF1_CV1) * p.f1Cv1 + in (I_VCF1_CV2) * p.f1Cv2));
        outs[O_VCF1] = vcf1.process (in (I_VCF1_IN), c1, p.f1Res, p.f1Drive);
        const float c2 = p.f2Cut * std::exp2 (5.0f * (in (I_VCF2_CV1) * p.f2Cv1 + in (I_VCF2_CV2) * p.f2Cv2));
        outs[O_VCF2] = vcf2.process (in (I_VCF2_IN), c2, p.f2Res, p.f2Drive);
    }

    // ---- VCAs
    outs[O_VCA1] = in (I_VCA1_IN) * clampf (p.a1Gain + in (I_VCA1_CV) * p.a1Cv, 0.0f, 1.5f);
    outs[O_VCA2] = in (I_VCA2_IN) * clampf (p.a2Gain + in (I_VCA2_CV) * p.a2Cv, 0.0f, 1.5f);

    // ---- CAMERA
    {
        const float coef = onePoleCoef (0.01f + p.camSlew * p.camSlew * 1.2f, fsr);
        camX += (camera.handX.load (std::memory_order_relaxed) - camX) * coef;
        camY += (camera.handY.load (std::memory_order_relaxed) - camY) * coef;
        camMot += (camera.motion.load (std::memory_order_relaxed) - camMot) * coef;
        const int tc = camera.triggerCount.load (std::memory_order_relaxed);
        if (tc != camTrigSeen) { camTrigSeen = tc; camTrigLeft = (int) (sr * 0.03); }
        if (camTrigLeft > 0) --camTrigLeft;
        outs[O_CAM_X] = camX;
        outs[O_CAM_Y] = camY;
        outs[O_CAM_MOTION] = camMot;
        outs[O_CAM_TRIG] = camTrigLeft > 0 ? 1.0f : 0.0f;
    }

    // ---- DREAM + GRAIN (stereo, normalled straight to the master bus unless their OUT is patched)
    float dl = 0.0f, dr = 0.0f, gl = 0.0f, gr = 0.0f;
    {
        const float dm = dream.process (dreamChords[juce::jlimit (0, 5, p.dreamChord)], 6, baseNote,
                                        p.dreamRate, p.dreamTone, p.dreamWarp, dl, dr);
        outs[O_DREAM] = dm * p.dreamLevel * 1.6f;
        dl *= p.dreamLevel; dr *= p.dreamLevel;

        const bool trig = in (I_GRAIN_TRIG) > 0.5f;
        const float gpos = p.gPos + in (I_GRAIN_POS) * 0.5f;
        const float gm = grains.process (currentSample.load (std::memory_order_acquire), gpos - std::floor (gpos), p.gSpray,
                                         p.gSize, p.gDensity, p.gPitch + 12.0f * in (I_GRAIN_PITCH), p.gRev, p.gScan,
                                         trig && ! grainLastTrig, gl, gr);
        grainLastTrig = trig;
        outs[O_GRAIN] = gm * p.gLevel;
        gl *= p.gLevel; gr *= p.gLevel;
    }
    bool dreamPatched = false, grainPatched = false;
    for (int k = 0; k < NumIns; ++k)
    {
        dreamPatched |= patchCache[(size_t) k] == O_DREAM;
        grainPatched |= patchCache[(size_t) k] == O_GRAIN;
    }

    // ---- OUTPUT + SPACE
    const float s1 = in (I_OUT1) * p.lvl1;
    const float s2 = in (I_OUT2) * p.lvl2;
    const float a1 = (p.pan1 + 1.0f) * 0.25f * kPi;
    const float a2 = (p.pan2 + 1.0f) * 0.25f * kPi;
    float l = s1 * std::cos (a1) + s2 * std::cos (a2);
    float r = s1 * std::sin (a1) + s2 * std::sin (a2);
    if (! dreamPatched) { l += dl; r += dr; }
    if (! grainPatched) { l += gl; r += gr; }

    ensemble.process (l, r, p.ens);
    tape.process (l, r, p.tWow, p.tAge, p.tHiss, p.tCrush);
    echo.process (l, r, p.echoTime, p.echoFb, p.echoMix, p.echoWow);
    reverb.process (l, r, p.revSize, p.revDamp, p.revMix, p.shimmer);

    l *= p.vol * 2.0f; r *= p.vol * 2.0f;
    // DC blocker
    dcL = l - dcInL + 0.9995f * dcL; dcInL = l;
    dcR = r - dcInR + 0.9995f * dcR; dcInR = r;
    outL = softClip (dcL);
    outR = softClip (dcR);
}

void DriftwerkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Tempo / transport
    double bpm = 0.0;
    hostSynced = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                {
                    hostSynced = true;
                    hostPpqAtBlock = *ppq;
                    beatPos = *ppq; // keep the free-running clock continuous after stop
                }
        }
    }
    if (bpm <= 0.0) bpm = apvts.getRawParameterValue ("clk.bpm")->load();
    samplesPerBeat = sr * 60.0 / juce::jlimit (20.0, 400.0, bpm);

    updateBlockParams();

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) midiTranspose = juce::jlimit (-36, 36, m.getNoteNumber() - 60);
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const int n = buffer.getNumSamples();
    int sw = scopeWrite.load (std::memory_order_relaxed);

    for (int s = 0; s < n; ++s)
    {
        blockSampleIndex = s;
        float l, r;
        renderSample (l, r);
        L[s] = l;
        if (R) R[s] = r;

        scope[sw] = 0.5f * (l + r);
        sw = (sw + 1) % scopeSize;

        if (++monitorCounter >= 64)
        {
            monitorCounter = 0;
            for (int k = 0; k < NumOuts; ++k)
                monitor[(size_t) k].store (outs[(size_t) k], std::memory_order_relaxed);
        }
    }
    scopeWrite.store (sw, std::memory_order_relaxed);
    grainPlayPos.store (grains.lastPlayPos, std::memory_order_relaxed);
    {
        float pk = 0.0f;
        for (int s2 = 0; s2 < n; ++s2) pk = std::max (pk, std::abs (L[s2]));
        outputLevel.store (std::max (pk, outputLevel.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
    }
    arpStep.store (arp.step);
    arpLength.store (p.length);
    midi.clear();
}

//==============================================================================
int DriftwerkProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String DriftwerkProcessor::getProgramName (int index)
{
    if (index >= 0 && index < getNumPrograms()) return presets()[(size_t) index].name;
    return {};
}

void DriftwerkProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms()) return;
    currentProgram = index;
    const auto& pr = presets()[(size_t) index];

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [id, value] : pr.values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));

    setPatchFromString (pr.patch != nullptr ? pr.patch : defaultPatch);
    setModFromString (pr.mods != nullptr ? pr.mods : "");
}

void DriftwerkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("patch", getPatchString(), nullptr);
    state.setProperty ("mods", getModString(), nullptr);
    state.setProperty ("sample", samplePath, nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DriftwerkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid() && state.hasType (apvts.state.getType()))
        {
            apvts.replaceState (state);
            setPatchFromString (state.getProperty ("patch", defaultPatch).toString());
            setModFromString (state.getProperty ("mods", {}).toString());
            const juce::String sp = state.getProperty ("sample", {}).toString();
            if (sp.isNotEmpty() && juce::File (sp).existsAsFile()) loadSample (juce::File (sp));
            currentProgram = state.getProperty ("program", 0);
        }
    }
}

juce::AudioProcessorEditor* DriftwerkProcessor::createEditor() { return new DriftwerkEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DriftwerkProcessor(); }
