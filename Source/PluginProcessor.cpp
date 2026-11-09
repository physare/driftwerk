#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>

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
const juce::StringArray dreamChordNames { "MAJ 7", "MIN 9", "SUS 2", "LYDIAN", "ADD 9", "QUARTAL" };

// Chord progressions: four chords each, as { root offset in semitones, pad chord, drone chord }.
// Every chord stays inside the key (except CREEPY, which leaves it on purpose).
const juce::StringArray progNames { "OFF", "PEACEFUL", "DREAMY", "DRIFTING", "CREEPY" };
const juce::StringArray progBarNames { "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS" };
const int progBars[] { 1, 2, 4, 8, 16 };
struct ProgChord { int offset, dream, drone; };
const ProgChord progTable[5][2][4] {
    { { { 0, 1, 1 }, { 0, 1, 1 }, { 0, 1, 1 }, { 0, 1, 1 } }, { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } } },
    // PEACEFUL   minor: i  VI  III  VII              major: I  vi  IV  V
    { { { 0, 1, 1 }, { 8, 0, 1 }, { 3, 4, 1 }, { 10, 2, 1 } }, { { 0, 0, 1 }, { 9, 1, 1 }, { 5, 0, 1 }, { 7, 2, 1 } } },
    // DREAMY     minor: i  iv  i  VI                 major: I  IV(lyd)  I  vi
    { { { 0, 1, 2 }, { 5, 1, 2 }, { 0, 1, 2 }, { 8, 0, 2 } }, { { 0, 0, 2 }, { 5, 3, 2 }, { 0, 0, 2 }, { 9, 1, 2 } } },
    // DRIFTING   minor: i(quartal) VII(sus) VI(add9) iv    major: I(add9) V(sus) vi IV(lyd)
    { { { 0, 5, 4 }, { 10, 2, 4 }, { 8, 4, 1 }, { 5, 1, 4 } }, { { 0, 4, 4 }, { 7, 2, 4 }, { 9, 1, 1 }, { 5, 3, 4 } } },
    // CREEPY     i  bII  bVI  tritone, slow glides
    { { { 0, 5, 3 }, { 1, 2, 5 }, { 8, 1, 3 }, { 6, 5, 5 } }, { { 0, 5, 3 }, { 1, 2, 5 }, { 8, 1, 3 }, { 6, 5, 5 } } },
};

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

// Drone TYPE: what each of the six drone voices is made of. COLOUR (drn.wave) shapes each type.
const juce::StringArray droneTypeNames { "SAWS", "ORGAN", "STRINGS", "GLASS", "CHOIR" };
// CHORDS layer styles.
const juce::StringArray chordStyleNames { "PAD", "SWELL", "PULSE", "ARP HITS", "STRUM" };

using NRange = juce::NormalisableRange<float>;

NRange skewed (float lo, float hi, float centre)
{
    NRange r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

struct Preset
{
    const char* group;                                  // section in the preset menu
    const char* name;
    const char* patch;                                  // nullptr = the default patch
    std::vector<std::pair<const char*, float>> values; // real (un-normalised) values; the rest go to defaults
    const char* mods = nullptr;                         // knob modulation, see setModFromString
};

// Shared building blocks for patches.
#define SEQ_CORE "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;mix.out>vcf1.in;arp.gate>env1.gate;" \
                 "env1.out>vcf1.cv1;env1.out>vca1.cv;vcf1.out>vca1.in;vca1.out>out.in1;"
#define DRONE_OUT "drn.out>vcf2.in;vcf2.out>out.in2;"

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        //======================================================================  SOUNDBATH: no plucks, all breath
        { "Soundbath", "Drift Cathedral", nullptr, {} },
        { "Soundbath", "Singing Bowls", DRONE_OUT "lfo1.out>vcf2.cv1;lfo2.out>vcf2.cv2",
          { { "prog.mode", 2 }, { "prog.bars", 3 }, { "prog.glide", 0.35f }, { "arp.on", 0 }, { "out.lvl1", 0 }, { "arp.scale", 1 },
            { "drn.chord", 2 }, { "drn.oct", -2 }, { "drn.detune", 0.25f }, { "drn.level", 0.8f },
            { "vcf2.cut", 520 }, { "vcf2.res", 0.88f }, { "vcf2.cv1", 0.55f }, { "vcf2.cv2", 0.35f }, { "out.lvl2", 0.55f },
            { "lfo1.rate", 0.018f }, { "lfo2.rate", 0.045f }, { "lfo2.shape", 3 },
            { "dream.chord", 2 }, { "dream.rate", 0.45f }, { "dream.tone", 0.2f }, { "dream.warp", 0.15f }, { "dream.level", 0.8f },
            { "grain.level", 0.3f }, { "grain.size", 1.1f }, { "grain.density", 3 }, { "grain.pitch", 12 }, { "grain.spray", 0.6f },
            { "hall.size", 0.97f }, { "hall.mix", 0.62f }, { "hall.shimmer", 0.55f }, { "out.ens", 0.7f },
            { "tape.wow", 0.12f }, { "tape.hiss", 0.08f }, { "echo.mix", 0.15f } } },
        { "Soundbath", "Low Tide Breathing", "drn.out>vcf2.in;vcf2.out>vca2.in;vca2.out>out.in2;lfo1.out>vca2.cv;lfo2.out>vcf2.cv1",
          { { "prog.mode", 1 }, { "prog.bars", 3 }, { "prog.glide", 0.4f }, { "drn.breath", 0.5f }, { "drn.air", 0.35f }, { "drn.wave", 0.5f }, { "arp.on", 0 }, { "out.lvl1", 0 },
            { "drn.chord", 1 }, { "drn.oct", -2 }, { "drn.detune", 0.6f }, { "vca2.gain", 0.25f }, { "vca2.cv", 0.75f },
            { "lfo1.rate", 0.08f }, { "lfo1.shape", 0 }, { "lfo2.rate", 0.01f }, { "lfo2.shape", 3 },
            { "vcf2.cut", 320 }, { "vcf2.res", 0.4f }, { "vcf2.cv1", 0.4f },
            { "dream.chord", 1 }, { "dream.rate", 0.3f }, { "dream.level", 0.5f }, { "dream.tone", 0.15f },
            { "tape.hiss", 0.32f }, { "tape.wow", 0.2f }, { "tape.age", 0.35f },
            { "grain.level", 0.15f }, { "grain.pitch", -12 }, { "grain.size", 0.9f }, { "hall.mix", 0.55f }, { "hall.size", 0.92f } } },
        { "Soundbath", "Cathedral Ghosts", DRONE_OUT "lfo1.out>vcf2.cv1",
          { { "prog.mode", 3 }, { "prog.bars", 3 }, { "prog.glide", 0.3f }, { "arp.on", 0 }, { "out.lvl1", 0 }, { "arp.scale", 7 },
            { "drn.chord", 2 }, { "drn.level", 0.4f },
            { "grain.reverse", 0.9f }, { "grain.pitch", -12 }, { "grain.size", 1.2f }, { "grain.density", 6 },
            { "grain.spray", 0.8f }, { "grain.level", 0.6f }, { "grain.scan", 0.02f },
            { "dream.chord", 3 }, { "dream.level", 0.6f }, { "dream.rate", 0.5f },
            { "hall.size", 0.99f }, { "hall.mix", 0.7f }, { "hall.shimmer", 0.7f },
            { "echo.div", 5 }, { "echo.fb", 0.72f }, { "echo.mix", 0.3f } } },
        { "Soundbath", "Airport Tapes", "drn.out>vcf2.in;lfo1.out>vcf2.cv1;vcf2.out>out.in2;clk.div4>nz.clk;nz.snh>vcf2.cv2",
          { { "dream.level", 0.9f }, { "dream.chord", 0 }, { "dream.rate", 0.7f }, { "dream.warp", 0.35f },
            { "drn.level", 0.35f }, { "drn.chord", 2 }, { "grain.level", 0.3f }, { "grain.size", 0.6f }, { "grain.density", 4 },
            { "tape.wow", 0.35f }, { "tape.age", 0.4f }, { "tape.hiss", 0.25f }, { "hall.mix", 0.6f }, { "hall.shimmer", 0.5f },
            { "echo.mix", 0.2f }, { "arp.scale", 6 } } },

        //======================================================================  PLUCKS + SEQUENCES
        { "Plucks + sequences", "Mirror Sequence",
          "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.saw>mix.in1;vco2.saw>mix.in2;"
          "mix.out>vcf1.in;arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;lfo1.out>vcf1.cv2;"
          "vcf1.out>vca1.in;vca1.out>out.in1;drn.out>vcf2.in;lfo2.out>vcf2.cv1;vcf2.out>out.in2;"
          "clk.div4>nz.clk;nz.snh>vco1.pwm",
          { { "clk.div", 3 }, { "arp.mutate", 0.06f }, { "arp.walk", 0.8f }, { "vcf1.res", 0.72f },
            { "vcf1.cut", 380 }, { "vcf1.cv2", 0.7f }, { "lfo1.rate", 0.02f }, { "drn.chord", 2 },
            { "echo.div", 2 }, { "echo.fb", 0.68f }, { "echo.mix", 0.45f }, { "vco2.fine", 0.12f } } },
        { "Plucks + sequences", "Berlin Night Drive",
          SEQ_CORE "vco1.saw>mix.in1;vco2.sub>mix.in2;lfo1.out>vcf1.cv2;" DRONE_OUT "lfo2.out>vcf2.cv1",
          { { "clk.div", 3 }, { "arp.follow", 0 }, { "arp.scale", 0 }, { "arp.len", 8 }, { "arp.mutate", 0.04f },
            { "arp.walk", 0.3f }, { "arp.density", 0.95f }, { "arp.range", 2 }, { "arp.gate", 0.35f },
            { "vco2.oct", -1 }, { "vcf1.cut", 300 }, { "vcf1.res", 0.8f }, { "vcf1.drive", 0.55f }, { "vcf1.cv1", 0.6f },
            { "vcf1.cv2", 0.75f }, { "lfo1.rate", 0.012f }, { "lfo1.shape", 1 }, { "env1.d", 0.22f },
            { "drn.chord", 1 }, { "drn.level", 0.45f }, { "dream.chord", 1 }, { "dream.level", 0.25f },
            { "echo.div", 2 }, { "echo.fb", 0.62f }, { "echo.mix", 0.4f }, { "hall.mix", 0.45f } } },
        { "Plucks + sequences", "Glass Harp",
          SEQ_CORE "vco1.pls>mix.in1;vco2.saw>mix.in2;lfo2.out>vco1.pwm;" DRONE_OUT,
          { { "prog.mode", 1 }, { "prog.bars", 2 }, { "clk.div", 3 }, { "arp.follow", 1 }, { "arp.scale", 6 }, { "arp.root", 48 }, { "arp.range", 3 },
            { "arp.density", 0.7f }, { "arp.mutate", 0.15f }, { "arp.walk", 0.7f }, { "arp.gate", 0.15f },
            { "env1.a", 0.001f }, { "env1.d", 0.12f }, { "env1.r", 0.15f },
            { "vcf1.cut", 900 }, { "vcf1.res", 0.75f }, { "vcf1.cv1", 0.75f }, { "vcf1.cv2", 0.0f }, { "vco2.oct", 1 },
            { "dream.chord", 4 }, { "dream.level", 0.35f }, { "drn.chord", 2 }, { "drn.level", 0.3f },
            { "echo.div", 2 }, { "echo.fb", 0.6f }, { "echo.mix", 0.45f }, { "hall.mix", 0.45f }, { "hall.shimmer", 0.35f } } },
        { "Plucks + sequences", "Kalimba Rain",
          SEQ_CORE "vco1.saw>mix.in1;vco2.sub>vco1.fm;arp.gate>grain.trig;" DRONE_OUT,
          { { "clk.div", 4 }, { "arp.follow", 0 }, { "arp.scale", 3 }, { "arp.root", 45 }, { "arp.range", 4 },
            { "arp.density", 0.45f }, { "arp.mutate", 0.3f }, { "arp.walk", 0.4f }, { "arp.gate", 0.1f },
            { "vco1.fm", 0.35f }, { "vco2.oct", 1 }, { "env1.d", 0.18f }, { "env1.r", 0.25f },
            { "vcf1.cut", 1400 }, { "vcf1.res", 0.35f }, { "vcf1.cv2", 0.0f },
            { "grain.src", 4 }, { "grain.level", 0.3f }, { "grain.pitch", 12 }, { "grain.size", 0.05f }, { "grain.density", 1 }, { "grain.spray", 0.9f },
            { "dream.chord", 1 }, { "dream.level", 0.3f }, { "drn.level", 0.3f }, { "drn.chord", 1 },
            { "echo.div", 1 }, { "echo.fb", 0.5f }, { "echo.mix", 0.35f }, { "hall.mix", 0.5f } } },
        { "Plucks + sequences", "Toy Piano Ghost",
          SEQ_CORE "vco1.pls>mix.in1;vco2.saw>mix.in2;" DRONE_OUT,
          { { "clk.div", 1 }, { "arp.follow", 1 }, { "arp.scale", 6 }, { "arp.root", 48 }, { "arp.density", 0.6f },
            { "arp.gate", 0.2f }, { "vco1.oct", 1 }, { "vco2.oct", 2 }, { "vco2.fine", 0.05f }, { "vco1.pw", 0.3f },
            { "env1.d", 0.25f }, { "vcf1.cut", 2200 }, { "vcf1.res", 0.2f }, { "vcf1.cv2", 0.0f },
            { "tape.wow", 0.6f }, { "tape.age", 0.6f }, { "tape.crush", 0.2f }, { "tape.hiss", 0.3f },
            { "grain.src", 2 }, { "grain.level", 0.3f }, { "grain.size", 0.5f },
            { "dream.chord", 0 }, { "dream.warp", 0.7f }, { "dream.level", 0.45f }, { "drn.level", 0.25f }, { "drn.chord", 2 },
            { "echo.div", 3 }, { "echo.mix", 0.3f }, { "hall.mix", 0.5f } } },
        { "Plucks + sequences", "Black Tide",
          "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.sub>mix.in1;vco2.pls>mix.in2;"
          "mix.out>vcf1.in;arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;lfo2.out>vcf1.cv2;"
          "vcf1.out>vca1.in;vca1.out>out.in1;drn.out>vcf2.in;lfo1.out>vcf2.cv1;env2.out>vcf2.cv2;"
          "clk.div4>env2.gate;vcf2.out>out.in2",
          { { "clk.div", 1 }, { "arp.root", 31 }, { "arp.scale", 2 }, { "arp.follow", 0 }, { "arp.density", 0.6f }, { "dream.chord", 5 },
            { "drn.chord", 5 }, { "drn.oct", -2 }, { "drn.detune", 0.7f }, { "hall.size", 0.95f }, { "dream.level", 0.2f },
            { "hall.mix", 0.6f }, { "env2.a", 3 }, { "env2.r", 10 }, { "vcf2.cv2", 0.35f }, { "lfo1.shape", 3 } } },
        { "Plucks + sequences", "Pulse Engine",
          "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.pls>mix.in1;vco2.saw>mix.in2;"
          "mix.out>vcf1.in;arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;nz.snh>vcf1.cv2;"
          "clk.out>nz.clk;vcf1.out>vca1.in;vca1.out>out.in1;drn.out>vcf2.in;lfo1.out>vcf2.cv1;"
          "lfo2.out>vco1.pwm;vcf2.out>out.in2",
          { { "clk.div", 4 }, { "arp.scale", 2 }, { "arp.follow", 0 }, { "arp.range", 3 }, { "arp.mutate", 0.25f },
            { "vco2.oct", 1 }, { "vcf1.cv2", 0.35f }, { "nz.slew", 0.1f }, { "echo.div", 1 }, { "dream.level", 0.2f }, { "dream.chord", 5 },
            { "drn.level", 0.45f }, { "env1.d", 0.18f } } },

        //======================================================================  WEIRD: still in key, but strange
        { "Weird", "Warped Cassette", nullptr,
          { { "dream.level", 0.75f }, { "dream.chord", 1 }, { "dream.warp", 0.95f }, { "grain.level", 0.6f }, { "grain.pitch", -12 },
            { "grain.reverse", 0.6f }, { "tape.wow", 0.85f }, { "tape.age", 0.8f }, { "tape.hiss", 0.5f }, { "tape.crush", 0.35f },
            { "out.lvl1", 0.35f }, { "clk.div", 1 }, { "vcf1.cut", 300 }, { "hall.mix", 0.4f } } },
        { "Weird", "Insect Choir",
          SEQ_CORE "vco1.pls>mix.in1;arp.rnd>vco2.fm;vco2.saw>mix.in2;lfo2.out>vcf1.cv2;" DRONE_OUT,
          { { "clk.div", 5 }, { "arp.follow", 0 }, { "arp.scale", 5 }, { "arp.range", 4 }, { "arp.density", 0.35f },
            { "arp.mutate", 0.5f }, { "arp.gate", 0.08f }, { "env1.d", 0.04f }, { "env1.r", 0.06f },
            { "vcf1.cut", 2000 }, { "vcf1.res", 0.9f }, { "vcf1.cv2", 0.4f }, { "lfo2.rate", 6.0f }, { "lfo2.shape", 2 },
            { "vco2.fm", 0.6f }, { "vco2.oct", 2 },
            { "grain.level", 0.3f }, { "grain.size", 0.02f }, { "grain.density", 60 }, { "grain.spray", 1.0f }, { "grain.pitch", 19 },
            { "dream.chord", 3 }, { "dream.level", 0.2f }, { "drn.level", 0.3f }, { "drn.chord", 4 },
            { "hall.mix", 0.5f }, { "echo.div", 4 }, { "echo.mix", 0.3f } } },
        { "Weird", "Broken Radio Prayer",
          SEQ_CORE "vco1.saw>mix.in1;nz.noise>mix.in3;nz.snh>vcf1.cv2;clk.div4>nz.clk;" DRONE_OUT,
          { { "arp.follow", 2 }, { "arp.density", 0.3f }, { "arp.gate", 0.7f }, { "clk.div", 1 },
            { "mix.l3", 0.25f }, { "vcf1.res", 0.85f }, { "vcf1.cv2", 0.6f }, { "nz.slew", 0.6f },
            { "tape.crush", 0.6f }, { "tape.age", 0.9f }, { "tape.hiss", 0.6f }, { "tape.wow", 0.7f },
            { "grain.scan", -0.4f }, { "grain.pitch", -7 }, { "grain.level", 0.5f }, { "grain.reverse", 0.5f },
            { "dream.warp", 1.0f }, { "dream.level", 0.5f }, { "dream.chord", 2 },
            { "echo.div", 6 }, { "echo.ms", 333 }, { "echo.fb", 0.85f }, { "echo.wow", 0.8f }, { "echo.mix", 0.4f } } },
        { "Weird", "Gravity Well",
          "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.saw>mix.in1;vco2.saw>vco1.fm;mix.out>vcf1.in;"
          "vcf1.out>vca1.in;vca1.out>out.in1;lfo2.out>vcf1.cv2;lfo1.out>drn.pitch;" DRONE_OUT,
          { { "prog.mode", 4 }, { "prog.bars", 2 }, { "prog.glide", 0.6f }, { "clk.div", 0 }, { "arp.density", 0.3f }, { "arp.range", 1 }, { "arp.follow", 2 }, { "arp.mutate", 0.1f },
            { "vco1.oct", -1 }, { "vco1.fm", 0.7f }, { "vco2.oct", -1 }, { "vco2.fine", 0.3f },
            { "vca1.gain", 0.45f }, { "vcf1.cut", 260 }, { "vcf1.res", 0.6f }, { "vcf1.cv2", 0.6f },
            { "lfo1.rate", 0.007f }, { "lfo1.shape", 3 }, { "lfo2.rate", 0.03f },
            { "drn.oct", -2 }, { "drn.chord", 1 }, { "drn.drift", 0.9f }, { "dream.level", 0.35f }, { "dream.chord", 5 },
            { "hall.size", 0.98f }, { "hall.mix", 0.6f }, { "hall.damp", 0.7f }, { "tape.wow", 0.4f }, { "out.lvl1", 0.55f } } },
        { "Weird", "Clockwork Jellyfish",
          SEQ_CORE "vco1.pls>mix.in1;vco2.saw>mix.in2;lfo2.out>lfo1.rate;lfo1.out>vco1.pwm;clk.div4>env2.gate;"
          "env2.out>vcf2.cv1;clk.div4>grain.trig;" DRONE_OUT,
          { { "clk.div", 4 }, { "arp.follow", 1 }, { "arp.len", 7 }, { "arp.density", 0.75f }, { "arp.mutate", 0.08f },
            { "arp.gate", 0.25f }, { "env1.d", 0.15f }, { "vcf1.res", 0.7f },
            { "env2.a", 0.005f }, { "env2.d", 0.6f }, { "env2.s", 0.0f }, { "env2.r", 0.8f },
            { "vcf2.res", 0.85f }, { "vcf2.cut", 400 }, { "vcf2.cv1", 0.7f }, { "drn.chord", 1 },
            { "lfo1.rate", 0.4f }, { "lfo2.rate", 0.05f }, { "vco1.pwm", 0.8f },
            { "grain.level", 0.35f }, { "grain.pitch", 24 }, { "grain.size", 0.04f }, { "grain.density", 1 },
            { "dream.chord", 5 }, { "dream.level", 0.3f }, { "echo.div", 4 }, { "echo.mix", 0.35f }, { "hall.mix", 0.5f } } },

        //======================================================================  CHORDS + TEXTURES: drone types, the chord layer, built-in grain sounds
        { "Chords + textures", "Choir Of Rooms", DRONE_OUT "lfo1.out>vcf2.cv1",
          { { "drn.type", 4 }, { "drn.wave", 0.55f }, { "drn.breath", 0.45f }, { "drn.oct", -1 }, { "drn.chord", 1 }, { "drn.level", 0.6f },
            { "vcf2.cut", 3200 }, { "vcf2.res", 0.1f }, { "vcf2.cv1", 0.2f }, { "lfo1.rate", 0.02f },
            { "chd.on", 1 }, { "chd.mode", 0 }, { "chd.level", 0.35f }, { "chd.voices", 4 }, { "chd.wave", 0.08f }, { "chd.tone", 0.45f },
            { "chd.attack", 3.0f }, { "chd.release", 6.0f }, { "chd.width", 0.8f },
            { "prog.mode", 1 }, { "prog.bars", 3 }, { "prog.glide", 0.55f }, { "arp.on", 0 }, { "out.lvl1", 0 },
            { "grain.src", 3 }, { "grain.level", 0.3f }, { "grain.size", 0.9f }, { "grain.density", 4 }, { "grain.spray", 0.6f },
            { "dream.level", 0.25f }, { "hall.size", 0.95f }, { "hall.mix", 0.6f }, { "hall.shimmer", 0.35f }, { "out.ens", 0.6f } } },
        { "Chords + textures", "Organ Sunday", DRONE_OUT,
          { { "drn.type", 1 }, { "drn.wave", 0.6f }, { "drn.oct", -2 }, { "drn.chord", 2 }, { "drn.level", 0.55f }, { "drn.breath", 0.2f },
            { "vcf2.cut", 4000 }, { "vcf2.res", 0.05f },
            { "chd.on", 1 }, { "chd.mode", 1 }, { "chd.level", 0.4f }, { "chd.voices", 5 }, { "chd.wave", 0.25f }, { "chd.tone", 0.5f },
            { "chd.attack", 1.6f }, { "chd.release", 4.0f },
            { "prog.mode", 2 }, { "prog.bars", 2 }, { "prog.glide", 0.3f }, { "arp.on", 0 }, { "out.lvl1", 0 },
            { "arp.scale", 6 }, { "arp.root", 41 }, { "dream.chord", 0 }, { "dream.level", 0.2f },
            { "grain.src", 5 }, { "grain.level", 0.25f }, { "grain.pitch", 12 }, { "grain.size", 0.7f }, { "grain.density", 5 },
            { "hall.size", 0.9f }, { "hall.mix", 0.55f }, { "tape.wow", 0.1f } } },
        { "Chords + textures", "Glass Weather",
          SEQ_CORE "vco1.pls>mix.in1;vco2.saw>mix.in2;lfo2.out>vco1.pwm;" DRONE_OUT,
          { { "drn.type", 3 }, { "drn.wave", 0.45f }, { "drn.level", 0.35f }, { "drn.chord", 2 }, { "vcf2.cut", 5000 }, { "vcf2.res", 0.0f },
            { "chd.on", 1 }, { "chd.mode", 4 }, { "chd.level", 0.45f }, { "chd.voices", 5 }, { "chd.wave", 0.15f }, { "chd.tone", 0.7f },
            { "chd.attack", 0.004f }, { "chd.release", 3.0f }, { "chd.width", 0.9f },
            { "prog.mode", 3 }, { "prog.bars", 1 }, { "arp.follow", 1 }, { "arp.range", 3 }, { "arp.density", 0.55f }, { "arp.gate", 0.15f },
            { "clk.div", 3 }, { "vco1.oct", 1 }, { "env1.d", 0.15f }, { "vcf1.cut", 1200 }, { "vcf1.cv2", 0.0f }, { "out.lvl1", 0.45f },
            { "grain.src", 4 }, { "grain.level", 0.3f }, { "grain.pitch", 12 }, { "grain.size", 0.25f }, { "grain.density", 6 }, { "grain.spray", 0.8f },
            { "dream.level", 0.2f }, { "echo.div", 2 }, { "echo.fb", 0.55f }, { "echo.mix", 0.35f }, { "hall.mix", 0.5f }, { "hall.shimmer", 0.45f } } },
        { "Chords + textures", "String Machine Dawn",
          SEQ_CORE "vco1.saw>mix.in1;vco2.sub>mix.in2;lfo1.out>vcf1.cv2;" DRONE_OUT,
          { { "drn.type", 2 }, { "drn.wave", 0.75f }, { "drn.oct", -1 }, { "drn.chord", 1 }, { "drn.level", 0.45f }, { "vcf2.cut", 2200 },
            { "chd.on", 1 }, { "chd.mode", 2 }, { "chd.level", 0.3f }, { "chd.voices", 4 }, { "chd.wave", 0.85f }, { "chd.tone", 0.35f },
            { "chd.attack", 0.02f }, { "chd.release", 0.45f },
            { "prog.mode", 1 }, { "prog.bars", 2 }, { "arp.follow", 0 }, { "arp.len", 8 }, { "arp.mutate", 0.05f }, { "arp.density", 0.9f },
            { "clk.div", 3 }, { "vco2.oct", -1 }, { "vcf1.cut", 320 }, { "vcf1.res", 0.7f }, { "vcf1.cv2", 0.6f }, { "lfo1.rate", 0.015f },
            { "env1.d", 0.2f }, { "out.lvl1", 0.55f },
            { "grain.src", 6 }, { "grain.level", 0.3f }, { "grain.size", 0.8f }, { "grain.density", 4 },
            { "tape.wow", 0.35f }, { "tape.age", 0.45f }, { "tape.hiss", 0.25f }, { "out.ens", 0.75f },
            { "echo.div", 2 }, { "echo.mix", 0.3f }, { "hall.mix", 0.45f } } },
        { "Chords + textures", "Rain Stabs",
          SEQ_CORE "vco1.pls>mix.in1;vco2.saw>mix.in2;arp.gate>grain.trig;" DRONE_OUT,
          { { "drn.type", 3 }, { "drn.level", 0.25f }, { "drn.wave", 0.2f }, { "vcf2.cut", 3000 },
            { "chd.on", 1 }, { "chd.mode", 3 }, { "chd.level", 0.4f }, { "chd.voices", 3 }, { "chd.wave", 0.5f }, { "chd.tone", 0.6f },
            { "chd.attack", 0.003f }, { "chd.release", 0.35f }, { "chd.oct", -1 },
            { "prog.mode", 2 }, { "prog.bars", 1 }, { "arp.follow", 1 }, { "arp.density", 0.6f }, { "arp.gate", 0.12f }, { "clk.div", 3 },
            { "env1.d", 0.12f }, { "vcf1.cut", 900 }, { "out.lvl1", 0.35f },
            { "grain.src", 7 }, { "grain.level", 0.45f }, { "grain.size", 0.35f }, { "grain.density", 9 }, { "grain.spray", 0.9f },
            { "echo.div", 1 }, { "echo.fb", 0.5f }, { "echo.mix", 0.35f }, { "hall.mix", 0.45f } } },
        { "Chords + textures", "Bowed Iron Creep", DRONE_OUT "lfo1.out>vcf2.cv1;lfo2.out>vcf2.cv2",
          { { "prog.mode", 4 }, { "prog.bars", 2 }, { "prog.glide", 0.7f }, { "arp.on", 0 }, { "out.lvl1", 0 }, { "arp.scale", 2 }, { "arp.root", 38 },
            { "drn.type", 0 }, { "drn.chord", 5 }, { "drn.oct", -2 }, { "drn.detune", 0.7f }, { "drn.level", 0.5f }, { "drn.air", 0.3f },
            { "vcf2.cut", 420 }, { "vcf2.res", 0.6f }, { "vcf2.cv1", 0.5f }, { "lfo1.rate", 0.03f }, { "lfo2.shape", 3 },
            { "chd.on", 1 }, { "chd.mode", 0 }, { "chd.level", 0.35f }, { "chd.voices", 6 }, { "chd.oct", -1 }, { "chd.wave", 0.9f },
            { "chd.tone", 0.25f }, { "chd.attack", 4.0f }, { "chd.release", 8.0f },
            { "grain.src", 1 }, { "grain.level", 0.45f }, { "grain.pitch", -12 }, { "grain.reverse", 0.7f }, { "grain.size", 1.0f },
            { "dream.level", 0.15f }, { "dream.warp", 0.8f }, { "hall.size", 0.98f }, { "hall.mix", 0.6f }, { "echo.div", 5 }, { "echo.fb", 0.7f } } },

        //======================================================================  HANDS: the camera plays it
        { "Hands (camera)", "Hand Garden", nullptr,
          { { "cam.on", 1 }, { "play.on", 1 }, { "play.depth", 0.85f }, { "dream.level", 0.7f }, { "grain.level", 0.5f },
            { "tape.wow", 0.5f }, { "tape.age", 0.5f }, { "hall.shimmer", 0.4f }, { "out.lvl1", 0.3f } } },
        { "Hands (camera)", "Hand Weather",
          "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.saw>mix.in1;vco2.pls>mix.in2;"
          "mix.out>vcf1.in;arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;cam.y>vcf1.cv2;"
          "lfo2.out>vco2.pwm;vcf1.out>vca1.in;vca1.out>out.in1;drn.out>vcf2.in;cam.x>vcf2.cv1;"
          "cam.trig>env2.gate;env2.out>vcf2.cv2;vcf2.out>out.in2",
          { { "cam.on", 1 }, { "vcf1.cv2", 0.6f }, { "vcf2.cv1", 0.55f }, { "vcf2.cv2", 0.5f },
            { "env2.a", 0.05f }, { "env2.d", 2.5f }, { "env2.s", 0.0f }, { "env2.r", 3.0f } },
          "hall.mix<cam.y@0.45;echo.fb<cam.mot@0.35;arp.mutate<cam.mot@0.5" },
        { "Hands (camera)", "Puppet Strings",
          "cam.trig>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.saw>mix.in1;vco2.pls>mix.in2;mix.out>vcf1.in;"
          "arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;cam.spread>vcf1.cv2;vcf1.out>vca1.in;vca1.out>out.in1;"
          "drn.out>vcf2.in;cam.fist>vcf2.cv1;vcf2.out>out.in2;cam.tilt>grain.pitch;cam.y>grain.pos",
          { { "cam.on", 1 }, { "arp.density", 1.0f }, { "arp.follow", 1 }, { "arp.mutate", 0.5f }, { "arp.range", 3 },
            { "env1.d", 0.6f }, { "env1.r", 1.2f }, { "vcf1.cv2", 0.6f }, { "vcf2.cv1", -0.5f },
            { "hall.mix", 0.55f }, { "echo.mix", 0.4f }, { "grain.level", 0.4f } },
          "hall.shimmer<cam.pinch@0.6;tape.wow<cam.mot@0.4" },
        { "Hands (camera)", "Theremin Fog",
          "cam.y>vco1.pitch;cam.y>vco2.pitch;vco1.saw>mix.in1;vco2.saw>mix.in2;mix.out>vcf1.in;cam.spread>vcf1.cv1;"
          "vcf1.out>vca1.in;cam.pinch>vca1.cv;vca1.out>out.in1;cam.tilt>vco2.fm;" DRONE_OUT "lfo1.out>vcf2.cv1",
          { { "cam.on", 1 }, { "cam.slew", 0.45f }, { "arp.on", 0 }, { "vco1.oct", 1 }, { "vco2.oct", 1 }, { "vco2.fine", 0.15f },
            { "vco2.fm", 0.4f }, { "vcf1.cut", 600 }, { "vcf1.cv1", 0.7f }, { "vcf1.res", 0.4f },
            { "vca1.gain", 0.0f }, { "vca1.cv", 1.0f }, { "dream.level", 0.4f }, { "dream.chord", 3 },
            { "hall.mix", 0.65f }, { "hall.shimmer", 0.45f }, { "echo.mix", 0.35f }, { "echo.div", 4 } } },
    };
    return list;
}
#undef SEQ_CORE
#undef DRONE_OUT
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
    bo ("arp.hold", "Arp Hold", false);
    in ("arp.oct", "Arp Octave", -2, 2, 0);
    fl ("arp.glide", "Arp Glide", NRange (0.0f, 1.0f), 0.0f);
    ch ("prog.mode", "Chord Progression", progNames, 0);
    ch ("prog.bars", "Chord Change Every", progBarNames, 3);
    fl ("prog.glide", "Chord Glide", NRange (0.0f, 1.0f), 0.2f);
    ch ("scene.key", "New Scene Key", juce::StringArray { "KEEP", "NEAR", "ANY" }, 1);
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
    fl ("drn.tone", "Drone Tone", NRange (0.0f, 1.0f), 1.0f);
    fl ("drn.wave", "Drone Colour", NRange (0.0f, 1.0f), 0.0f);
    fl ("drn.breath", "Drone Breath", NRange (0.0f, 1.0f), 0.0f);
    fl ("drn.air", "Drone Air", NRange (0.0f, 1.0f), 0.0f);
    fl ("drn.sub", "Drone Sub", NRange (0.0f, 1.0f), 0.0f);
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
    ch ("dream.chord", "Dream Chord", dreamChordNames, 1);
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
    bo ("cam.hands", "Hand Skeleton", true);
    fl ("cam.sens", "Camera Sensitivity", NRange (0.0f, 1.0f), 0.5f);
    fl ("cam.slew", "Camera Smoothing", NRange (0.0f, 1.0f), 0.3f);
    // Drone types, GRAIN sounds and the CHORDS layer (kept at the end so older sets load cleanly)
    ch ("drn.type", "Drone Type", droneTypeNames, 0);
    ch ("grain.src", "Grain Sound", grainSourceNames(), 0);
    bo ("grain.key", "Grain In Key", true);
    bo ("chd.on", "Chords On", false);
    ch ("chd.mode", "Chords Style", chordStyleNames, 0);
    fl ("chd.level", "Chords Level", NRange (0.0f, 1.0f), 0.45f);
    in ("chd.voices", "Chords Voices", 3, 6, 4);
    in ("chd.oct", "Chords Octave", -2, 1, 0);
    fl ("chd.wave", "Chords Colour", NRange (0.0f, 1.0f), 0.3f);
    fl ("chd.tone", "Chords Tone", NRange (0.0f, 1.0f), 0.55f);
    fl ("chd.attack", "Chords Attack", skewed (0.002f, 8.0f, 0.6f), 0.9f);
    fl ("chd.release", "Chords Release", skewed (0.02f, 12.0f, 1.5f), 2.5f);
    fl ("chd.width", "Chords Width", NRange (0.0f, 1.0f), 0.6f);

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
        stepNote[(size_t) i].store ((int) ((arpRng.next() * 0.5f + 0.5f) * 9.0f));
        arp.rnd[i] = arpRng.next() * 0.5f + 0.5f;
        stepOn[(size_t) i].store (i == 0 || arp.rnd[i] < 0.82f);
        arp.gate[i] = true;
    }
    for (int i = 0; i < 6; ++i)
    {
        droneOsc[i].phase = (droneRng.next() * 0.5 + 0.5);
        droneBreathPh[i] = droneRng.next() * 0.5f + 0.5f;
    }
    loadSlots();

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            paramIndexMap[rp->getParameterID().toStdString()] = (int) paramList.size();
            paramList.push_back (rp);
            mods.push_back (std::make_unique<ModSlot>());
        }
    effective.resize (paramList.size(), 0.0f);
    for (auto& c : chordNotes) c.store (0);
    syncGrainSource();
    apvts.addParameterListener ("grain.src", this);

    cameraPoller = std::make_unique<CameraPoller> (*this);
}

int DriftwerkProcessor::paramIndex (const juce::String& id) const
{
    const auto it = paramIndexMap.find (id.toStdString());
    return it == paramIndexMap.end() ? -1 : it->second;
}

//==============================================================================
juce::StringArray DriftwerkProcessor::grainSourceNames()
{
    juce::StringArray names;
    for (auto* n : dw::builtInSoundNames) names.add (n);
    names.add ("MY FILE");
    return names;
}

juce::String DriftwerkProcessor::getSampleName() const
{
    const int src = (int) std::lround (apvts.getRawParameterValue ("grain.src")->load());
    if (src == myFileSource && userSample != nullptr) return juce::File (samplePath).getFileName();
    if (src == myFileSource) return "no file loaded yet: drop one here (playing glass choir)";
    return "built-in: " + grainSourceNames()[juce::jlimit (0, myFileSource - 1, src)].toLowerCase();
}

void DriftwerkProcessor::syncGrainSource()
{
    const int src = juce::jlimit (0, myFileSource, (int) std::lround (apvts.getRawParameterValue ("grain.src")->load()));
    SampleData* want = nullptr;
    if (src == myFileSource) want = userSample;
    if (want == nullptr)
    {
        const int b = src == myFileSource ? 0 : src;
        if (builtIns[b] == nullptr) builtIns[b] = dw::makeBuiltInSound (b);   // made once, then kept
        want = builtIns[b].get();
    }
    if (currentSample.load() != want) currentSample.store (want, std::memory_order_release);
}

bool DriftwerkProcessor::loadSample (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) return false;

    const int maxLen = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 90.0));
    if (maxLen < 256) return false;
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
    // Find its pitch so IN KEY can move it into Timewind's key.
    sd->rootMidi = dw::detectRootMidi (*sd);

    userSample = sd.get();
    sampleStore.push_back (std::move (sd));
    // Free user sounds older than the previous one; the audio thread only ever holds the current or previous.
    while (sampleStore.size() > 3) sampleStore.erase (sampleStore.begin());
    samplePath = f.getFullPathName();
    syncGrainSource();
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
    // Key: KEEP stays put, NEAR moves to a closely related key (up a fifth / down a fifth / same),
    // ANY jumps anywhere. The scale family follows so arp, pads and drone agree.
    const int keyMode = (int) std::lround (apvts.getRawParameterValue ("scene.key")->load());
    const int curRoot = (int) std::lround (apvts.getRawParameterValue ("arp.root")->load());
    const int curScale = (int) std::lround (apvts.getRawParameterValue ("arp.scale")->load());
    const bool curMajor = curScale == 6 || curScale == 7;
    bool major = r.nextBool();
    if (keyMode == 2)
    {
        const int roots[] { 33, 35, 36, 38, 40, 41, 43, 45 };
        set ("arp.root", (float) roots[r.nextInt (8)]);
    }
    else
    {
        major = curMajor;
        if (keyMode == 1)
        {
            const int moves[] { 0, 0, 7, -5, 5, -7 };
            int nr = curRoot + moves[r.nextInt (6)];
            while (nr > 45) nr -= 12;
            while (nr < 31) nr += 12;
            set ("arp.root", (float) nr);
        }
    }
    if (! major)
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
    if (r.nextFloat() < 0.4f) set ("drn.type", (float) r.nextInt (5));
    if (apvts.getRawParameterValue ("arp.hold")->load() < 0.5f)
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
    apvts.removeParameterListener ("grain.src", this);
    cancelPendingUpdate();
    stopRecording();
    cameraPoller = nullptr;
    camera.close();
}

void DriftwerkProcessor::CameraPoller::timerCallback()
{
    // In hand-play mode a sudden gesture rolls a new random scene (at most every 2 s).
    const int tc = owner.camera.waveCount.load();   // a quick wave (not a pinch) rolls a scene
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
    const bool bones = owner.apvts.getRawParameterValue ("cam.hands")->load() > 0.5f;
    owner.camera.hand.enabled.store (bones);
    if (bones && owner.camera.isOpen() && ! owner.camera.hand.isReady()) owner.camera.hand.start();
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
    chordLayer.prepare ((float) sr);
    chdLastKey = -1;
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

    const bool held = p.hold || recordNotes.load (std::memory_order_relaxed);
    if (! held && u() < p.mutate)
    {
        int d;
        if (u() < p.walk)
        {
            const int moves[] { -2, -1, -1, 1, 1, 2, 0, n };
            d = stepNote[(size_t) prevStep].load() + moves[(int) (u() * 7.999f)];
            if (d < 0) d = -d;
            if (d > maxDeg) d = 2 * maxDeg - d;
        }
        else
            d = (int) (u() * (float) (maxDeg + 1));
        stepNote[(size_t) arp.step].store (juce::jlimit (0, maxDeg, d));
        arp.rnd[arp.step] = u();
    }

    // A note played on the keyboard while recording lands on this step.
    if (pendingRecStep == arp.step) { stepNote[(size_t) arp.step].store (pendingRecNote); stepOn[(size_t) arp.step].store (true); pendingRecStep = -1; }

    const int deg = juce::jmin (stepNote[(size_t) arp.step].load(), maxDeg);
    // Free: step 0 always plays (an anchor), the others follow DENSITY. Held: the steps decide.
    bool on;
    if (held) on = stepOn[(size_t) arp.step].load();
    else { on = arp.step == 0 || arp.rnd[arp.step] < p.density; stepOn[(size_t) arp.step].store (on); }
    const bool play = p.arpOn && on;

    if (play)
    {
        arp.pitchOut = (float) (scaleSemitone (deg) + (p.arpFollow != 0 ? chordOffsetTarget : 0)) / 12.0f;
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
    // Hand skeleton: pinch sparkles (shimmer, glassy grains), an open spread widens the space,
    // a fist hushes the sequence and darkens, tilting bends the grains.
    nudge ("hall.shimmer", 0.5f * camPinch); nudge ("grain.density", 0.35f * camPinch);
    nudge ("hall.size", 0.2f * camSpread);   nudge ("out.ens", 0.3f * camSpread); nudge ("dream.level", 0.15f * camSpread);
    nudge ("arp.density", -0.6f * camFist);  nudge ("vcf1.cut", -0.25f * camFist); nudge ("echo.mix", 0.2f * camFist);
    nudge ("grain.scan", 0.3f * camTilt);
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
    p.hold = f ("arp.hold") > 0.5f; p.arpOct = i ("arp.oct"); p.glide = f ("arp.glide");
    p.droneTone = f ("drn.tone"); p.droneWave = f ("drn.wave"); p.droneBreath = f ("drn.breath");
    p.droneAir = f ("drn.air"); p.droneSub = f ("drn.sub"); p.droneType = i ("drn.type");
    p.grainKey = f ("grain.key") > 0.5f;
    p.chdOn = f ("chd.on") > 0.5f; p.chdMode = i ("chd.mode"); p.chdVoices = i ("chd.voices"); p.chdOct = i ("chd.oct");
    p.chdLevel = f ("chd.level"); p.chdWave = f ("chd.wave"); p.chdTone = f ("chd.tone");
    p.chdAttack = f ("chd.attack"); p.chdRelease = f ("chd.release"); p.chdWidth = f ("chd.width");
    p.progMode = i ("prog.mode"); p.progBars = progBars[juce::jlimit (0, 4, i ("prog.bars"))]; p.progGlide = f ("prog.glide");
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
// Chord progression: four chords, each held for prog.bars bars. Run every sample so the
// pads, drone, arp and CHORDS all change on the same sample, right on the bar line.
void DriftwerkProcessor::applyProgression (double beat)
{
    if (p.progMode > 0)
    {
        const int family = (p.scale == 6 || p.scale == 7) ? 1 : 0;
        const int idx = (int) std::floor (beat / (4.0 * p.progBars)) & 3;
        const auto& c = progTable[p.progMode][family][idx];
        chordOffsetTarget = c.offset > 6 ? c.offset - 12 : c.offset;   // keep roots close: move down rather than far up
        p.dreamChord = c.dream;
        p.droneChord = c.drone;
        if (progIndex.load (std::memory_order_relaxed) != idx) progIndex.store (idx, std::memory_order_relaxed);
    }
    else
    {
        chordOffsetTarget = 0;
        if (progIndex.load (std::memory_order_relaxed) != -1) progIndex.store (-1, std::memory_order_relaxed);
    }
}

//==============================================================================
// CHORDS layer: plays the same chord as the pads (and the arp in PADS mode), voice-led.
// With the arp on SCALE, chord tones outside the scale are left out, so it can never clash.
void DriftwerkProcessor::updateChordLayer()
{
    const int keyRoot = p.root + midiTranspose;
    const int dc = juce::jlimit (0, 5, p.dreamChord);
    const int key = (((((((keyRoot + 64) * 32 + chordOffsetTarget + 12) * 8 + dc) * 8 + p.chdVoices) * 8 + (p.chdOct + 4)) * 16
                     + p.scale) * 4 + p.arpFollow);
    bool changed = false;
    if (key != chdLastKey)
    {
        chdLastKey = key;
        changed = true;
        const float root = (float) keyRoot + p.tune + (float) chordOffsetTarget;
        const auto& sc = scales[(size_t) juce::jlimit (0, (int) scales.size() - 1, p.scale)];
        float notes[6];
        int n = 0;
        for (int k = 0; k < 6; ++k)
        {
            const int iv = dreamChords[dc][k];
            const int pc = ((chordOffsetTarget + iv) % 12 + 12) % 12;
            const bool inScale = std::find (sc.begin(), sc.end(), pc) != sc.end();
            if (k == 0 || p.arpFollow != 0 || inScale) notes[n++] = root + (float) iv;
        }
        const float centre = (float) keyRoot + p.tune + 24.0f + 12.0f * (float) p.chdOct;
        chordLayer.setChord (notes, n, juce::jlimit (3, 6, p.chdVoices), centre);
        for (int i = 0; i < dw::ChordLayer::V; ++i)
            chordNotes[(size_t) i].store (i < chordLayer.numTargets ? (int) std::lround (chordLayer.targets[i] - p.tune) : 0, std::memory_order_relaxed);
    }
    chordNoteCount.store (p.chdOn ? chordLayer.numTargets : 0, std::memory_order_relaxed);

    if (! p.chdOn)
    {
        if (chdWasOn) chordLayer.hold (false);
        chdWasOn = false;
        chdLastBeat = -1;
        return;
    }
    const int mode = juce::jlimit (0, 4, p.chdMode);
    if (mode != chdLastMode || ! chdWasOn)
    {
        chordLayer.hold (false);
        chdLastBeat = -1;
        chdLastMode = mode;
        changed = true;
    }
    chdWasOn = true;

    const double spb = samplesPerBeat;
    auto unitChanged = [this] (double beatsPerUnit)
    {
        const long long u = (long long) std::floor (curBeat / beatsPerUnit);
        if (u == chdLastBeat) return false;
        chdLastBeat = u;
        return true;
    };
    switch (mode)
    {
        case 0: chordLayer.hold (true); break;                                                    // PAD
        case 1: if (unitChanged (8.0) || changed) chordLayer.strike ((int) (spb * 5.0), 0); break; // SWELL every 2 bars
        case 2: if (unitChanged (1.0) || changed) chordLayer.strike ((int) (spb * 0.5), 0); break; // PULSE on the beat
        case 3:                                                                                   // ARP HITS
        {
            const bool g = arp.gateOut;
            if (g && ! chdLastArpGate) chordLayer.strike (juce::jmax (64, arp.gateLeft), 0);
            chdLastArpGate = g;
            break;
        }
        default: if (unitChanged (4.0) || changed) chordLayer.strike ((int) spb, (int) (sr * 0.045)); break; // STRUM
    }
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
    curBeat = beat;
    applyProgression (beat);
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
        arpGlided += (arp.pitchOut - arpGlided) * (p.glide <= 0.001f ? 1.0f : onePoleCoef (0.004f + p.glide * p.glide * 0.6f, fsr));
        outs[O_ARP_PITCH] = arpGlided + (float) p.arpOct + in (I_ARP_TRANS);
        outs[O_ARP_GATE] = arp.gateOut ? 1.0f : 0.0f;
        outs[O_ARP_RND] = arp.rndOut;
    }

    const float baseNote = (float) (p.root + midiTranspose) + p.tune;
    // Chord progression: pads and drone glide to the new chord root.
    chordOffsetNow += ((float) chordOffsetTarget - chordOffsetNow) * chordGlideCoef;
    const float chordBase = baseNote + chordOffsetNow;

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
        const float dronePitch = chordBase + 12.0f * ((float) p.droneOct + in (I_DRONE_PITCH));
        const int* chord = chords[juce::jlimit (0, 5, p.droneChord)];
        const int type = juce::jlimit (0, 4, p.droneType);
        const float c = p.droneWave;                                          // COLOUR, read differently by each type
        float sum = 0.0f, breathAvg = 0.0f;
        for (int k = 0; k < 6; ++k)
        {
            droneDrift[k] += (droneDriftTarget[k] - droneDrift[k]) * driftCoef;
            const float cents = detuneCents[k] * p.droneDetune * 22.0f + droneDrift[k] * p.droneDriftAmt * 18.0f;
            float note = dronePitch + (float) chord[k] + cents * 0.01f;
            // Each voice breathes on its own slow cycle (8 to 20 seconds).
            droneBreathPh[k] += (0.05f + 0.016f * (float) k) / fsr;
            if (droneBreathPh[k] >= 1.0f) droneBreathPh[k] -= 1.0f;
            const float br = 1.0f - p.droneBreath * (0.5f + 0.5f * std::sin (kTwoPi * droneBreathPh[k]));
            breathAvg += br;

            float v = 0.0f;
            if (type == 0)
            {   // SAWS: bright saw > soft triangle
                const float saw = droneOsc[k].process (BlepOsc::Saw, midiToHz (note), sr);
                const float ph = (float) droneOsc[k].phase;
                const float tri = 1.0f - 4.0f * std::abs (ph - 0.5f);
                v = saw + (tri - saw) * c;
            }
            else if (type == 2)
            {   // STRINGS: two saws per voice with a slow chorus, like an old string machine.
                // COLOUR widens the ensemble.
                droneVibPh[k] += (0.37f + 0.09f * (float) k) / fsr; if (droneVibPh[k] >= 1.0f) droneVibPh[k] -= 1.0f;
                const float spread = (4.0f + 14.0f * c) * (1.0f + 0.35f * std::sin (kTwoPi * droneVibPh[k]));
                v = 0.68f * (droneOsc[k].process (BlepOsc::Saw, midiToHz (note + spread * 0.01f), sr)
                           + droneOsc2[k].process (BlepOsc::Saw, midiToHz (note - spread * 0.01f), sr));
            }
            else
            {
                if (type == 4)
                {   // CHOIR: a gentle vibrato on each voice
                    droneVibPh[k] += (4.7f + 0.23f * (float) k) / fsr; if (droneVibPh[k] >= 1.0f) droneVibPh[k] -= 1.0f;
                    note += 0.09f * std::sin (kTwoPi * droneVibPh[k]);
                    v = droneOsc[k].process (BlepOsc::Saw, midiToHz (note), sr);
                }
                else
                {
                    auto& ph = droneOsc[k].phase;
                    ph += midiToHz (note) / sr; if (ph >= 1.0) ph -= 1.0;
                    const float x = kTwoPi * (float) ph;
                    if (type == 1)
                    {   // ORGAN: drawbars. COLOUR pulls out the upper ones.
                        v = 0.8f * (std::sin (x) + 0.65f * std::sin (2.0f * x)
                                    + c * (0.5f * std::sin (3.0f * x) + 0.4f * std::sin (4.0f * x) + 0.22f * std::sin (6.0f * x) + 0.18f * std::sin (8.0f * x)));
                    }
                    else
                    {   // GLASS: FM with whole-number ratios (so it stays in tune), shimmering with the breath.
                        const float index = (0.25f + 1.6f * c) * (0.75f + 0.25f * std::sin (kTwoPi * droneBreathPh[k] * 3.0f));
                        v = 0.85f * (std::sin (x + index * std::sin (3.0f * x)) + 0.3f * std::sin (2.0f * x));
                    }
                }
            }
            sum += v * br;
        }
        if (type == 4)
        {   // Vowel formants on the whole choir: COLOUR moves oo > oh > ah > eh, breath sways it.
            if (droneFormCounter-- <= 0)
            {
                droneFormCounter = 32;
                float fm[3];
                dw::vowelFormants (c + 0.08f * p.droneBreath * std::sin (kTwoPi * droneBreathPh[0]), fm);
                for (int k = 0; k < 3; ++k) droneForm[k].set (fm[k], 7.0f, fsr);
            }
            float lp;
            sum = 1.9f * (droneForm[0].process (sum, lp) + 0.55f * droneForm[1].process (sum, lp) + 0.25f * droneForm[2].process (sum, lp)) + 0.06f * sum;
        }
        breathAvg /= 6.0f;
        // Sub: a sine an octave below the root.
        droneSubPh += midiToHz (dronePitch - 12.0f) / fsr;
        if (droneSubPh >= 1.0f) droneSubPh -= 1.0f;
        sum += p.droneSub * 2.2f * std::sin (kTwoPi * droneSubPh);
        // Tone: a gentle low-pass, wide open at the top of the knob.
        const float cutHz = 60.0f * std::exp2 (p.droneTone * 8.2f);
        droneLp += (sum - droneLp) * (p.droneTone > 0.995f ? 1.0f : onePoleCoef (1.0f / (kTwoPi * cutHz), fsr));
        // Air: breathy band of noise that swells with the voices.
        const float nz = droneRng.next();
        droneAirLp += (nz - droneAirLp) * 0.35f;
        droneAirHp += (droneAirLp - droneAirHp) * 0.02f;
        const float air = (droneAirLp - droneAirHp) * p.droneAir * 1.6f * breathAvg;
        outs[O_DRONE] = (droneLp + air) * 0.2f * p.droneLevel;
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
        // Hand skeleton gestures (hold their last shape when the hand leaves, except pinch).
        const bool hp = camera.hand.present.load (std::memory_order_relaxed);
        camPinch  += ((hp ? camera.hand.pinch.load (std::memory_order_relaxed) : 0.0f) - camPinch) * coef;
        camSpread += (camera.hand.spread.load (std::memory_order_relaxed) - camSpread) * coef;
        camFist   += (camera.hand.fist.load (std::memory_order_relaxed) - camFist) * coef;
        camTilt   += (camera.hand.tilt.load (std::memory_order_relaxed) - camTilt) * coef;
        outs[O_CAM_PINCH] = camPinch;
        outs[O_CAM_SPREAD] = camSpread;
        outs[O_CAM_FIST] = camFist;
        outs[O_CAM_TILT] = camTilt;
    }

    // ---- DREAM + GRAIN (stereo, normalled straight to the master bus unless their OUT is patched)
    float dl = 0.0f, dr = 0.0f, gl = 0.0f, gr = 0.0f;
    {
        const float dm = dream.process (dreamChords[juce::jlimit (0, 5, p.dreamChord)], 6, chordBase,
                                        p.dreamRate, p.dreamTone, p.dreamWarp, dl, dr);
        outs[O_DREAM] = dm * p.dreamLevel * 1.6f;
        dl *= p.dreamLevel; dr *= p.dreamLevel;

        const bool trig = in (I_GRAIN_TRIG) > 0.5f;
        const float gpos = p.gPos + in (I_GRAIN_POS) * 0.5f;
        const auto* sd = currentSample.load (std::memory_order_acquire);
        float gp = p.gPitch + 12.0f * in (I_GRAIN_PITCH);
        if (p.grainKey)
        {
            // IN KEY: PITCH snaps to octaves (and fifths, when the scale has the notes for it),
            // and the sound's own pitch is moved onto the current chord root.
            static const float withFifths[] { -24, -17, -12, -5, 0, 7, 12, 19, 24 };
            static const float octaves[] { -24, -12, 0, 12, 24, 24, 24, 24, 24 };
            const bool fifths = p.scale == 0 || p.scale == 1 || p.scale == 4 || p.scale == 6 || p.scale == 7;
            const float* steps = fifths ? withFifths : octaves;
            float best = steps[0];
            for (int k = 1; k < 9; ++k) if (std::abs (steps[k] - gp) < std::abs (best - gp)) best = steps[k];
            gp = best;
            if (sd != nullptr && sd->rootMidi > 0.0f)
            {
                float d = baseNote - sd->rootMidi;
                d -= 12.0f * std::round (d / 12.0f);
                gp += d + chordOffsetNow;
            }
        }
        const float gm = grains.process (sd, gpos - std::floor (gpos), p.gSpray, p.gSize, p.gDensity, gp, p.gRev, p.gScan,
                                         trig && ! grainLastTrig, gl, gr);
        grainLastTrig = trig;
        outs[O_GRAIN] = gm * p.gLevel;
        gl *= p.gLevel; gr *= p.gLevel;
    }
    // ---- CHORDS layer (stereo, straight to the master bus)
    float cl = 0.0f, cr = 0.0f;
    updateChordLayer();
    chordLayer.process (chordGlideCoef, p.chdWave, p.chdTone, p.chdAttack, p.chdRelease, p.chdWidth, cl, cr);
    cl *= p.chdLevel * 0.9f; cr *= p.chdLevel * 0.9f;

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
    l += cl; r += cr;

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

    // ---- Chord progression (changes exactly on bar lines, see applyProgression)
    {
        const float glideSec = (0.03f + p.progGlide * p.progGlide * 5.0f) * (p.progMode == 4 ? 2.0f : 1.0f);
        chordGlideCoef = onePoleCoef (glideSec / 4.0f, (float) sr);
        applyProgression (hostSynced ? hostPpqAtBlock : beatPos);
        progChord.store (p.dreamChord);
        progOffset.store (chordOffsetTarget);
    }

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOn()) continue;
        if (recordNotes.load (std::memory_order_relaxed))
        {
            // Record: the note goes into the sequence, on this step or the next (whichever is closer).
            const int rel = m.getNoteNumber() - p.root - (p.arpFollow != 0 ? chordOffsetTarget : 0);
            int set[12];
            const int nset = arpPitchSet (set);
            int best = 0, bestErr = 1000;
            for (int d = 0; d <= nset * 4; ++d)
            {
                const int e = std::abs (set[d % nset] + 12 * (d / nset) - rel);
                if (e < bestErr) { bestErr = e; best = d; }
            }
            const int len = juce::jlimit (1, 16, p.length);
            const bool early = arp.samplesSinceClock < arp.clockPeriod / 2;
            const int stepIdx = early ? arp.step : (arp.step + 1) % len;
            if (early)
            {
                stepNote[(size_t) stepIdx].store (best);
                stepOn[(size_t) stepIdx].store (true);
                arp.pitchOut = (float) (scaleSemitone (best) + (p.arpFollow != 0 ? chordOffsetTarget : 0)) / 12.0f;
                arp.gateOut = true;
                arp.gateLeft = juce::jmax (8, (int) ((float) arp.clockPeriod * p.gateLen));
            }
            else { pendingRecStep = stepIdx; pendingRecNote = best; }
        }
        else
            midiTranspose = juce::jlimit (-36, 36, m.getNoteNumber() - 60);
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

    // ---- WAV recording (the writer thread does the disk work)
    {
        const juce::ScopedLock sl (writerLock);
        if (auto* w = activeWriter.load())
        {
            w->write (buffer.getArrayOfReadPointers(), n);
            recordedSamples.store (recordedSamples.load() + n);
        }
    }
    midi.clear();
}

//==============================================================================
int DriftwerkProcessor::getNumPresets() { return (int) presets().size(); }

juce::String DriftwerkProcessor::getPresetName (int index)
{
    if (index >= 0 && index < getNumPresets()) return presets()[(size_t) index].name;
    return {};
}

juce::String DriftwerkProcessor::getPresetGroup (int index)
{
    if (index >= 0 && index < getNumPresets()) return presets()[(size_t) index].group;
    return {};
}

void DriftwerkProcessor::loadPreset (int index)
{
    if (index < 0 || index >= getNumPresets()) return;
    currentProgram = index;
    const auto& pr = presets()[(size_t) index];

    // Work out every parameter's final value first (preset value, else default), then send
    // each one to the host once, as a complete gesture, and only if it actually changes.
    std::map<juce::String, float> target;
    for (auto& [id, value] : pr.values)
        if (auto* rp = apvts.getParameter (id))
            target[id] = rp->convertTo0to1 (value);
    // Your own sound stays loaded in GRAIN when you change presets.
    if (auto* src = apvts.getParameter ("grain.src"))
        if ((int) std::lround (apvts.getRawParameterValue ("grain.src")->load()) == myFileSource && userSample != nullptr)
            target["grain.src"] = src->getValue();

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto it = target.find (rp->getParameterID());
            const float v = it != target.end() ? it->second : rp->getDefaultValue();
            if (std::abs (rp->getValue() - v) < 1.0e-6f) continue;
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (v);
            rp->endChangeGesture();
        }

    setPatchFromString (pr.patch != nullptr ? pr.patch : defaultPatch);
    setModFromString (pr.mods != nullptr ? pr.mods : "");
    if (juce::MessageManager::existsAndIsCurrentThread()) syncGrainSource();
}

void DriftwerkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("patch", getPatchString(), nullptr);
    state.setProperty ("mods", getModString(), nullptr);
    state.setProperty ("sample", samplePath, nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", getPresetName (currentProgram), nullptr);
    state.setProperty ("seq", getSequenceString(), nullptr);
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
            if (sp.isNotEmpty() && juce::File (sp).existsAsFile())
            {
                loadSample (juce::File (sp));
                // Sets saved before GRAIN had a sound menu: they were playing this file.
                if (! state.getChildWithProperty ("id", "grain.src").isValid())
                    if (auto* src = apvts.getParameter ("grain.src"))
                        src->setValueNotifyingHost (src->convertTo0to1 ((float) myFileSource));
            }
            if (juce::MessageManager::existsAndIsCurrentThread()) syncGrainSource();
            else triggerAsyncUpdate();
            currentProgram = state.getProperty ("program", 0);
            const juce::String pn = state.getProperty ("presetName", {}).toString();
            for (int i = 0; i < getNumPresets(); ++i)
                if (pn.isNotEmpty() && getPresetName (i) == pn) currentProgram = i;
            currentProgram = juce::jlimit (0, getNumPresets() - 1, currentProgram);
            const juce::String seq = state.getProperty ("seq", {}).toString();
            if (seq.isNotEmpty()) setSequenceString (seq);
        }
    }
}

//==============================================================================
// Step sequence
int DriftwerkProcessor::pitchSetFromParams (int* out) const
{
    // Same as arpPitchSet but from the parameters, so the editor can call it.
    auto iv = [this] (const char* id) { return (int) std::lround (apvts.getRawParameterValue (id)->load()); };
    const int follow = iv ("arp.follow");
    if (follow == 1)
    {
        const int chord = progIndex.load() >= 0 ? progChord.load() : iv ("dream.chord");
        bool has[12] {};
        for (int k = 0; k < 6; ++k) has[dreamChords[juce::jlimit (0, 5, chord)][k] % 12] = true;
        int n = 0;
        for (int k = 0; k < 12; ++k) if (has[k]) out[n++] = k;
        return n;
    }
    if (follow == 2) { out[0] = 0; out[1] = 7; return 2; }
    const auto& sc = scales[(size_t) juce::jlimit (0, (int) scales.size() - 1, iv ("arp.scale"))];
    for (size_t k = 0; k < sc.size(); ++k) out[k] = sc[k];
    return (int) sc.size();
}

juce::String DriftwerkProcessor::progressionChordName (int k) const
{
    auto iv = [this] (const char* id) { return (int) std::lround (apvts.getRawParameterValue (id)->load()); };
    const int mode = juce::jlimit (0, 4, iv ("prog.mode"));
    const int sc = iv ("arp.scale");
    const auto& c = progTable[mode][(sc == 6 || sc == 7) ? 1 : 0][k & 3];
    const auto name = juce::MidiMessage::getMidiNoteName (iv ("arp.root") + c.offset, true, false, 3);
    return name + " " + dreamChordNames[c.dream].toLowerCase();
}

int DriftwerkProcessor::maxDegree() const
{
    int set[12];
    return pitchSetFromParams (set) * (int) std::lround (apvts.getRawParameterValue ("arp.range")->load());
}

juce::String DriftwerkProcessor::degreeName (int degree) const
{
    int set[12];
    const int n = pitchSetFromParams (set);
    degree = juce::jmax (0, degree);
    const int follow = (int) std::lround (apvts.getRawParameterValue ("arp.follow")->load());
    const int note = (int) std::lround (apvts.getRawParameterValue ("arp.root")->load()) + set[degree % n] + 12 * (degree / n)
                   + (follow != 0 ? progOffset.load() : 0) + (int) std::lround (apvts.getRawParameterValue ("arp.oct")->load()) * 12;
    return juce::MidiMessage::getMidiNoteName (note, true, true, 3);
}

void DriftwerkProcessor::setStep (int i, int degree, bool on)
{
    if (i < 0 || i >= numSteps) return;
    stepNote[(size_t) i].store (juce::jlimit (0, 63, degree));
    stepOn[(size_t) i].store (on);
    if (auto* h = apvts.getParameter ("arp.hold"); h != nullptr && h->getValue() < 0.5f)
    {
        h->beginChangeGesture(); h->setValueNotifyingHost (1.0f); h->endChangeGesture();
    }
}

juce::String DriftwerkProcessor::getSequenceString() const
{
    juce::StringArray notes, ons;
    for (int i = 0; i < numSteps; ++i) { notes.add (juce::String (stepNote[(size_t) i].load())); ons.add (stepOn[(size_t) i].load() ? "1" : "0"); }
    const int len = (int) std::lround (apvts.getRawParameterValue ("arp.len")->load());
    return juce::String (len) + "|" + notes.joinIntoString (",") + "|" + ons.joinIntoString (",");
}

void DriftwerkProcessor::setSequenceString (const juce::String& s)
{
    juce::StringArray parts;
    parts.addTokens (s, "|", "");
    if (parts.size() < 3) return;
    juce::StringArray notes, ons;
    notes.addTokens (parts[1], ",", ""); ons.addTokens (parts[2], ",", "");
    for (int i = 0; i < numSteps; ++i)
    {
        if (i < notes.size()) stepNote[(size_t) i].store (juce::jlimit (0, 63, notes[i].getIntValue()));
        if (i < ons.size()) stepOn[(size_t) i].store (ons[i].getIntValue() != 0);
    }
    if (auto* lp = apvts.getParameter ("arp.len"))
    {
        lp->beginChangeGesture();
        lp->setValueNotifyingHost (lp->convertTo0to1 ((float) juce::jlimit (1, 16, parts[0].getIntValue())));
        lp->endChangeGesture();
    }
}

void DriftwerkProcessor::newRandomSequence()
{
    juce::Random r;
    const int maxDeg = juce::jmax (1, maxDegree());
    int d = r.nextInt (maxDeg / 2 + 1);
    for (int i = 0; i < numSteps; ++i)
    {
        d = juce::jlimit (0, maxDeg, d + r.nextInt (5) - 2);           // mostly small steps: singable lines
        stepNote[(size_t) i].store (d);
        stepOn[(size_t) i].store (i == 0 || r.nextFloat() < 0.7f);
    }
}

juce::File DriftwerkProcessor::slotsFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Timewind").getChildFile ("sequences.txt");
}

void DriftwerkProcessor::loadSlots()
{
    slots.clear();
    juce::StringArray lines;
    slotsFile().readLines (lines);
    for (int i = 0; i < numSlots; ++i) slots.add (i < lines.size() ? lines[i].trim() : juce::String());
}

void DriftwerkProcessor::saveSlots() const
{
    auto f = slotsFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (slots.joinIntoString ("\n"));
}

void DriftwerkProcessor::storeSlot (int slot)
{
    if (slot < 0 || slot >= numSlots) return;
    loadSlots();                                    // pick up slots saved by another Timewind
    slots.set (slot, getSequenceString());
    saveSlots();
}

bool DriftwerkProcessor::recallSlot (int slot)
{
    loadSlots();
    if (! slotUsed (slot)) return false;
    setSequenceString (slots[slot]);
    if (auto* h = apvts.getParameter ("arp.hold"))
    {
        h->beginChangeGesture(); h->setValueNotifyingHost (1.0f); h->endChangeGesture();
    }
    return true;
}

void DriftwerkProcessor::setRecordNotes (bool on)
{
    recordNotes.store (on);
    if (! on)
        if (auto* h = apvts.getParameter ("arp.hold"))
        {
            h->beginChangeGesture(); h->setValueNotifyingHost (1.0f); h->endChangeGesture();   // keep what you played
        }
}

//==============================================================================
// WAV recording
bool DriftwerkProcessor::startRecording()
{
    stopRecording();
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Timewind");
    dir.createDirectory();
    auto file = dir.getNonexistentChildFile ("Timewind " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S"), ".wav", false);
    std::unique_ptr<juce::OutputStream> os (file.createOutputStream());
    if (os == nullptr) return false;
    juce::WavAudioFormat wav;
    auto* writer = wav.createWriterFor (os.get(), sr, 2, 24, {}, 0);
    if (writer == nullptr) return false;
    os.release();                                  // the writer owns the stream now
    writerThread.startThread();
    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer, writerThread, 32768);
    recordedSamples.store (0.0);
    lastRecording = file;
    const juce::ScopedLock sl (writerLock);
    activeWriter.store (threadedWriter.get());
    return true;
}

void DriftwerkProcessor::stopRecording()
{
    {
        const juce::ScopedLock sl (writerLock);
        activeWriter.store (nullptr);
    }
    threadedWriter.reset();                        // flushes the rest to disk
}

juce::AudioProcessorEditor* DriftwerkProcessor::createEditor() { return new DriftwerkEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DriftwerkProcessor(); }
