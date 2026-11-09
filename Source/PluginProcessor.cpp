#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <limits>

using namespace dw;

namespace
{
const juce::StringArray clockDivNames { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
const double clockDivBeats[] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

const juce::StringArray echoDivNames { "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2", "FREE" };
const double echoDivBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 0.0 };

// Scales, chord voicings and progressions live in Harmony.h (shared with the editor).
using harm::scaleNames; using harm::scales; using harm::dreamChords; using harm::dreamChordNames;
using harm::progNames; using harm::progBarNames; using harm::progBars;
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

// Drone TYPE: what each of the six drone voices is made of. COLOUR (drn.wave) shapes each type.
const juce::StringArray droneTypeNames { "SAWS", "ORGAN", "STRINGS", "GLASS", "CHOIR" };
// CHORDS layer styles.
const juce::StringArray chordStyleNames { "PAD", "SWELL", "PULSE", "ARP HITS", "STRUM", "STEPS" };
constexpr int chordStepsStyle = 5;
// HITS pattern: how long each step is.
const juce::StringArray hitRateNames { "1/16", "1/8", "1/4", "1/2", "1 BAR" };
const double hitRateBeats[] { 0.25, 0.5, 1.0, 2.0, 4.0 };
// A gentle default pattern: at DENSITY 0.5 over 8 steps it plays 3 + 3 + 2.
const float defaultHitWeights[16] { 0.0f, 0.9f, 0.7f, 0.15f, 0.55f, 0.85f, 0.3f, 0.95f, 0.4f, 0.8f, 0.65f, 0.2f, 0.6f, 0.9f, 0.35f, 0.75f };
const juce::StringArray loopNoteNames { "CHORD", "SCALE", "ROOTS" };
const juce::StringArray slimeVoiceNames { "DROPS", "BREATH", "TINES", "MIXED", "GLASS", "PLUCK", "MARIMBA", "VOICE", "BOWED",
                                          "BUBBLES", "CHIRPS", "GRAIN SOUND", "RANDOM" };
constexpr int slimeMixed = 3, slimeRandom = 12;
const juce::StringArray pulseStyleNames { "HEARTBEAT", "DRIFT", "LAZY", "RAIN", "MOTORIK", "ORGANISM",
                                          "SHUFFLE", "DUB", "TRIBAL", "BOSSA", "BROKEN", "BREAK", "JUNGLE", "DRILL" };
constexpr int pulseOrganismStyle = 5, pulseDrillStyle = 13, pulseNumStyles = 14;
const juce::StringArray pulseKitNames { "SOFT", "DEEP", "DUSTY", "GLASS", "HANDS", "GLITCH", "IDM" };
// LENGTH: every lane the same, or lanes of different lengths that drift against each other.
const juce::StringArray pulseLengthNames { "16", "12", "8", "6", "POLY", "WILD" };
const int pulseLengths[6][5] { { 16, 16, 16, 16, 16 }, { 12, 12, 12, 12, 12 }, { 8, 8, 8, 8, 8 }, { 6, 6, 6, 6, 6 },
                               { 16, 16, 12, 14, 10 }, { 16, 15, 13, 11, 7 } };
const juce::StringArray pulseSpeedNames { "1/16", "1/8", "1/4" };
const juce::StringArray texNames { "RAIN", "WIND", "STREAM", "CRACKLE", "CREATURES", "FIRE", "SHORE" };
const juce::StringArray texNamesB { "OFF", "RAIN", "WIND", "STREAM", "CRACKLE", "CREATURES", "FIRE", "SHORE" };
const juce::StringArray arpWaveNames { "PATCH", "SINE", "TRIANGLE", "SAW", "SQUARE", "WAVETABLE" };
const juce::StringArray wordsSourceNames { "MY RECORDINGS", "SYNTH VOICE" };
const juce::StringArray memSpeedNames { "1x", "1/2", "1/4", "REVERSE", "REV 1/2", "WANDER" };
constexpr long long pulseNoStep = std::numeric_limits<long long>::min();
constexpr float breakGain = 0.6f;   // BREAK LEVEL 1 sits about where PULSE LEVEL 1 does
constexpr float bassGain = 0.3f;   // BASS LEVEL 0.6 sits a few dB under a full scene, about where the drums do
// PULSE styles: how likely each lane (low, snare, wood, hat, tick) is to play on each step.
// ORGANISM (5) has no pattern: the slime plays the drums.
const float pulseStyleW[pulseNumStyles][5][16] {
    {   // HEARTBEAT: lub-dub, a brush now and then
        { 1, 0, .85f, 0, 0, 0, 0, 0, .5f, 0, .42f, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, .2f, 0, 0, 0, 0, 0, .35f, 0, 0, .15f },
        { 0, 0, 0, 0, .25f, 0, 0, 0, 0, 0, 0, 0, 0, 0, .2f, 0 },
        { .1f, 0, 0, .1f, .45f, 0, .12f, 0, .1f, 0, 0, .15f, .45f, 0, .12f, .1f },
        { .2f, .08f, .08f, .08f, .1f, .08f, .15f, .08f, .12f, .08f, .08f, .08f, .1f, .08f, .15f, .08f } },
    {   // DRIFT: loose and sparse
        { .7f, 0, 0, 0, 0, 0, .15f, 0, 0, 0, .3f, 0, 0, 0, 0, 0 },
        { 0, .1f, 0, .2f, 0, .1f, 0, 0, .3f, 0, 0, .15f, 0, .1f, .2f, 0 },
        { .15f, 0, .3f, 0, .1f, 0, 0, .25f, 0, .15f, 0, 0, .3f, 0, .1f, 0 },
        { .2f, .15f, .3f, .15f, .2f, .15f, .3f, .15f, .2f, .15f, .3f, .15f, .2f, .15f, .3f, .15f },
        { .25f, .2f, .25f, .2f, .25f, .2f, .25f, .2f, .25f, .2f, .25f, .2f, .25f, .2f, .25f, .2f } },
    {   // LAZY: a slow, slightly broken beat
        { 1, 0, 0, .3f, 0, 0, 0, .55f, 0, 0, .75f, 0, 0, 0, 0, .2f },
        { 0, 0, 0, 0, .95f, 0, 0, 0, 0, 0, 0, 0, .95f, 0, 0, .25f },
        { 0, 0, 0, 0, 0, 0, .3f, 0, 0, 0, 0, 0, 0, 0, .25f, 0 },
        { .75f, .2f, .75f, .2f, .75f, .2f, .75f, .2f, .75f, .2f, .75f, .2f, .75f, .2f, .75f, .3f },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, .25f, 0, 0, 0, 0, 0, 0 } },
    {   // RAIN: no beat, just wood, brush and ticks falling
        { .25f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, .12f, 0, 0, .1f, 0, 0, 0, 0, .12f, 0, 0, 0, 0, .1f },
        { .35f, .2f, .3f, .25f, .35f, .2f, .3f, .25f, .35f, .2f, .3f, .25f, .35f, .2f, .3f, .25f },
        { .6f, .45f, .6f, .45f, .6f, .45f, .6f, .45f, .6f, .45f, .6f, .45f, .6f, .45f, .6f, .45f },
        { .55f, .5f, .55f, .5f, .55f, .5f, .55f, .5f, .55f, .5f, .55f, .5f, .55f, .5f, .55f, .5f } },
    {   // MOTORIK: a soft steady pulse
        { .95f, 0, 0, 0, .9f, 0, 0, 0, .95f, 0, 0, 0, .9f, 0, 0, 0 },
        { 0, 0, 0, 0, .3f, 0, 0, 0, 0, 0, 0, 0, .35f, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, .35f, 0 },
        { .3f, 0, .9f, 0, .3f, 0, .9f, 0, .3f, 0, .9f, 0, .3f, 0, .9f, .2f },
        { .3f, .3f, .3f, .3f, .3f, .3f, .3f, .3f, .3f, .3f, .3f, .3f, .3f, .3f, .3f, .3f } },
    {   // ORGANISM: no pattern
        {}, {}, {}, {}, {} },
    {   // SHUFFLE: a backbeat with a lilt (turn SWING up)
        { 1, 0, 0, 0, 0, 0, 0, .3f, 0, 0, .85f, 0, 0, 0, 0, .2f },
        { 0, 0, 0, 0, .95f, 0, 0, .15f, 0, 0, 0, 0, .95f, 0, 0, .2f },
        { 0, 0, 0, 0, 0, 0, .25f, 0, 0, 0, 0, 0, 0, 0, 0, .3f },
        { .9f, 0, .6f, .35f, .9f, 0, .6f, .35f, .9f, 0, .6f, .35f, .9f, 0, .6f, .4f },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, .12f, 0 } },
    {   // DUB: one drop on the third beat, rims around it, hats on the offbeat
        { .15f, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, .9f, 0, 0, 0, 0, 0, 0, .12f },
        { 0, 0, 0, .4f, 0, 0, .45f, 0, 0, 0, 0, .4f, 0, 0, .5f, 0 },
        { 0, 0, .85f, 0, 0, 0, .85f, 0, 0, 0, .85f, 0, 0, 0, .85f, .3f },
        { .15f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, .1f, 0, 0, 0 } },
    {   // TRIBAL: hand drums, three against four
        { 1, 0, 0, .35f, 0, 0, .7f, 0, 0, .4f, 0, 0, .75f, 0, 0, 0 },
        { 0, 0, .6f, 0, .5f, 0, 0, .55f, 0, 0, .6f, 0, 0, .45f, 0, .5f },
        { .7f, 0, 0, .7f, 0, 0, .7f, 0, 0, .7f, 0, 0, .7f, 0, 0, .6f },
        { .7f, .35f, .5f, .35f, .7f, .35f, .5f, .35f, .7f, .35f, .5f, .35f, .7f, .35f, .5f, .4f },
        { 0, 0, 0, 0, .55f, 0, 0, 0, 0, 0, 0, 0, .55f, 0, 0, .2f } },
    {   // BOSSA: a rocking low drum and a clave on the rim
        { .95f, 0, 0, .55f, .8f, 0, 0, .55f, .95f, 0, 0, .55f, .8f, 0, 0, .55f },
        { 0, 0, 0, 0, 0, 0, .15f, 0, 0, 0, 0, 0, 0, 0, .15f, 0 },
        { .85f, 0, 0, .8f, 0, 0, .8f, 0, 0, 0, .8f, 0, .8f, 0, 0, 0 },
        { .7f, .4f, .7f, .4f, .7f, .4f, .7f, .4f, .7f, .4f, .7f, .4f, .7f, .4f, .7f, .4f },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, .1f, 0, 0, 0, 0, 0, 0 } },
    {   // BROKEN: a broken beat that never quite sits square
        { 1, 0, 0, 0, 0, 0, .65f, 0, 0, 0, .8f, 0, 0, .4f, 0, 0 },
        { 0, 0, 0, 0, .95f, 0, 0, .3f, 0, .2f, 0, 0, .9f, 0, 0, 0 },
        { 0, 0, .25f, 0, 0, 0, 0, 0, 0, 0, 0, .3f, 0, 0, .35f, 0 },
        { .8f, 0, .55f, .4f, 0, .5f, .7f, 0, .75f, .3f, 0, .6f, .5f, 0, .65f, .4f },
        { 0, .2f, 0, 0, 0, 0, 0, .25f, 0, 0, 0, 0, 0, .2f, 0, 0 } },
    {   // BREAK: a funky breakbeat with ghost snares
        { 1, 0, .55f, 0, 0, 0, 0, 0, 0, 0, .9f, .6f, 0, 0, 0, 0 },
        { 0, 0, 0, 0, .95f, 0, 0, .35f, 0, .35f, 0, 0, .95f, 0, .3f, .4f },
        { 0, 0, 0, 0, 0, 0, .15f, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { .9f, .2f, .85f, .2f, .9f, .2f, .85f, .2f, .9f, .2f, .85f, .2f, .9f, .2f, .85f, .3f },
        { .15f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    {   // JUNGLE: a fast break, snares tumbling (try STEP 1/16 and some CHOP)
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, .9f, 0, 0, 0, 0, .25f },
        { 0, 0, 0, 0, .95f, 0, 0, .55f, 0, .5f, 0, 0, .95f, 0, .45f, .5f },
        { 0, 0, 0, .2f, 0, 0, 0, 0, 0, 0, 0, 0, 0, .2f, 0, 0 },
        { .8f, .55f, .8f, .55f, .8f, .55f, .8f, .55f, .8f, .55f, .8f, .55f, .8f, .55f, .8f, .6f },
        { 0, 0, 0, 0, 0, 0, 0, .12f, 0, 0, 0, 0, 0, 0, 0, .12f } },
    {   // DRILL: skittering and restless, rolls everywhere (try the IDM kit)
        { .9f, 0, 0, .4f, 0, 0, .5f, 0, 0, .45f, 0, .3f, 0, 0, .5f, 0 },
        { 0, .25f, 0, 0, .85f, 0, .35f, .3f, 0, .4f, 0, .35f, .85f, .3f, .45f, .55f },
        { .3f, 0, .35f, 0, .25f, .3f, 0, .35f, .3f, 0, .3f, 0, .25f, 0, .4f, .3f },
        { .7f, .5f, .6f, .5f, .7f, .55f, .6f, .5f, .7f, .5f, .6f, .55f, .7f, .5f, .6f, .5f },
        { .3f, .25f, 0, .3f, 0, .25f, .3f, 0, .3f, 0, .25f, .3f, 0, .3f, 0, .25f } } };
const char* const slotDegIds[] { "prog.d1", "prog.d2", "prog.d3", "prog.d4", "prog.d5", "prog.d6", "prog.d7", "prog.d8" };
const char* const slotTypeIds[] { "prog.t1", "prog.t2", "prog.t3", "prog.t4", "prog.t5", "prog.t6", "prog.t7", "prog.t8" };
const char* const slotLenIds[] { "prog.b1", "prog.b2", "prog.b3", "prog.b4", "prog.b5", "prog.b6", "prog.b7", "prog.b8" };

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
            { "dream.chord", 2 }, { "dream.rate", 0.45f }, { "dream.tone", 0.2f }, { "dream.warp", 0.15f }, { "dream.level", 0.8f }, { "dream.sound", 9 },
            { "grain.level", 0.3f }, { "grain.size", 1.1f }, { "grain.density", 3 }, { "grain.pitch", 12 }, { "grain.spray", 0.6f },
            { "hall.size", 0.97f }, { "hall.mix", 0.62f }, { "hall.shimmer", 0.55f }, { "out.ens", 0.7f },
            { "tape.wow", 0.12f }, { "tape.hiss", 0.08f }, { "echo.mix", 0.15f } } },
        { "Soundbath", "Low Tide Breathing", "drn.out>vcf2.in;vcf2.out>vca2.in;vca2.out>out.in2;lfo1.out>vca2.cv;lfo2.out>vcf2.cv1",
          { { "prog.mode", 1 }, { "prog.bars", 3 }, { "prog.glide", 0.4f }, { "drn.breath", 0.5f }, { "drn.air", 0.35f }, { "drn.wave", 0.5f }, { "arp.on", 0 }, { "out.lvl1", 0 },
            { "drn.chord", 1 }, { "drn.oct", -2 }, { "drn.detune", 0.6f }, { "vca2.gain", 0.25f }, { "vca2.cv", 0.75f },
            { "lfo1.rate", 0.08f }, { "lfo1.shape", 0 }, { "lfo2.rate", 0.01f }, { "lfo2.shape", 3 },
            { "vcf2.cut", 320 }, { "vcf2.res", 0.4f }, { "vcf2.cv1", 0.4f },
            { "dream.chord", 1 }, { "dream.rate", 0.3f }, { "dream.level", 0.5f }, { "dream.sound", 11 }, { "dream.tone", 0.15f },
            { "tape.hiss", 0.32f }, { "tape.wow", 0.2f }, { "tape.age", 0.35f },
            { "grain.level", 0.15f }, { "grain.pitch", -12 }, { "grain.size", 0.9f }, { "hall.mix", 0.55f }, { "hall.size", 0.92f } } },
        { "Soundbath", "Cathedral Ghosts", DRONE_OUT "lfo1.out>vcf2.cv1",
          { { "prog.mode", 3 }, { "prog.bars", 3 }, { "prog.glide", 0.3f }, { "arp.on", 0 }, { "out.lvl1", 0 }, { "arp.scale", 7 },
            { "drn.chord", 2 }, { "drn.level", 0.4f },
            { "grain.reverse", 0.9f }, { "grain.pitch", -12 }, { "grain.size", 1.2f }, { "grain.density", 6 },
            { "grain.spray", 0.8f }, { "grain.level", 0.6f }, { "grain.scan", 0.02f },
            { "dream.chord", 3 }, { "dream.level", 0.6f }, { "dream.sound", 2 }, { "dream.rate", 0.5f },
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
            { "echo.div", 2 }, { "echo.fb", 0.62f }, { "echo.mix", 0.4f }, { "hall.mix", 0.45f },
            { "bass.on", 1 }, { "bass.mode", 2 }, { "bass.cutoff", 320 }, { "bass.reso", 0.45f }, { "bass.env", 0.5f }, { "bass.level", 0.42f } } },
        { "Plucks + sequences", "Glass Harp",
          SEQ_CORE "vco1.pls>mix.in1;vco2.saw>mix.in2;lfo2.out>vco1.pwm;" DRONE_OUT,
          { { "prog.mode", 1 }, { "prog.bars", 2 }, { "clk.div", 3 }, { "arp.follow", 1 }, { "arp.scale", 6 }, { "arp.root", 48 }, { "arp.range", 3 },
            { "arp.density", 0.7f }, { "arp.mutate", 0.15f }, { "arp.walk", 0.7f }, { "arp.gate", 0.15f },
            { "env1.a", 0.001f }, { "env1.d", 0.12f }, { "env1.r", 0.15f },
            { "vcf1.cut", 900 }, { "vcf1.res", 0.75f }, { "vcf1.cv1", 0.75f }, { "vcf1.cv2", 0.0f }, { "vco2.oct", 1 },
            { "dream.chord", 4 }, { "dream.level", 0.35f }, { "dream.sound", 9 }, { "drn.chord", 2 }, { "drn.level", 0.3f },
            { "echo.div", 2 }, { "echo.fb", 0.6f }, { "echo.mix", 0.45f }, { "hall.mix", 0.45f }, { "hall.shimmer", 0.35f } } },
        { "Plucks + sequences", "Kalimba Rain",
          SEQ_CORE "vco1.saw>mix.in1;vco2.sub>vco1.fm;arp.gate>grain.trig;" DRONE_OUT,
          { { "clk.div", 4 }, { "arp.follow", 0 }, { "arp.scale", 3 }, { "arp.root", 45 }, { "arp.range", 4 },
            { "arp.density", 0.45f }, { "arp.mutate", 0.3f }, { "arp.walk", 0.4f }, { "arp.gate", 0.1f },
            { "vco1.fm", 0.35f }, { "vco2.oct", 1 }, { "env1.d", 0.18f }, { "env1.r", 0.25f },
            { "vcf1.cut", 1400 }, { "vcf1.res", 0.35f }, { "vcf1.cv2", 0.0f },
            { "grain.src", 4 }, { "grain.level", 0.3f }, { "grain.pitch", 12 }, { "grain.size", 0.05f }, { "grain.density", 1 }, { "grain.spray", 0.9f },
            { "dream.chord", 1 }, { "dream.level", 0.3f }, { "dream.sound", 7 }, { "drn.level", 0.3f }, { "drn.chord", 1 },
            { "echo.div", 1 }, { "echo.fb", 0.5f }, { "echo.mix", 0.35f }, { "hall.mix", 0.5f } } },
        { "Plucks + sequences", "Toy Piano Ghost",
          SEQ_CORE "vco1.pls>mix.in1;vco2.saw>mix.in2;" DRONE_OUT,
          { { "clk.div", 1 }, { "arp.follow", 1 }, { "arp.scale", 6 }, { "arp.root", 48 }, { "arp.density", 0.6f },
            { "arp.gate", 0.2f }, { "vco1.oct", 1 }, { "vco2.oct", 2 }, { "vco2.fine", 0.05f }, { "vco1.pw", 0.3f },
            { "env1.d", 0.25f }, { "vcf1.cut", 2200 }, { "vcf1.res", 0.2f }, { "vcf1.cv2", 0.0f },
            { "tape.wow", 0.6f }, { "tape.age", 0.6f }, { "tape.crush", 0.2f }, { "tape.hiss", 0.3f },
            { "grain.src", 2 }, { "grain.level", 0.3f }, { "grain.size", 0.5f },
            { "dream.chord", 0 }, { "dream.warp", 0.7f }, { "dream.level", 0.45f }, { "dream.sound", 6 }, { "drn.level", 0.25f }, { "drn.chord", 2 },
            { "echo.div", 3 }, { "echo.mix", 0.3f }, { "hall.mix", 0.5f } } },
        { "Plucks + sequences", "Black Tide",
          "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.sub>mix.in1;vco2.pls>mix.in2;"
          "mix.out>vcf1.in;arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;lfo2.out>vcf1.cv2;"
          "vcf1.out>vca1.in;vca1.out>out.in1;drn.out>vcf2.in;lfo1.out>vcf2.cv1;env2.out>vcf2.cv2;"
          "clk.div4>env2.gate;vcf2.out>out.in2",
          { { "clk.div", 1 }, { "arp.root", 31 }, { "arp.scale", 2 }, { "arp.follow", 0 }, { "arp.density", 0.6f }, { "dream.chord", 5 },
            { "drn.chord", 5 }, { "drn.oct", -2 }, { "drn.detune", 0.7f }, { "hall.size", 0.95f }, { "dream.level", 0.2f }, { "dream.sound", 3 },
            { "hall.mix", 0.6f }, { "env2.a", 3 }, { "env2.r", 10 }, { "vcf2.cv2", 0.35f }, { "lfo1.shape", 3 } } },
        { "Plucks + sequences", "Pulse Engine",
          "clk.out>arp.clk;arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;vco1.pls>mix.in1;vco2.saw>mix.in2;"
          "mix.out>vcf1.in;arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;nz.snh>vcf1.cv2;"
          "clk.out>nz.clk;vcf1.out>vca1.in;vca1.out>out.in1;drn.out>vcf2.in;lfo1.out>vcf2.cv1;"
          "lfo2.out>vco1.pwm;vcf2.out>out.in2",
          { { "clk.div", 4 }, { "arp.scale", 2 }, { "arp.follow", 0 }, { "arp.range", 3 }, { "arp.mutate", 0.25f },
            { "vco2.oct", 1 }, { "vcf1.cv2", 0.35f }, { "nz.slew", 0.1f }, { "echo.div", 1 }, { "dream.level", 0.2f }, { "dream.chord", 5 },
            { "drn.level", 0.45f }, { "env1.d", 0.18f },
            { "bass.on", 1 }, { "bass.mode", 0 }, { "bass.wave", 3 }, { "bass.shape", 0.4f }, { "bass.cutoff", 450 }, { "bass.level", 0.45f } } },

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
            { "dream.chord", 3 }, { "dream.level", 0.2f }, { "dream.sound", 2 }, { "drn.level", 0.3f }, { "drn.chord", 4 },
            { "hall.mix", 0.5f }, { "echo.div", 4 }, { "echo.mix", 0.3f } } },
        { "Weird", "Broken Radio Prayer",
          SEQ_CORE "vco1.saw>mix.in1;nz.noise>mix.in3;nz.snh>vcf1.cv2;clk.div4>nz.clk;" DRONE_OUT,
          { { "arp.follow", 2 }, { "arp.density", 0.3f }, { "arp.gate", 0.7f }, { "clk.div", 1 },
            { "mix.l3", 0.25f }, { "vcf1.res", 0.85f }, { "vcf1.cv2", 0.6f }, { "nz.slew", 0.6f },
            { "tape.crush", 0.6f }, { "tape.age", 0.9f }, { "tape.hiss", 0.6f }, { "tape.wow", 0.7f },
            { "grain.scan", -0.4f }, { "grain.pitch", -7 }, { "grain.level", 0.5f }, { "grain.reverse", 0.5f },
            { "dream.warp", 1.0f }, { "dream.level", 0.5f }, { "dream.sound", 10 }, { "dream.chord", 2 },
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
            { "dream.level", 0.25f }, { "dream.sound", 2 }, { "hall.size", 0.95f }, { "hall.mix", 0.6f }, { "hall.shimmer", 0.35f }, { "out.ens", 0.6f } } },
        { "Chords + textures", "Organ Sunday", DRONE_OUT,
          { { "drn.type", 1 }, { "drn.wave", 0.6f }, { "drn.oct", -2 }, { "drn.chord", 2 }, { "drn.level", 0.55f }, { "drn.breath", 0.2f },
            { "vcf2.cut", 4000 }, { "vcf2.res", 0.05f },
            { "chd.on", 1 }, { "chd.mode", 1 }, { "chd.level", 0.4f }, { "chd.voices", 5 }, { "chd.wave", 0.25f }, { "chd.tone", 0.5f },
            { "chd.attack", 1.6f }, { "chd.release", 4.0f },
            { "prog.mode", 2 }, { "prog.bars", 2 }, { "prog.glide", 0.3f }, { "arp.on", 0 }, { "out.lvl1", 0 },
            { "arp.scale", 6 }, { "arp.root", 41 }, { "dream.chord", 0 }, { "dream.level", 0.2f }, { "dream.sound", 5 },
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
            { "dream.level", 0.2f }, { "dream.sound", 8 }, { "echo.div", 2 }, { "echo.fb", 0.55f }, { "echo.mix", 0.35f }, { "hall.mix", 0.5f }, { "hall.shimmer", 0.45f } } },
        { "Chords + textures", "String Machine Dawn",
          SEQ_CORE "vco1.saw>mix.in1;vco2.sub>mix.in2;lfo1.out>vcf1.cv2;" DRONE_OUT,
          { { "dream.sound", 3 }, { "drn.type", 2 }, { "drn.wave", 0.75f }, { "drn.oct", -1 }, { "drn.chord", 1 }, { "drn.level", 0.45f }, { "vcf2.cut", 2200 },
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

        { "Chords + textures", "Wandering Hymn",
          SEQ_CORE "vco1.pls>mix.in1;vco2.saw>mix.in2;lfo2.out>vco1.pwm;" DRONE_OUT "lfo1.out>vcf2.cv1",
          { { "prog.mode", 10 }, { "prog.rhythm", 1 }, { "prog.bars", 1 }, { "prog.glide", 0.45f }, { "arp.scale", 6 }, { "arp.root", 43 },
            { "drn.type", 4 }, { "drn.wave", 0.4f }, { "drn.breath", 0.4f }, { "drn.oct", -1 }, { "drn.level", 0.45f }, { "vcf2.cut", 2600 }, { "vcf2.cv1", 0.2f },
            { "chd.on", 1 }, { "chd.mode", 5 }, { "chd.steps", 8 }, { "chd.rate", 1 }, { "chd.density", 0.45f }, { "chd.gate", 0.9f }, { "chd.vary", 0.3f },
            { "chd.level", 0.4f }, { "chd.voices", 4 }, { "chd.wave", 0.2f }, { "chd.tone", 0.5f }, { "chd.attack", 0.06f }, { "chd.release", 1.2f },
            { "arp.follow", 1 }, { "arp.density", 0.5f }, { "arp.range", 3 }, { "arp.gate", 0.2f }, { "clk.div", 3 }, { "vco1.oct", 1 },
            { "env1.d", 0.2f }, { "vcf1.cut", 1100 }, { "vcf1.cv2", 0.0f }, { "out.lvl1", 0.3f }, { "lfo1.rate", 0.02f },
            { "dream.level", 0.25f }, { "dream.sound", 1 }, { "grain.src", 3 }, { "grain.level", 0.25f }, { "grain.size", 0.8f }, { "grain.density", 4 },
            { "echo.div", 2 }, { "echo.fb", 0.5f }, { "echo.mix", 0.3f }, { "hall.size", 0.9f }, { "hall.mix", 0.55f }, { "hall.shimmer", 0.35f } } },
        { "Chords + textures", "Postcard Loop",
          SEQ_CORE "vco1.saw>mix.in1;vco2.pls>mix.in2;lfo1.out>vcf1.cv2;" DRONE_OUT,
          { { "prog.mode", 11 }, { "prog.len", 3 }, { "prog.bars", 1 }, { "prog.glide", 0.25f }, { "prog.colour", 3 }, { "arp.scale", 0 }, { "arp.root", 40 },
            { "prog.d1", 0 }, { "prog.d2", 5 }, { "prog.d3", 6 }, { "prog.d4", 3 }, { "prog.d5", 4 },
            { "prog.b1", 3 }, { "prog.b2", 2 }, { "prog.b3", 2 }, { "prog.b4", 1 }, { "prog.b5", 1 },
            { "drn.type", 2 }, { "drn.wave", 0.6f }, { "drn.chord", 2 }, { "drn.oct", -2 }, { "drn.level", 0.4f }, { "vcf2.cut", 1800 },
            { "chd.on", 1 }, { "chd.mode", 5 }, { "chd.steps", 16 }, { "chd.rate", 0 }, { "chd.density", 0.35f }, { "chd.gate", 0.5f }, { "chd.vary", 0.1f },
            { "chd.level", 0.32f }, { "chd.voices", 5 }, { "chd.wave", 0.55f }, { "chd.tone", 0.45f }, { "chd.attack", 0.005f }, { "chd.release", 0.6f }, { "chd.oct", -1 },
            { "arp.follow", 1 }, { "arp.density", 0.7f }, { "arp.mutate", 0.08f }, { "arp.gate", 0.3f }, { "clk.div", 3 },
            { "vcf1.cut", 520 }, { "vcf1.res", 0.6f }, { "vcf1.cv2", 0.5f }, { "lfo1.rate", 0.03f }, { "out.lvl1", 0.45f },
            { "tape.wow", 0.45f }, { "tape.age", 0.5f }, { "tape.hiss", 0.22f }, { "dream.level", 0.3f }, { "dream.warp", 0.6f },
            { "grain.src", 6 }, { "grain.level", 0.25f }, { "echo.div", 2 }, { "echo.mix", 0.3f }, { "hall.mix", 0.45f } } },

        //======================================================================  WEIRD + CALM: loops, slime, drums, weather, memory
        { "Weird + calm", "Orbiting Bells", DRONE_OUT "lfo1.out>vcf2.cv1",
          { { "prog.mode", 2 }, { "prog.bars", 2 }, { "prog.glide", 0.4f }, { "arp.on", 0 }, { "out.lvl1", 0 }, { "arp.scale", 7 }, { "arp.root", 38 },
            { "drn.type", 3 }, { "drn.wave", 0.5f }, { "drn.oct", -2 }, { "drn.level", 0.35f }, { "vcf2.cut", 1400 }, { "vcf2.cv1", 0.3f }, { "out.lvl2", 0.45f },
            { "loop.on", 1 }, { "loop.voices", 6 }, { "loop.length", 16.0f }, { "loop.colour", 0.35f }, { "loop.swell", 0.45f }, { "loop.decay", 7.0f },
            { "loop.drift", 0.35f }, { "loop.weird", 0.2f }, { "loop.level", 0.6f },
            { "tex.on", 1 }, { "tex.a", 6 }, { "tex.level", 0.22f }, { "tex.space", 0.5f },
            { "dream.level", 0.12f }, { "dream.sound", 8 }, { "lfo1.rate", 0.02f }, { "hall.size", 0.95f }, { "hall.mix", 0.62f }, { "hall.shimmer", 0.4f } } },
        { "Weird + calm", "Reverse Garden", DRONE_OUT,
          { { "prog.mode", 10 }, { "prog.rhythm", 1 }, { "prog.bars", 2 }, { "prog.glide", 0.5f }, { "arp.on", 0 }, { "out.lvl1", 0 }, { "arp.scale", 1 }, { "arp.root", 40 },
            { "drn.type", 2 }, { "drn.oct", -2 }, { "drn.level", 0.3f }, { "vcf2.cut", 900 },
            { "loop.on", 1 }, { "loop.voices", 5 }, { "loop.length", 12.0f }, { "loop.weird", 0.75f }, { "loop.swell", 0.6f }, { "loop.decay", 5.0f }, { "loop.level", 0.5f },
            { "slime.play", 1 }, { "slime.voice", 3 }, { "slime.activity", 0.4f }, { "slime.weird", 0.6f }, { "slime.level", 0.45f },
            { "tex.on", 1 }, { "tex.a", 4 }, { "tex.key", 0.7f }, { "tex.level", 0.3f },
            { "mem.on", 1 }, { "mem.speed", 4 }, { "mem.ago", 20.0f }, { "mem.length", 6.0f }, { "mem.dissolve", 0.5f }, { "mem.level", 0.45f },
            { "dream.level", 0.15f }, { "hall.size", 0.93f }, { "hall.mix", 0.6f }, { "hall.shimmer", 0.3f } } },
        { "Weird + calm", "Slime Choir", DRONE_OUT "lfo1.out>vcf2.cv1",
          { { "dream.sound", 10 }, { "prog.mode", 3 }, { "prog.bars", 3 }, { "prog.glide", 0.6f }, { "arp.on", 0 }, { "out.lvl1", 0 }, { "arp.scale", 0 }, { "arp.root", 36 },
            { "drn.type", 4 }, { "drn.wave", 0.3f }, { "drn.breath", 0.5f }, { "drn.oct", -1 }, { "drn.level", 0.4f }, { "vcf2.cut", 1600 }, { "vcf2.cv1", 0.25f },
            { "slime.play", 1 }, { "slime.voice", 1 }, { "slime.activity", 0.5f }, { "slime.oct", 0 }, { "slime.weird", 0.35f }, { "slime.level", 0.55f }, { "slime.style", 3 },
            { "grain.src", 12 }, { "grain.level", 0.3f }, { "grain.pitch", -12 }, { "grain.size", 0.9f }, { "grain.density", 5 },
            { "mem.on", 1 }, { "mem.speed", 2 }, { "mem.ago", 40.0f }, { "mem.length", 8.0f }, { "mem.blur", 0.5f }, { "mem.level", 0.4f },
            { "lfo1.rate", 0.015f }, { "hall.size", 0.97f }, { "hall.mix", 0.68f }, { "hall.shimmer", 0.25f } } },
        { "Weird + calm", "Heartbeat Moss", DRONE_OUT,
          { { "prog.mode", 1 }, { "prog.bars", 2 }, { "prog.glide", 0.3f }, { "arp.on", 0 }, { "out.lvl1", 0 }, { "arp.scale", 6 }, { "arp.root", 41 }, { "clk.bpm", 72 },
            { "drn.type", 1 }, { "drn.wave", 0.3f }, { "drn.oct", -2 }, { "drn.level", 0.3f }, { "vcf2.cut", 1100 },
            { "pulse.on", 1 }, { "pulse.style", 0 }, { "pulse.speed", 1 }, { "pulse.level", 0.32f }, { "pulse.space", 0.6f }, { "pulse.vary", 0.25f },
            { "loop.on", 1 }, { "loop.voices", 4 }, { "loop.length", 10.0f }, { "loop.level", 0.4f },
            { "tex.on", 1 }, { "tex.a", 0 }, { "tex.b", 4 }, { "tex.blend", 0.3f }, { "tex.level", 0.3f }, { "tex.density", 0.35f },
            { "chd.on", 1 }, { "chd.mode", 0 }, { "chd.level", 0.25f }, { "chd.voices", 4 }, { "chd.tone", 0.35f }, { "chd.attack", 3.0f }, { "chd.release", 6.0f },
            { "grain.src", 8 }, { "grain.level", 0.2f }, { "hall.mix", 0.55f } } },
        { "Weird + calm", "Lazy Tape Afternoon",
          SEQ_CORE "vco1.saw>mix.in1;vco2.pls>mix.in2;lfo1.out>vcf1.cv2;" DRONE_OUT,
          { { "prog.mode", 5 }, { "prog.bars", 1 }, { "prog.glide", 0.2f }, { "prog.colour", 3 }, { "arp.scale", 6 }, { "arp.root", 38 }, { "clk.bpm", 84 },
            { "pulse.on", 1 }, { "pulse.style", 2 }, { "pulse.speed", 0 }, { "pulse.swing", 0.35f }, { "pulse.human", 0.45f }, { "pulse.tone", 0.35f },
            { "pulse.level", 0.35f }, { "pulse.space", 0.35f }, { "pulse.vary", 0.3f },
            { "chd.on", 1 }, { "chd.mode", 5 }, { "chd.steps", 16 }, { "chd.rate", 0 }, { "chd.density", 0.3f }, { "chd.gate", 0.6f }, { "chd.level", 0.3f },
            { "chd.voices", 4 }, { "chd.wave", 0.6f }, { "chd.tone", 0.4f }, { "chd.attack", 0.01f }, { "chd.release", 0.8f },
            { "arp.follow", 1 }, { "arp.density", 0.6f }, { "arp.gate", 0.25f }, { "clk.div", 3 }, { "vcf1.cut", 600 }, { "vcf1.cv2", 0.4f },
            { "lfo1.rate", 0.03f }, { "out.lvl1", 0.35f }, { "drn.level", 0.3f },
            { "mem.on", 1 }, { "mem.speed", 1 }, { "mem.ago", 12.0f }, { "mem.blur", 0.55f }, { "mem.level", 0.35f },
            { "tex.on", 1 }, { "tex.a", 3 }, { "tex.level", 0.35f },
            { "tape.wow", 0.45f }, { "tape.age", 0.5f }, { "echo.div", 2 }, { "echo.mix", 0.25f }, { "hall.mix", 0.4f } } },
        { "Weird + calm", "Rain on the Organism", DRONE_OUT,
          { { "prog.mode", 9 }, { "prog.bars", 2 }, { "arp.on", 0 }, { "out.lvl1", 0 }, { "arp.scale", 3 }, { "arp.root", 45 },
            { "drn.type", 3 }, { "drn.oct", -2 }, { "drn.level", 0.3f },
            { "slime.play", 1 }, { "slime.voice", 0 }, { "slime.activity", 0.55f }, { "slime.weird", 0.3f }, { "slime.level", 0.5f },
            { "pulse.on", 1 }, { "pulse.style", 5 }, { "pulse.level", 0.28f }, { "pulse.space", 0.7f },
            { "tex.on", 1 }, { "tex.a", 0 }, { "tex.b", 3 }, { "tex.blend", 0.5f }, { "tex.key", 0.8f }, { "tex.level", 0.35f },
            { "mem.on", 1 }, { "mem.speed", 5 }, { "mem.ago", 30.0f }, { "mem.level", 0.35f },
            { "dream.level", 0.15f }, { "hall.size", 0.92f }, { "hall.mix", 0.55f } } },
        { "Weird + calm", "Memory Hall", SEQ_CORE "vco1.pls>mix.in1;vco2.saw>mix.in2;lfo2.out>vco1.pwm;" DRONE_OUT,
          { { "dream.sound", 1 }, { "prog.mode", 2 }, { "prog.bars", 2 }, { "arp.scale", 1 }, { "arp.root", 36 }, { "arp.follow", 1 }, { "arp.density", 0.55f },
            { "env1.d", 0.25f }, { "vcf1.cut", 900 }, { "out.lvl1", 0.35f }, { "drn.level", 0.35f },
            { "mem.on", 1 }, { "mem.speed", 5 }, { "mem.ago", 25.0f }, { "mem.length", 7.0f }, { "mem.scatter", 0.6f }, { "mem.blur", 0.6f },
            { "mem.dissolve", 0.7f }, { "mem.level", 0.75f },
            { "echo.div", 3 }, { "echo.mix", 0.3f }, { "hall.size", 0.96f }, { "hall.mix", 0.6f }, { "hall.shimmer", 0.3f } } },
        { "Weird + calm", "Night Creatures", DRONE_OUT "lfo1.out>vcf2.cv1",
          { { "dream.sound", 4 }, { "prog.mode", 9 }, { "prog.bars", 3 }, { "prog.glide", 0.6f }, { "arp.on", 0 }, { "out.lvl1", 0 }, { "arp.scale", 1 }, { "arp.root", 33 },
            { "drn.type", 2 }, { "drn.oct", -1 }, { "drn.level", 0.35f }, { "vcf2.cut", 700 }, { "vcf2.cv1", 0.35f }, { "lfo1.rate", 0.02f },
            { "tex.on", 1 }, { "tex.a", 4 }, { "tex.b", 2 }, { "tex.blend", 0.35f }, { "tex.key", 0.85f }, { "tex.level", 0.45f }, { "tex.density", 0.6f },
            { "slime.play", 1 }, { "slime.voice", 2 }, { "slime.activity", 0.3f }, { "slime.level", 0.4f },
            { "loop.on", 1 }, { "loop.voices", 3 }, { "loop.oct", -1 }, { "loop.length", 20.0f }, { "loop.level", 0.4f },
            { "hall.size", 0.9f }, { "hall.mix", 0.5f } } },
        { "Weird + calm", "Motorik Dream",
          SEQ_CORE "vco1.saw>mix.in1;vco2.saw>mix.in2;lfo1.out>vcf1.cv2;" DRONE_OUT,
          { { "prog.mode", 7 }, { "prog.bars", 3 }, { "arp.scale", 0 }, { "arp.root", 40 }, { "clk.bpm", 104 }, { "clk.div", 3 },
            { "arp.density", 0.9f }, { "arp.mutate", 0.1f }, { "arp.follow", 1 }, { "vcf1.cut", 500 }, { "vcf1.res", 0.55f }, { "vcf1.cv2", 0.5f },
            { "lfo1.rate", 0.025f }, { "out.lvl1", 0.45f }, { "drn.level", 0.35f },
            { "pulse.on", 1 }, { "pulse.style", 4 }, { "pulse.speed", 0 }, { "pulse.level", 0.24f }, { "pulse.tone", 0.4f }, { "pulse.space", 0.5f }, { "pulse.swing", 0.05f },
            { "loop.on", 1 }, { "loop.voices", 4 }, { "loop.length", 18.0f }, { "loop.level", 0.35f },
            { "echo.div", 2 }, { "echo.fb", 0.55f }, { "echo.mix", 0.35f }, { "hall.mix", 0.5f }, { "hall.shimmer", 0.3f },
            { "bass.on", 1 }, { "bass.mode", 1 }, { "bass.density", 0.85f }, { "bass.wave", 1 }, { "bass.sub", 0.5f }, { "bass.cutoff", 520 },
            { "bass.length", 0.75f }, { "bass.level", 0.5f } } },

        //======================================================================  WORDS + BEATS
        { "Words + beats", "Educational Film",
          SEQ_CORE "vco1.saw>mix.in1;vco2.pls>mix.in2;lfo1.out>vcf1.cv2;" DRONE_OUT,
          { { "dream.sound", 4 }, { "prog.mode", 5 }, { "prog.bars", 1 }, { "prog.glide", 0.25f }, { "prog.colour", 3 }, { "arp.scale", 6 }, { "arp.root", 38 }, { "clk.bpm", 86 },
            { "arp.wave", 1 }, { "arp.shape", 0.35f }, { "arp.move", 0.3f }, { "vcf1.cut", 900 }, { "out.lvl1", 0.35f }, { "drn.level", 0.3f },
            { "pulse.on", 1 }, { "pulse.style", 11 }, { "pulse.kit", 2 }, { "pulse.speed", 0 }, { "pulse.swing", 0.3f }, { "pulse.human", 0.35f },
            { "pulse.level", 0.33f }, { "pulse.tone", 0.35f }, { "pulse.space", 0.3f }, { "pulse.density", 0.45f }, { "pulse.fill", 0.3f },
            { "words.on", 1 }, { "words.often", 0.45f }, { "words.phrases", 0.6f }, { "words.speed", 0.86f }, { "words.wobble", 0.6f },
            { "words.radio", 0.45f }, { "words.space", 0.45f }, { "words.level", 0.5f },
            { "tape.wow", 0.55f }, { "tape.age", 0.6f }, { "hall.mix", 0.35f },
            { "bass.on", 1 }, { "bass.mode", 3 }, { "bass.wave", 1 }, { "bass.density", 0.5f }, { "bass.glide", 0.3f }, { "bass.cutoff", 700 }, { "bass.level", 0.45f } } },
        { "Words + beats", "Numbers on the Radio", DRONE_OUT,
          { { "dream.sound", 6 }, { "prog.mode", 1 }, { "prog.bars", 3 }, { "arp.on", 0 }, { "out.lvl1", 0 }, { "arp.scale", 1 }, { "arp.root", 45 }, { "clk.bpm", 70 },
            { "drn.type", 3 }, { "drn.level", 0.35f }, { "drn.oct", -1 }, { "vcf2.cut", 1400 },
            { "words.on", 1 }, { "words.often", 0.6f }, { "words.phrases", 0.75f }, { "words.speed", 0.8f }, { "words.wobble", 0.35f },
            { "words.radio", 0.85f }, { "words.space", 0.6f }, { "words.level", 0.55f }, { "words.repeat", 0.25f },
            { "tex.on", 1 }, { "tex.a", 1 }, { "tex.b", 7 }, { "tex.level", 0.35f }, { "tex.density", 0.4f },
            { "pulse.on", 1 }, { "pulse.style", 0 }, { "pulse.kit", 1 }, { "pulse.level", 0.3f }, { "pulse.space", 0.7f },
            { "hall.mix", 0.55f }, { "hall.shimmer", 0.2f } } },
        { "Words + beats", "Broken Tape Break",
          SEQ_CORE "vco1.pls>mix.in1;vco2.saw>mix.in2;lfo2.out>vco1.pwm;" DRONE_OUT,
          { { "prog.mode", 3 }, { "prog.bars", 2 }, { "arp.scale", 0 }, { "arp.root", 43 }, { "clk.bpm", 92 },
            { "arp.wave", 4 }, { "arp.shape", 0.3f }, { "arp.fold", 0.15f }, { "vcf1.cut", 1100 }, { "out.lvl1", 0.35f }, { "drn.level", 0.25f },
            { "pulse.on", 1 }, { "pulse.style", 11 }, { "pulse.kit", 2 }, { "pulse.speed", 0 }, { "pulse.swing", 0.22f }, { "pulse.level", 0.42f },
            { "pulse.punch", 0.55f }, { "pulse.chop", 0.4f }, { "pulse.fill", 0.5f }, { "pulse.density", 0.55f }, { "pulse.space", 0.25f },
            { "words.on", 1 }, { "words.often", 0.3f }, { "words.phrases", 0.3f }, { "words.repeat", 0.5f }, { "words.radio", 0.35f },
            { "words.level", 0.45f }, { "tape.age", 0.5f }, { "echo.mix", 0.3f },
            { "brk.on", 1 }, { "brk.chop", 0.45f }, { "brk.stutter", 0.2f }, { "brk.dust", 0.45f }, { "brk.level", 0.9f } } },
        { "Words + beats", "Drill Garden",
          SEQ_CORE "vco1.saw>mix.in1;vco2.saw>mix.in2;lfo1.out>vcf1.cv2;" DRONE_OUT,
          { { "prog.mode", 6 }, { "prog.bars", 2 }, { "arp.scale", 4 }, { "arp.root", 41 }, { "clk.bpm", 88 },
            { "arp.wave", 5 }, { "arp.table", 4 }, { "arp.shape", 0.3f }, { "arp.move", 0.6f }, { "vcf1.cut", 1400 }, { "out.lvl1", 0.32f },
            { "pulse.on", 1 }, { "pulse.style", 13 }, { "pulse.kit", 6 }, { "pulse.speed", 0 }, { "pulse.stutter", 0.6f }, { "pulse.length", 5 },
            { "pulse.level", 0.42f }, { "pulse.punch", 0.5f }, { "pulse.space", 0.35f }, { "pulse.density", 0.5f },
            { "slime.play", 1 }, { "slime.voice", 4 }, { "slime.level", 0.5f }, { "slime.activity", 0.55f },
            { "drn.level", 0.2f }, { "hall.mix", 0.4f }, { "echo.mix", 0.35f },
            { "bass.on", 1 }, { "bass.mode", 4 }, { "bass.wave", 0 }, { "bass.sub", 0.2f }, { "bass.drive", 0.5f }, { "bass.glide", 0.55f },
            { "bass.decay", 0.7f }, { "bass.cutoff", 1500 }, { "bass.env", 0.2f }, { "bass.level", 0.5f } } },
        { "Words + beats", "Jungle Memory",
          SEQ_CORE "vco1.saw>mix.in1;vco2.sub>mix.in2;lfo1.out>vcf1.cv2;" DRONE_OUT,
          { { "prog.mode", 2 }, { "prog.bars", 2 }, { "arp.scale", 1 }, { "arp.root", 40 }, { "clk.bpm", 84 }, { "out.lvl1", 0.25f },
            { "pulse.on", 1 }, { "pulse.style", 12 }, { "pulse.kit", 1 }, { "pulse.speed", 0 }, { "pulse.chop", 0.5f }, { "pulse.fill", 0.6f },
            { "pulse.level", 0.4f }, { "pulse.punch", 0.45f }, { "pulse.space", 0.3f }, { "pulse.density", 0.6f },
            { "mem.on", 1 }, { "mem.level", 0.3f }, { "mem.speed", 1 }, { "drn.level", 0.35f }, { "drn.type", 0 },
            { "words.on", 1 }, { "words.often", 0.2f }, { "words.reverse", 0.4f }, { "words.level", 0.4f }, { "words.radio", 0.5f },
            { "hall.mix", 0.45f },
            { "bass.on", 1 }, { "bass.mode", 1 }, { "bass.rate", 2 }, { "bass.wave", 0 }, { "bass.sub", 0.6f }, { "bass.length", 1.0f },
            { "bass.glide", 0.4f }, { "bass.density", 0.4f }, { "bass.decay", 1.2f }, { "bass.level", 0.6f } } },
        { "Words + beats", "Wavetable Choir",
          SEQ_CORE "vco1.saw>mix.in1;vco2.saw>mix.in2;lfo1.out>vcf1.cv2;" DRONE_OUT,
          { { "dream.sound", 10 }, { "prog.mode", 1 }, { "prog.bars", 2 }, { "prog.glide", 0.4f }, { "arp.scale", 6 }, { "arp.root", 45 }, { "clk.bpm", 72 },
            { "arp.wave", 5 }, { "arp.table", 1 }, { "arp.shape", 0.4f }, { "arp.move", 0.7f }, { "vcf1.cut", 2200 }, { "vcf1.res", 0.3f },
            { "out.lvl1", 0.35f }, { "arp.density", 0.6f },
            { "slime.play", 1 }, { "slime.voice", 7 }, { "slime.level", 0.5f }, { "slime.style", 1 },
            { "loop.on", 1 }, { "loop.voices", 4 }, { "loop.length", 14.0f }, { "loop.level", 0.3f },
            { "words.on", 1 }, { "words.often", 0.25f }, { "words.speed", 0.75f }, { "words.space", 0.8f }, { "words.level", 0.35f },
            { "drn.level", 0.3f }, { "hall.mix", 0.6f }, { "hall.shimmer", 0.35f } } },

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
            { "vca1.gain", 0.0f }, { "vca1.cv", 1.0f }, { "dream.level", 0.4f }, { "dream.sound", 11 }, { "dream.chord", 3 },
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
    ch ("arp.wave", "Arp Wave", arpWaveNames, 0);
    {
        StringArray banks;
        for (int b = 0; b < dw::Wavetables::NumBanks; ++b) banks.add (dw::Wavetables::bankName (b));
        ch ("arp.table", "Arp Wavetable", banks, 0);
    }
    fl ("arp.shape", "Arp Wave Shape", NRange (0.0f, 1.0f), 0.0f);
    fl ("arp.fold", "Arp Wave Fold", NRange (0.0f, 1.0f), 0.0f);
    fl ("arp.move", "Arp Wave Move", NRange (0.0f, 1.0f), 0.0f);
    bo ("words.on", "Words On", false);
    ch ("words.src", "Words Source", wordsSourceNames, 0);
    fl ("words.level", "Words Level", NRange (0.0f, 1.0f), 0.5f);
    fl ("words.often", "Words Often", NRange (0.0f, 1.0f), 0.4f);
    fl ("words.phrases", "Words Phrases", NRange (0.0f, 1.0f), 0.5f);
    fl ("words.speed", "Words Speed", skewed (0.5f, 1.5f, 0.9f), 0.9f);
    fl ("words.wobble", "Words Wobble", NRange (0.0f, 1.0f), 0.4f);
    fl ("words.radio", "Words Radio", NRange (0.0f, 1.0f), 0.3f);
    fl ("words.reverse", "Words Reverse", NRange (0.0f, 1.0f), 0.1f);
    fl ("words.repeat", "Words Repeat", NRange (0.0f, 1.0f), 0.1f);
    fl ("words.space", "Words Space", NRange (0.0f, 1.0f), 0.5f);
    bo ("words.beat", "Words On The Beat", true);
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
    ch ("dream.sound", "Dream Sound", dreamSoundNames(), 0);
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
    // Harmony: chord colour, MY CHORDS (up to 8 chords, each with its own length), WANDER rhythm
    ch ("prog.colour", "Chord Colour", harm::colourNames, 0);
    ch ("prog.len", "My Chords Length", harm::lengthNames, 2);
    ch ("prog.rhythm", "Chord Rhythm", harm::rhythmNames, 0);
    for (int k = 0; k < harm::maxSlots; ++k)
    {
        const juce::String n (k + 1);
        ch (slotDegIds[k], ("My Chord " + n + " Degree").toRawUTF8(), harm::degreeNames, harm::defaultDegrees[k]);
        ch (slotTypeIds[k], ("My Chord " + n + " Type").toRawUTF8(), harm::slotTypeNames, 0);
        ch (slotLenIds[k], ("My Chord " + n + " Length").toRawUTF8(), harm::slotLenNames, 0);
    }
    // CHORDS layer HITS pattern
    in ("chd.steps", "Chords Hit Steps", 2, 16, 8);
    ch ("chd.rate", "Chords Hit Rate", hitRateNames, 1);
    fl ("chd.density", "Chords Hit Density", NRange (0.0f, 1.0f), 0.5f);
    fl ("chd.gate", "Chords Hit Gate", NRange (0.05f, 1.0f), 0.7f);
    fl ("chd.vary", "Chords Hit Vary", NRange (0.0f, 1.0f), 0.0f);
    // LOOPS: notes on loops of uneven lengths
    bo ("loop.on", "Loops On", false);
    ch ("loop.notes", "Loops Notes", loopNoteNames, 0);
    fl ("loop.level", "Loops Level", NRange (0.0f, 1.0f), 0.5f);
    fl ("loop.length", "Loops Length", skewed (4.0f, 40.0f, 14.0f), 14.0f);
    in ("loop.voices", "Loops Voices", 1, 8, 5);
    in ("loop.oct", "Loops Octave", -2, 1, 0);
    fl ("loop.colour", "Loops Colour", NRange (0.0f, 1.0f), 0.3f);
    fl ("loop.tone", "Loops Tone", NRange (0.0f, 1.0f), 0.55f);
    fl ("loop.swell", "Loops Swell", NRange (0.0f, 1.0f), 0.4f);
    fl ("loop.decay", "Loops Decay", skewed (0.5f, 20.0f, 5.0f), 6.0f);
    fl ("loop.drift", "Loops Drift", NRange (0.0f, 1.0f), 0.3f);
    fl ("loop.weird", "Loops Weird", NRange (0.0f, 1.0f), 0.25f);
    // SLIME PLAYS: the organism's ears play notes
    bo ("slime.play", "Slime Plays", false);
    ch ("slime.voice", "Slime Voice", slimeVoiceNames, 3);
    ch ("slime.style", "Slime Plays As", juce::StringArray { "NOTES", "CHORDS", "ARPEGGIO", "ALL THREE" }, 0);
    fl ("slime.level", "Slime Level", NRange (0.0f, 1.0f), 0.5f);
    fl ("slime.activity", "Slime Activity", NRange (0.0f, 1.0f), 0.45f);
    fl ("slime.weird", "Slime Weird", NRange (0.0f, 1.0f), 0.3f);
    in ("slime.ears", "Slime Ears", 3, 12, 7);
    fl ("slime.growth", "Slime Growth", skewed (0.25f, 3.0f, 1.0f), 1.0f);
    in ("slime.range", "Slime Range", 1, 3, 2);
    in ("slime.oct", "Slime Octave", -1, 2, 1);
    fl ("slime.tone", "Slime Tone", NRange (0.0f, 1.0f), 0.6f);
    // PULSE: soft percussion
    bo ("pulse.on", "Pulse On", false);
    ch ("pulse.style", "Pulse Style", pulseStyleNames, 0);
    ch ("pulse.speed", "Pulse Speed", pulseSpeedNames, 1);
    fl ("pulse.level", "Pulse Level", NRange (0.0f, 1.0f), 0.5f);
    fl ("pulse.density", "Pulse Density", NRange (0.0f, 1.0f), 0.5f);
    fl ("pulse.swing", "Pulse Swing", NRange (0.0f, 0.7f), 0.2f);
    fl ("pulse.human", "Pulse Human", NRange (0.0f, 1.0f), 0.3f);
    fl ("pulse.tone", "Pulse Tone", NRange (0.0f, 1.0f), 0.45f);
    fl ("pulse.space", "Pulse Space", NRange (0.0f, 1.0f), 0.55f);
    fl ("pulse.vary", "Pulse Vary", NRange (0.0f, 1.0f), 0.2f);
    fl ("pulse.weird", "Pulse Weird", NRange (0.0f, 1.0f), 0.15f);
    ch ("pulse.kit", "Pulse Kit", pulseKitNames, 0);
    fl ("pulse.punch", "Pulse Punch", NRange (0.0f, 1.0f), 0.35f);
    ch ("pulse.length", "Pulse Length", pulseLengthNames, 0);
    fl ("pulse.fill", "Pulse Fill", NRange (0.0f, 1.0f), 0.2f);
    fl ("pulse.stutter", "Pulse Stutter", NRange (0.0f, 1.0f), 0.0f);
    fl ("pulse.chop", "Pulse Chop", NRange (0.0f, 1.0f), 0.0f);
    for (int l = 0; l < 5; ++l)
    {
        const juce::String n (l + 1);
        fl (("pulse.l" + n).toRawUTF8(), ("Pulse Drum " + n + " Level").toRawUTF8(), NRange (0.0f, 1.5f), 1.0f);
        fl (("pulse.p" + n).toRawUTF8(), ("Pulse Drum " + n + " Pitch").toRawUTF8(), NRange (-12.0f, 12.0f, 1.0f), 0.0f);
        fl (("pulse.d" + n).toRawUTF8(), ("Pulse Drum " + n + " Decay").toRawUTF8(), skewed (0.25f, 4.0f, 1.0f), 1.0f);
    }
    // BREAK: a sliced drum loop on the beat
    bo ("brk.on", "Break On", false);
    ch ("brk.src", "Break Source", StringArray { "SYNTH BREAK", "MY BREAK" }, 0);
    ch ("brk.bars", "Break Bars", StringArray { "AUTO", "1 BAR", "2 BARS", "4 BARS", "8 BARS" }, 0);
    bo ("brk.fit", "Break Follows Tempo", true);
    bo ("brk.drums", "Break Keeps Drums", false);
    fl ("brk.level", "Break Level", NRange (0.0f, 1.0f), 0.7f);
    fl ("brk.chop", "Break Chop", NRange (0.0f, 1.0f), 0.35f);
    fl ("brk.stutter", "Break Stutter", NRange (0.0f, 1.0f), 0.15f);
    fl ("brk.reverse", "Break Reverse", NRange (0.0f, 1.0f), 0.1f);
    fl ("brk.pitch", "Break Pitch", NRange (-12.0f, 12.0f, 1.0f), 0.0f);
    fl ("brk.dust", "Break Dust", NRange (0.0f, 1.0f), 0.2f);
    // BASS: a bassline on the arp's clock and harmony
    bo ("bass.on", "Bass On", false);
    for (auto& ld : layerDefs)   // LAYERS: a mute for every sound
        bo (ld.mute, (juce::String ("Mute ") + juce::String (ld.name).toLowerCase()).toRawUTF8(), false);
    ch ("bass.mode", "Bass Mode", StringArray { "FOLLOW", "ROOTS", "PULSE", "WALK", "ACID" }, 0);
    ch ("bass.rate", "Bass Rate", StringArray { "EVERY STEP", "EVERY 2", "EVERY 4" }, 1);
    ch ("bass.range", "Bass Range", StringArray { "LOW", "MID", "HIGH" }, 1);
    ch ("bass.wave", "Bass Wave", StringArray { "SINE", "TRIANGLE", "SAW", "SQUARE", "WAVETABLE" }, 2);
    {
        StringArray banks;
        for (int b = 0; b < dw::Wavetables::NumBanks; ++b) banks.add (dw::Wavetables::bankName (b));
        ch ("bass.table", "Bass Wavetable", banks, 0);
    }
    fl ("bass.shape", "Bass Shape", NRange (0.0f, 1.0f), 0.3f);
    fl ("bass.sub", "Bass Sub", NRange (0.0f, 1.0f), 0.4f);
    fl ("bass.cutoff", "Bass Cutoff", skewed (40.0f, 5000.0f, 400.0f), 380.0f);
    fl ("bass.reso", "Bass Resonance", NRange (0.0f, 1.0f), 0.3f);
    fl ("bass.env", "Bass Env", NRange (0.0f, 1.0f), 0.45f);
    fl ("bass.decay", "Bass Decay", skewed (0.05f, 2.0f, 0.3f), 0.3f);
    fl ("bass.drive", "Bass Drive", NRange (0.0f, 1.0f), 0.25f);
    fl ("bass.glide", "Bass Glide", NRange (0.0f, 1.0f), 0.15f);
    fl ("bass.density", "Bass Density", NRange (0.0f, 1.0f), 0.6f);
    fl ("bass.length", "Bass Length", NRange (0.1f, 1.0f), 0.6f);
    fl ("bass.level", "Bass Level", NRange (0.0f, 1.0f), 0.6f);
    // TEXTURE: weather and creatures
    bo ("tex.on", "Texture On", false);
    ch ("tex.a", "Texture A", texNames, 0);
    ch ("tex.b", "Texture B", texNamesB, 0);
    fl ("tex.level", "Texture Level", NRange (0.0f, 1.0f), 0.4f);
    fl ("tex.blend", "Texture Blend", NRange (0.0f, 1.0f), 0.4f);
    fl ("tex.density", "Texture Density", NRange (0.0f, 1.0f), 0.5f);
    fl ("tex.tone", "Texture Tone", NRange (0.0f, 1.0f), 0.5f);
    fl ("tex.motion", "Texture Motion", NRange (0.0f, 1.0f), 0.5f);
    fl ("tex.key", "Texture In Key", NRange (0.0f, 1.0f), 0.5f);
    fl ("tex.space", "Texture Space", NRange (0.0f, 1.0f), 0.4f);
    // MEMORY: the instrument replays its own past
    bo ("mem.on", "Memory On", false);
    ch ("mem.speed", "Memory Speed", memSpeedNames, 1);
    fl ("mem.level", "Memory Level", NRange (0.0f, 1.0f), 0.5f);
    fl ("mem.ago", "Memory Ago", skewed (3.0f, 180.0f, 30.0f), 30.0f);
    fl ("mem.length", "Memory Length", skewed (1.0f, 12.0f, 4.0f), 5.0f);
    fl ("mem.blur", "Memory Blur", NRange (0.0f, 1.0f), 0.35f);
    fl ("mem.scatter", "Memory Scatter", NRange (0.0f, 1.0f), 0.4f);
    fl ("mem.dissolve", "Memory Dissolve", NRange (0.0f, 0.9f), 0.35f);
    // MORPH: glide every knob between two snapshots, A and B
    fl ("morph.pos", "Morph A-B", NRange (0.0f, 1.0f), 0.0f);
    ch ("morph.move", "Morph Move", juce::StringArray { "HOLD", "A TO B", "B TO A", "BACK + FORTH", "WANDER" }, 0);
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "morph.time", 1 }, "Morph Time", skewed (5.0f, 600.0f, 60.0f), 60.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return DriftwerkProcessor::morphTimeText (v); })));

    return { ps.begin(), ps.end() };
}

//==============================================================================
// LAYERS: the mixer's strips, in the order it shows them.
const DriftwerkProcessor::LayerDef DriftwerkProcessor::layerDefs[DriftwerkProcessor::numLayers] = {
    { "OUT 1",   "out.lvl1",    "mute.out1" },    { "OUT 2",   "out.lvl2",    "mute.out2" },
    { "DRONE",   "drn.level",   "mute.drone" },   { "DREAM",   "dream.level", "mute.dream" },
    { "GRAIN",   "grain.level", "mute.grain" },   { "CHORDS",  "chd.level",   "mute.chords" },
    { "LOOPS",   "loop.level",  "mute.loops" },   { "SLIME",   "slime.level", "mute.slime" },
    { "PULSE",   "pulse.level", "mute.pulse" },   { "BREAK",   "brk.level",   "mute.break" },
    { "BASS",    "bass.level",  "mute.bass" },    { "TEXTURE", "tex.level",   "mute.texture" },
    { "WORDS",   "words.level", "mute.words" },   { "MEMORY",  "mem.level",   "mute.memory" },
};

DriftwerkProcessor::DriftwerkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DRIFTWERK", createLayout())
{
    dw::Wavetables::get();   // build the arp's wavetables now, not on the audio thread
    synthWords = dw::SpeechSynth::makeBank();
    synthBreakAudio = dw::makeSynthBreak();
    syncBreak();
    for (auto& h : bassHist) h.store (-1);
    for (int k = 0; k < numLayers; ++k)
    {
        layerMuteParams[(size_t) k] = apvts.getRawParameterValue (layerDefs[k].mute);
        layerGain[k] = layerTarget[k] = 1.0f;
    }
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
    drifts.resize (paramList.size());
    // MORPH: which knobs it moves, which ones flip rather than glide, and where each one flips
    // (spread across the middle, so a scene changes over a while instead of all at once).
    morphStepped.resize (paramList.size(), 0);
    morphSkip.resize (paramList.size(), 0);
    morphFlipAt.resize (paramList.size(), 0.5f);
    for (size_t k = 0; k < paramList.size(); ++k)
    {
        auto* rp = paramList[k];
        const auto id = rp->getParameterID();
        morphSkip[k] = morphSkips (id) ? 1 : 0;
        morphStepped[k] = (dynamic_cast<juce::AudioParameterChoice*> (rp) != nullptr || dynamic_cast<juce::AudioParameterBool*> (rp) != nullptr
                           || id == "arp.root") ? 1 : 0;
        const auto h = (uint32_t) id.hashCode();
        morphFlipAt[k] = (id == "arp.root" || id == "arp.scale" || id == "prog.mode") ? 0.5f
                                                                                       : 0.3f + 0.4f * (float) (h % 1000u) / 999.0f;
    }
    morphTimer = std::make_unique<MorphTimer> (*this);
    for (auto& c : chordNotes) c.store (0);
    for (auto& w : wanderHist) w.store (0);
    for (int i = 0; i < dw::TapeLoops::V; ++i) { loopPhase[(size_t) i].store (0.0f); loopEnv[(size_t) i].store (0.0f); loopNote[(size_t) i].store (60); loopRatio[(size_t) i].store (1.0f); }
    for (int i = 0; i < numHitSteps; ++i) { hitWeight[(size_t) i].store (defaultHitWeights[i]); hitForce[(size_t) i].store (0); }
    for (auto& x : slimeEarNote) x.store (-1);
    for (auto& x : pulseW) x.store (0.0f);
    for (auto& x : pulseF) x.store (0);
    for (auto& x : pulseLaneHits) x.store (0);
    for (auto& x : pulseLanePos) x.store (-1);
    for (int l = 0; l < pulseLanes; ++l)
    {
        const juce::String n (l + 1);
        pulseLaneParams[l][0] = apvts.getRawParameterValue ("pulse.l" + n);
        pulseLaneParams[l][1] = apvts.getRawParameterValue ("pulse.p" + n);
        pulseLaneParams[l][2] = apvts.getRawParameterValue ("pulse.d" + n);
    }
    for (auto& x : texEnergy) x.store (0.0f);
    for (auto& x : memPeaks) x.store (0.0f);
    for (int k = 0; k < 2; ++k) { memHeadAgo[(size_t) k].store (0.0f); memHeadWin[(size_t) k].store (0.0f); memHeadSpeed[(size_t) k].store (1.0f); }
    curHarm = harm::padChord (0, 1, 1, 0);
    chordIvMask.store (curHarm.ivMask());
    syncGrainSource();
    apvts.addParameterListener ("grain.src", this);
    apvts.addParameterListener ("brk.src", this);
    apvts.addParameterListener ("brk.bars", this);

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

//==============================================================================
// PULSE samples and BREAK
std::unique_ptr<dw::SampleData> DriftwerkProcessor::readSound (const juce::File& f, double maxSeconds, bool trimStart)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr || reader->sampleRate < 1000.0) return nullptr;
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * maxSeconds));
    if (len < 64) return nullptr;
    juce::AudioBuffer<float> buf ((int) juce::jlimit (1u, 2u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, buf.getNumChannels() > 1);

    auto sd = std::make_unique<dw::SampleData>();
    sd->sampleRate = reader->sampleRate;
    sd->left.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
    sd->right.assign (buf.getReadPointer (buf.getNumChannels() - 1), buf.getReadPointer (buf.getNumChannels() - 1) + len);
    float peak = 0.0f;
    for (int i = 0; i < len; ++i) peak = std::max ({ peak, std::abs (sd->left[(size_t) i]), std::abs (sd->right[(size_t) i]) });
    if (peak < 1.0e-4f) return nullptr;
    if (trimStart)
    {   // skip silence before the first hit (keeping 2 ms), so it lands right on the beat
        int start = 0;
        while (start < len - 64 && std::max (std::abs (sd->left[(size_t) start]), std::abs (sd->right[(size_t) start])) < peak * 0.02f) ++start;
        start = std::max (0, start - (int) (0.002 * sd->sampleRate));
        if (start > 0)
        {
            sd->left.erase (sd->left.begin(), sd->left.begin() + start);
            sd->right.erase (sd->right.begin(), sd->right.begin() + start);
        }
    }
    for (auto* ch : { &sd->left, &sd->right })
        for (auto& x : *ch) x *= 0.9f / peak;
    return sd;
}

bool DriftwerkProcessor::loadPulseSample (int lane, const juce::File& f)
{
    if (lane < 0 || lane >= 5) return false;
    auto sd = readSound (f, 8.0, true);
    if (sd == nullptr) return false;
    pulseSampleNow[(size_t) lane].store (sd.get(), std::memory_order_release);
    pulseSampleStore.push_back (std::move (sd));
    pulseSamplePath[lane] = f.getFullPathName();
    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
    return true;
}

void DriftwerkProcessor::clearPulseSample (int lane)
{
    if (lane < 0 || lane >= 5) return;
    pulseSampleNow[(size_t) lane].store (nullptr, std::memory_order_release);
    if (pulseSamplePath[lane].isNotEmpty())
    {
        pulseSamplePath[lane].clear();
        updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
    }
}

bool DriftwerkProcessor::loadBreak (const juce::File& f, bool select)
{
    auto sd = readSound (f, 30.0, true);
    if (sd == nullptr) return false;
    myBreakAudio = sd.get();
    breakAudioStore.push_back (std::move (sd));
    breakPath = f.getFullPathName();
    if (select)
        if (auto* src = apvts.getParameter ("brk.src"))
        {
            src->beginChangeGesture();
            src->setValueNotifyingHost (src->convertTo0to1 (1.0f));
            src->endChangeGesture();
        }
    syncBreak();
    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
    return true;
}

void DriftwerkProcessor::syncBreak()
{
    const int src = (int) std::lround (apvts.getRawParameterValue ("brk.src")->load());
    const int barsSel = juce::jlimit (0, 4, (int) std::lround (apvts.getRawParameterValue ("brk.bars")->load()));
    const dw::SampleData* audio = (src == 1 && myBreakAudio != nullptr) ? myBreakAudio : synthBreakAudio.get();
    if (audio == nullptr) return;
    const int bars = barsSel == 0 ? 0 : (1 << (barsSel - 1));
    if (const auto* now = breakNow.load(); now != nullptr && now->audio == audio && now->barsAsked == bars) return;
    auto bl = std::make_unique<dw::BreakLoop>();
    bl->audio = audio;
    bl->barsAsked = bars;
    bl->builtIn = audio == synthBreakAudio.get();
    bl->name = bl->builtIn ? "synth break" : juce::File (breakPath).getFileNameWithoutExtension().toStdString();
    bl->analyse (bars);
    breakNow.store (bl.get(), std::memory_order_release);
    breakLoopStore.push_back (std::move (bl));
}

//==============================================================================
// DREAM: MY SOUND
juce::StringArray DriftwerkProcessor::dreamSoundNames()
{
    juce::StringArray names;
    for (int s = 0; s < dw::DreamBank::NumSounds; ++s) names.add (dw::DreamBank::soundName (s));
    return names;
}

bool DriftwerkProcessor::loadDreamSound (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr || reader->sampleRate < 1000.0) return false;

    // The first 30 seconds, mixed to mono (each loop has its own place in the stereo field).
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));
    if (len < 256) return false;
    juce::AudioBuffer<float> buf ((int) juce::jlimit (1u, 2u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, buf.getNumChannels() > 1);

    auto sd = std::make_unique<dw::SampleData>();
    sd->sampleRate = reader->sampleRate;
    sd->left.resize ((size_t) len);
    const float* a = buf.getReadPointer (0);
    const float* b = buf.getReadPointer (buf.getNumChannels() - 1);
    for (int i = 0; i < len; ++i) sd->left[(size_t) i] = 0.5f * (a[i] + b[i]);
    // Skip silence at the start, so every loop speaks the moment it strikes.
    float peak = 0.0f;
    for (auto x : sd->left) peak = std::max (peak, std::abs (x));
    if (peak < 1.0e-4f) return false;
    int start = 0;
    while (start < len - 256 && std::abs (sd->left[(size_t) start]) < peak * 0.02f) ++start;
    start = std::max (0, start - (int) (0.005 * reader->sampleRate));
    sd->left.erase (sd->left.begin(), sd->left.begin() + start);
    for (auto& x : sd->left) x *= 0.8f / peak;
    sd->right = sd->left;
    sd->rootMidi = dw::detectRootMidi (*sd);

    dreamMine.store (sd.get(), std::memory_order_release);
    dreamStore.push_back (std::move (sd));
    dreamPath = f.getFullPathName();
    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
    return true;
}

//==============================================================================
// WORDS
const dw::WordBank* DriftwerkProcessor::wordsBankNow() const
{
    const int src = (int) std::lround (apvts.getRawParameterValue ("words.src")->load());
    if (src == 0)
        if (auto* mine = userWords.load()) return mine;
    return synthWords.get();
}

void DriftwerkProcessor::loadWords (const juce::StringArray& paths)
{
    if (paths.isEmpty() || wordsLoading.exchange (true)) return;
    wordsLoader.addJob ([this, paths]
    {
        // Read the files (a folder gives every sound in it, in name order) as one long mono
        // recording at 22 kHz, up to 20 minutes; then find the words in each.
        juce::Array<juce::File> files;
        for (const auto& path : paths)
        {
            const juce::File f (path);
            if (f.isDirectory())
            {
                auto inside = f.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
                inside.sort();
                files.addArray (inside);
            }
            else if (f.existsAsFile()) files.add (f);
        }
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        auto bank = std::make_unique<dw::WordBank>();
        bank->rate = 22050.0;
        const double maxSeconds = 20.0 * 60.0;
        int used = 0;
        for (const auto& f : files)
        {
            if ((double) bank->audio.size() / bank->rate > maxSeconds) break;
            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
            if (reader == nullptr || reader->sampleRate <= 0.0) continue;
            const double step = reader->sampleRate / bank->rate;
            const juce::int64 total = std::min<juce::int64> (reader->lengthInSamples,
                (juce::int64) ((maxSeconds - (double) bank->audio.size() / bank->rate) * reader->sampleRate));
            const int chunk = 65536;
            juce::AudioBuffer<float> buf ((int) juce::jmax (1u, reader->numChannels), chunk);
            dw::Svf aa1, aa2;   // keep what's above 10 kHz from folding down
            aa1.set (9500.0f, 0.54f, (float) reader->sampleRate); aa2.set (9500.0f, 1.3f, (float) reader->sampleRate);
            std::vector<float> mono, out;
            double readPos = 0.0;
            float prev = 0.0f;
            const int from = (int) bank->audio.size();
            for (juce::int64 at = 0; at < total; at += chunk)
            {
                const int n = (int) std::min<juce::int64> (chunk, total - at);
                reader->read (&buf, 0, n, at, true, true);
                mono.resize ((size_t) n);
                for (int i = 0; i < n; ++i)
                {
                    float m = 0.0f;
                    for (int c = 0; c < buf.getNumChannels(); ++c) m += buf.getSample (c, i);
                    m /= (float) buf.getNumChannels();
                    mono[(size_t) i] = aa2.lowpass (aa1.lowpass (m));
                }
                out.clear();
                while (readPos < (double) n)
                {
                    const int i0 = (int) readPos;
                    const float fr = (float) (readPos - (double) i0);
                    const float a = i0 == 0 ? prev : mono[(size_t) i0 - 1], b = mono[(size_t) i0];
                    out.push_back (a + (b - a) * fr);
                    readPos += step;
                }
                readPos -= (double) n;
                prev = mono.back();
                bank->append (out.data(), (int) out.size(), 1.0f);
            }
            // each file is normalised on its own, so quiet and loud recordings sit together
            int16_t peak = 1;
            for (size_t i = (size_t) from; i < bank->audio.size(); ++i) peak = std::max<int16_t> (peak, (int16_t) std::abs ((int) bank->audio[i]));
            const float g = std::min (8.0f, 0.7f * 32767.0f / (float) peak);
            for (size_t i = (size_t) from; i < bank->audio.size(); ++i)
                bank->audio[i] = (int16_t) juce::jlimit (-32767, 32767, (int) std::lround ((float) bank->audio[i] * g));
            bank->slice (from);
            for (int k = 0; k < (int) (0.3 * bank->rate); ++k) bank->audio.push_back (0);
            ++used;
        }
        if (! bank->words.empty())
        {
            bank->name = (used == 1 ? files[0].getFileName() : juce::String (used) + " recordings").toStdString();
            const juce::ScopedLock sl (wordBanksLock);
            userWords.store (bank.get());
            wordBanks.push_back (std::move (bank));
            wordsPath = paths.joinIntoString ("\n");
            wordsError.clear();
        }
        else
        {
            const juce::ScopedLock sl (wordBanksLock);
            wordsError = files.isEmpty() ? juce::String ("no sound files there") : juce::String ("couldn't hear any words in them");
        }
        wordsLoading.store (false);
    });
}

void DriftwerkProcessor::scheduleWords (int numSamples)
{
    const auto* bank = (p.wordsSrc == 0 && userWords.load() != nullptr) ? userWords.load() : synthWords.get();
    wordsNowBank.store (bank, std::memory_order_relaxed);
    if (! p.wordsOn || bank == nullptr || bank->words.empty()) return;
    wordsWait -= (double) numSamples / sr;
    if (wordsWait > 0.0) return;
    auto& rng = weirdRng;
    // How long until the next one: about 40 s apart at OFTEN 0, a couple of seconds at 1.
    const double mean = 2.0 + 40.0 * (1.0 - p.wordsOften) * (1.0 - p.wordsOften);
    wordsWait = mean * (0.4 + 1.2 * dw::uni (rng));
    if (wordsPlayer.busy() && dw::uni (rng) < 0.7f) return;   // mostly one voice at a time
    int first, count = 1;
    if (dw::uni (rng) < p.wordsPhrases && ! bank->phrases.empty())
    {
        const auto& ph = bank->phrases[(size_t) std::min ((int) bank->phrases.size() - 1, (int) (dw::uni (rng) * (float) bank->phrases.size()))];
        first = ph.first; count = juce::jmin (ph.count, 12);
    }
    else first = std::min ((int) bank->words.size() - 1, (int) (dw::uni (rng) * (float) bank->words.size()));
    const float speed = p.wordsSpeed * (0.97f + 0.06f * dw::uni (rng));
    const bool reverse = dw::uni (rng) < p.wordsReverse * 0.6f;
    const int repeats = dw::uni (rng) < p.wordsRepeat ? 1 + (int) (dw::uni (rng) * (1.0f + 3.0f * p.wordsRepeat)) : 0;
    float delay = 0.0f;
    if (p.wordsBeat)   // start on the next beat
    {
        const double b = beatPos, next = std::ceil (b + 1.0e-6);
        delay = (float) ((next - b) * samplesPerBeat / sr);
    }
    wordsPlayer.start (bank, first, count, speed, reverse, repeats, (dw::uni (rng) - 0.5f) * 0.6f, delay);
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
    // A new DREAM sound most of the time (built-in ones only; MY SOUND stays if you chose it).
    if ((int) std::lround (apvts.getRawParameterValue ("dream.sound")->load()) != dreamMySound && r.nextFloat() < 0.7f)
        set ("dream.sound", (float) r.nextInt (dreamMySound));
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
    loopShuffleReq.fetch_add (1);
    pulseNewReq.fetch_add (1);
    sceneCounter.fetch_add (1);
}

juce::String DriftwerkProcessor::getModString() const
{
    juce::StringArray items;
    for (size_t k = 0; k < paramList.size(); ++k)
    {
        const int src = mods[k]->source.load();
        const char* id = src >= 0 && src < NumOuts ? outDefs[src].id
                       : (src >= driftSource && src < driftSource + numDrifts ? driftId (src - driftSource) : nullptr);
        if (id != nullptr)
            items.add (paramList[k]->getParameterID() + "<" + id + "@" + juce::String (mods[k]->depth.load(), 3));
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
        const auto srcId = rest.upToFirstOccurrenceOf ("@", false, false).trim();
        int src = findOut (srcId.toRawUTF8());
        for (int d = 0; d < numDrifts; ++d) if (srcId == driftId (d)) src = driftSource + d;
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
    apvts.removeParameterListener ("brk.src", this);
    apvts.removeParameterListener ("brk.bars", this);
    wordsLoader.removeAllJobs (false, 60000);   // let a words file that is loading finish first
    cancelPendingUpdate();
    stopRecording();
    cameraPoller = nullptr;
    morphTimer = nullptr;
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
    owner.organism.handActive.store (owner.camera.isOpen());
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
    limiter.prepare ((float) sr);
    setLatencySamples (limiter.look);
    vcf1.reset(); vcf2.reset();
    ensemble.prepare ((float) sr);
    echo.prepare ((float) sr);
    reverb.prepare ((float) sr);
    dream.prepare ((float) sr);
    grains.prepare ((float) sr);
    chordLayer.prepare ((float) sr);
    tapeLoops.prepare ((float) sr);
    loopPitchKey = -1;
    slimeVoices.prepare ((float) sr);
    pulseDrums.prepare ((float) sr);
    bassVoice.prepare ((float) sr);
    breakPlayer.prepare ((float) sr);
    for (int k = 0; k < numLayers; ++k) layerGain[k] = layerTarget[k] = layerMuteParams[(size_t) k]->load() > 0.5f ? 0.0f : 1.0f;
    layerCoef = onePoleCoef (0.012f, (float) sr);
    breakLastK = pulseNoStep;
    wordsPlayer.prepare ((float) sr);
    texture.prepare ((float) sr);
    memory.prepare ((float) sr);
    weirdPitchKey = -1;
    pulseLastK = pulseNoStep;
    organism.ensureRunning();
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
        const int m = curHarm.ivMask();
        int n = 0;
        for (int k = 0; k < 12; ++k) if ((m >> k) & 1) out[n++] = k;
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

// BASS: the arp's clock and harmony, its own register and rhythm. Every MODE plays the chord's
// root at the start of its bar and when the chord changes, so it always grounds the harmony.
void DriftwerkProcessor::bassClock (bool arpPlays, int arpSemis)
{
    if (! p.bassOn) { bassTick = 0; return; }
    const int div = p.bassRate == 0 ? 1 : (p.bassRate == 1 ? 2 : 4);
    const int t = bassTick++;
    if (t % div != 0) return;
    const int slot = (t / div) % 16;
    auto u = [this] { return bassRng.next() * 0.5f + 0.5f; };
    const int chordRoot = chordOffsetTarget;
    const int third = curHarm.has (3) ? 3 : (curHarm.has (4) ? 4 : (curHarm.has (2) ? 2 : 5));
    const int fifth = curHarm.has (7) ? 7 : (curHarm.has (6) ? 6 : (curHarm.has (8) ? 8 : 7));
    const bool changed = chordRoot != bassLastChord;
    bassLastChord = chordRoot;
    const bool anchor = slot == 0 || changed;
    const int keyRoot = p.root + midiTranspose;
    auto inScale = [&] (int rel)
    {
        const int pc = (((keyRoot + chordRoot + rel) % 12) + 12) % 12;
        for (int k = 0; k < scalePcCount; ++k) if (scalePcs[k] == pc) return true;
        return scalePcCount == 0;
    };
    constexpr int rest = -999;
    int rel = rest;
    bool slide = false;
    float acc = 0.0f;
    switch (p.bassMode)
    {
        case 0:   // FOLLOW: plays with the arp, its notes moved down under the chord, a chord tone more often than not
            if (anchor) rel = 0;
            else if (arpPlays && u() < 0.3f + 0.7f * p.bassDensity)
            {
                rel = (((arpSemis - chordRoot) % 12) + 12) % 12;
                if (! curHarm.has (rel) && u() < 0.6f)
                {
                    const int tones[] { 0, third, fifth, 12 };
                    int best = 0;
                    for (int c : tones) if (std::abs (c - rel) < std::abs (best - rel)) best = c;
                    rel = best;
                }
                slide = u() < p.bassGlide * 0.4f;
            }
            break;
        case 1:   // ROOTS: the root, now and then its octave or fifth
            if (anchor) rel = 0;
            else if (u() < p.bassDensity * 0.7f) { const float m = u(); rel = m < 0.6f ? 0 : (m < 0.8f ? 12 : fifth); }
            break;
        case 2:   // PULSE: every step, root and octave, like a Berlin-school sequencer
            if (anchor || u() < 0.55f + 0.45f * p.bassDensity)
            {
                static const int pat[8] { 0, 0, 12, 0, 0, 12, 0, -1 };
                rel = pat[slot % 8] < 0 ? fifth : pat[slot % 8];
                if (! anchor && u() < 0.12f * p.bassDensity) rel = third;
            }
            break;
        case 3:   // WALK: steps up and down the scale around the root, home again on the anchors
            if (anchor) { rel = 0; bassWalk = 0; }
            else if (u() < 0.3f + 0.6f * p.bassDensity)
            {
                int cand[24], n = 0;
                for (int r = -5; r <= 12 && n < 24; ++r) if (inScale (r)) cand[n++] = r;
                int at = 0;
                for (int k = 0; k < n; ++k) if (std::abs (cand[k] - bassWalk) < std::abs (cand[at] - bassWalk)) at = k;
                int move = u() < 0.7f ? 1 : 2;           // a step or a skip, down when high, up when low
                if (bassWalk > 7 || (bassWalk >= -3 && u() < 0.5f)) move = -move;
                at = juce::jlimit (0, juce::jmax (0, n - 1), at + move);
                rel = bassWalk = n > 0 ? cand[at] : 0;
                slide = u() < p.bassGlide * 0.3f;
            }
            break;
        default:  // ACID: its own sixteen steps with accents and slides; the arp's MUTATE rewrites a few each bar
        {
            auto make = [&] (int k)
            {
                auto& a = bassAcid[k];
                a.on = k == 0 || u() < 0.3f + 0.55f * p.bassDensity;
                const int choices[] { 0, 0, 0, 12, 12, fifth, third, 10 };
                a.rel = choices[(int) (u() * 7.999f)];
                if (a.rel == 10 && ! inScale (10)) a.rel = 0;
                a.slide = u() < 0.15f + 0.35f * p.bassGlide;
                a.acc = u() < 0.3f ? 1.0f : 0.0f;
            };
            if (! bassAcidMade) { for (int k = 0; k < 16; ++k) make (k); bassAcidMade = true; }
            if (slot == 0 && u() < p.mutate + 0.05f)
                for (int r = 0, n = 1 + (int) (u() * 3.0f); r < n; ++r) make ((int) (u() * 15.999f));
            const auto& a = bassAcid[slot];
            if (a.on) { rel = a.rel; slide = a.slide; acc = a.acc; }
            break;
        }
    }
    const int head = bassHistHead.load (std::memory_order_relaxed);
    if (rel == rest)
    {
        bassHist[(size_t) head].store (-1, std::memory_order_relaxed);
        bassHistHead.store ((head + 1) % bassHistSize, std::memory_order_relaxed);
        return;
    }
    // The chord root folded into the RANGE's octave, the note on top of it.
    const int lo = p.bassRange == 0 ? 26 : (p.bassRange == 1 ? 33 : 40);   // the root sits in D1..C#2, A1..G#2 or E2..D#3
    int rootNote = keyRoot + chordRoot;
    while (rootNote < lo) rootNote += 12;
    while (rootNote >= lo + 12) rootNote -= 12;
    const int note = rootNote + rel;
    const double span = (double) arp.clockPeriod * div;
    // A slide holds this note into the next one, which then glides in.
    const int gateSamples = (int) (span * (slide ? 1.08 : (double) p.bassLength));
    bassVoice.noteOn ((float) note + p.tune, gateSamples, true, acc);
    bassHist[(size_t) head].store (note | (acc > 0.0f ? 256 : 0) | (slide ? 512 : 0), std::memory_order_relaxed);
    bassHistHead.store ((head + 1) % bassHistSize, std::memory_order_relaxed);
    bassNotes.fetch_add (1, std::memory_order_relaxed);
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
    const int semis = scaleSemitone (deg) + (p.arpFollow != 0 ? chordOffsetTarget : 0);
    bassClock (on, semis);

    if (play)
    {
        arp.pitchOut = (float) semis / 12.0f;
        arp.gateOut = true;
        arp.gateLeft = juce::jmax (8, (int) ((float) arp.clockPeriod * p.gateLen));
        arpNotePc.store ((((p.root + midiTranspose + semis) % 12) + 12) % 12, std::memory_order_relaxed);
        arpNoteCount.fetch_add (1, std::memory_order_relaxed);
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
        else if (src >= driftSource && src < driftSource + numDrifts)
        {
            // DRIFT: glides from one random point to the next (-1..1), a little irregularly.
            auto& d = drifts[k];
            static constexpr float periods[numDrifts] { 40.0f, 12.0f, 3.0f };
            d.t += d.rate * (float) blockLen / (float) sr;
            if (d.t >= 1.0f || d.rate <= 0.0f)
            {
                d.a = d.rate <= 0.0f ? 0.0f : d.b;
                d.b = driftRng.next();
                d.t = 0.0f;
                d.rate = 1.0f / (periods[src - driftSource] * (0.6f + 0.8f * (driftRng.next() * 0.5f + 0.5f)));
            }
            const float sm = d.t * d.t * (3.0f - 2.0f * d.t);
            n = juce::jlimit (0.0f, 1.0f, n + mods[k]->depth.load (std::memory_order_relaxed) * (d.a + (d.b - d.a) * sm));
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
    p.progMode = juce::jlimit (0, harm::numProgModes - 1, i ("prog.mode"));
    p.progBars = progBars[juce::jlimit (0, 4, i ("prog.bars"))]; p.progGlide = f ("prog.glide");
    p.progColour = juce::jlimit (0, 6, i ("prog.colour")); p.progLen = juce::jlimit (2, harm::maxSlots, i ("prog.len") + 2);
    p.progRhythm = juce::jlimit (0, 3, i ("prog.rhythm"));
    for (int k = 0; k < harm::maxSlots; ++k)
    {
        p.slotDeg[k] = juce::jlimit (0, 6, i (slotDegIds[k]));
        p.slotType[k] = juce::jlimit (0, harm::NumTypes - 1, i (slotTypeIds[k]));
        p.slotLen[k] = juce::jlimit (0, 6, i (slotLenIds[k]));
    }
    p.chdSteps = juce::jlimit (2, numHitSteps, i ("chd.steps")); p.chdRate = juce::jlimit (0, 4, i ("chd.rate"));
    p.chdDensity = f ("chd.density"); p.chdGate = f ("chd.gate"); p.chdVary = f ("chd.vary");
    p.loopOn = f ("loop.on") > 0.5f; p.loopNotes = juce::jlimit (0, 2, i ("loop.notes")); p.loopVoices = juce::jlimit (1, 8, i ("loop.voices"));
    p.loopOct = i ("loop.oct"); p.loopLevel = f ("loop.level"); p.loopLength = f ("loop.length"); p.loopColour = f ("loop.colour");
    p.loopTone = f ("loop.tone"); p.loopSwell = f ("loop.swell"); p.loopDecay = f ("loop.decay"); p.loopDrift = f ("loop.drift");
    p.loopWeird = f ("loop.weird");
    p.slimePlay = f ("slime.play") > 0.5f; p.slimeVoice = juce::jlimit (0, slimeVoiceNames.size() - 1, i ("slime.voice")); p.slimeStyle = juce::jlimit (0, 3, i ("slime.style")); p.slimeLevel = f ("slime.level");
    p.slimeWeird = f ("slime.weird"); p.slimeRange = juce::jlimit (1, 3, i ("slime.range")); p.slimeOct = i ("slime.oct"); p.slimeTone = f ("slime.tone");
    organism.activity.store (f ("slime.activity"), std::memory_order_relaxed);
    organism.weird.store (p.slimeWeird, std::memory_order_relaxed);
    organism.growth.store (f ("slime.growth"), std::memory_order_relaxed);
    organism.earCount.store (juce::jlimit (3, Organism::maxEars, i ("slime.ears")), std::memory_order_relaxed);
    p.pulseOn = f ("pulse.on") > 0.5f; p.pulseStyle = juce::jlimit (0, pulseNumStyles - 1, i ("pulse.style")); p.pulseSpeed = juce::jlimit (0, 2, i ("pulse.speed"));
    p.pulseLevel = f ("pulse.level"); p.pulseDensity = f ("pulse.density"); p.pulseSwing = f ("pulse.swing"); p.pulseHuman = f ("pulse.human");
    p.pulseTone = f ("pulse.tone"); p.pulseSpace = f ("pulse.space"); p.pulseVary = f ("pulse.vary"); p.pulseWeird = f ("pulse.weird");
    p.pulseKit = juce::jlimit (0, dw::Pulse::NumKits - 1, i ("pulse.kit")); p.pulseLength = juce::jlimit (0, 5, i ("pulse.length"));
    p.pulsePunch = f ("pulse.punch"); p.pulseFill = f ("pulse.fill"); p.pulseStutter = f ("pulse.stutter"); p.pulseChop = f ("pulse.chop");
    pulseDrums.kit = p.pulseKit; pulseDrums.punch = p.pulsePunch;
    for (int l = 0; l < 5; ++l) pulseDrums.laneSample[l] = pulseSampleNow[(size_t) l].load (std::memory_order_acquire);
    p.brkOn = f ("brk.on") > 0.5f; p.brkDrums = f ("brk.drums") > 0.5f; p.brkFit = f ("brk.fit") > 0.5f;
    p.brkLevel = f ("brk.level"); p.brkChop = f ("brk.chop"); p.brkStutter = f ("brk.stutter"); p.brkReverse = f ("brk.reverse");
    p.brkPitch = f ("brk.pitch"); p.brkDust = f ("brk.dust");
    for (int k = 0; k < numLayers; ++k) layerTarget[k] = layerMuteParams[(size_t) k]->load (std::memory_order_relaxed) > 0.5f ? 0.0f : 1.0f;
    if (const int req = bassNewReq.load (std::memory_order_relaxed); req != bassNewSeen) { bassNewSeen = req; bassAcidMade = false; bassWalk = 0; }
    p.bassOn = f ("bass.on") > 0.5f; p.bassMode = juce::jlimit (0, 4, i ("bass.mode")); p.bassRate = juce::jlimit (0, 2, i ("bass.rate"));
    p.bassRange = juce::jlimit (0, 2, i ("bass.range")); p.bassWave = juce::jlimit (0, 4, i ("bass.wave"));
    p.bassTable = juce::jlimit (0, dw::Wavetables::NumBanks - 1, i ("bass.table"));
    p.bassShape = f ("bass.shape"); p.bassSub = f ("bass.sub"); p.bassCutoff = f ("bass.cutoff"); p.bassReso = f ("bass.reso");
    p.bassEnv = f ("bass.env"); p.bassDecay = f ("bass.decay"); p.bassDrive = f ("bass.drive"); p.bassGlide = f ("bass.glide");
    p.bassDensity = f ("bass.density"); p.bassLength = f ("bass.length"); p.bassLevel = f ("bass.level");
    for (int l = 0; l < 5; ++l)
    {
        pulseDrums.laneLevel[l] = pulseLaneParams[l][0]->load();
        pulseDrums.lanePitch[l] = pulseLaneParams[l][1]->load();
        pulseDrums.laneDecay[l] = pulseLaneParams[l][2]->load();
    }
    p.texOn = f ("tex.on") > 0.5f; p.texA = juce::jlimit (0, (int) dw::Texture::NumKinds - 1, i ("tex.a")); p.texB = juce::jlimit (0, (int) dw::Texture::NumKinds, i ("tex.b"));
    p.texLevel = f ("tex.level"); p.texBlend = f ("tex.blend"); p.texDensity = f ("tex.density"); p.texTone = f ("tex.tone");
    p.texMotion = f ("tex.motion"); p.texKey = f ("tex.key"); p.texSpace = f ("tex.space");
    p.memOn = f ("mem.on") > 0.5f; p.memSpeed = juce::jlimit (0, 5, i ("mem.speed")); p.memLevel = f ("mem.level"); p.memAgo = f ("mem.ago");
    p.memLength = f ("mem.length"); p.memBlur = f ("mem.blur"); p.memScatter = f ("mem.scatter"); p.memDissolve = f ("mem.dissolve");
    p.vco1Oct = i ("vco1.oct"); p.vco1Fine = f ("vco1.fine"); p.vco1Pw = f ("vco1.pw"); p.vco1Pwm = f ("vco1.pwm"); p.vco1Fm = f ("vco1.fm");
    p.vco2Oct = i ("vco2.oct"); p.vco2Fine = f ("vco2.fine"); p.vco2Pw = f ("vco2.pw"); p.vco2Pwm = f ("vco2.pwm"); p.vco2Fm = f ("vco2.fm");
    p.arpWave = juce::jlimit (0, dw::WaveOsc::NumWaves - 1, i ("arp.wave")); p.arpTable = juce::jlimit (0, dw::Wavetables::NumBanks - 1, i ("arp.table"));
    p.arpShape = f ("arp.shape"); p.arpFold = f ("arp.fold"); p.arpMove = f ("arp.move");
    p.wordsOn = f ("words.on") > 0.5f; p.wordsBeat = f ("words.beat") > 0.5f; p.wordsSrc = juce::jlimit (0, 1, i ("words.src"));
    p.wordsLevel = f ("words.level"); p.wordsOften = f ("words.often"); p.wordsPhrases = f ("words.phrases"); p.wordsSpeed = f ("words.speed");
    p.wordsWobble = f ("words.wobble"); p.wordsRadio = f ("words.radio"); p.wordsReverse = f ("words.reverse"); p.wordsRepeat = f ("words.repeat");
    p.wordsSpace = f ("words.space");
    p.droneChord = p.droneChordUser = juce::jlimit (0, 5, i ("drn.chord")); p.droneOct = i ("drn.oct"); p.droneDetune = f ("drn.detune");
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
    p.dreamChord = p.dreamChordUser = juce::jlimit (0, 5, i ("dream.chord")); p.dreamRate = f ("dream.rate"); p.dreamTone = f ("dream.tone");
    p.dreamWarp = f ("dream.warp"); p.dreamLevel = f ("dream.level");
    p.dreamSound = juce::jlimit (0, dw::DreamBank::NumSounds - 1, i ("dream.sound"));
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
// Chord progression. Runs every sample so the pads, drone, arp and CHORDS all change on the
// same sample, right on the bar line. The chord itself is only rebuilt when it changes.
void DriftwerkProcessor::applyProgression (double beat)
{
    using namespace harm;
    const int mode = p.progMode;
    const double even = 4.0 * p.progBars;              // beats per chord at EVEN (CHANGE EVERY)
    int idx = -1;
    long long code = 0;                                 // which chord of this mode
    double phase = 0.0;
    if (mode == 0)
        code = 0;
    else if (mode <= progTableLast)
    {
        const double u = beat / even, f = std::floor (u);
        idx = (int) ((long long) f & 3);
        phase = u - f;
        code = idx;
    }
    else if (mode == progWander)
    {
        advanceWander (beat, even);
        idx = juce::jmax (0, wanderCount.load (std::memory_order_relaxed) - 1);
        phase = (beat - wanderStart) / wanderLen;
        code = wanderCur;
    }
    else
    {
        // MY CHORDS: LENGTH chords, each as long as its own length; the loop starts at beat 0.
        auto slotBeats = [&] (int k) { return p.slotLen[k] == 0 ? even : slotLenBeats[p.slotLen[k]]; };
        double total = 0.0;
        for (int k = 0; k < p.progLen; ++k) total += slotBeats (k);
        double pos = std::fmod (beat, total);
        if (pos < 0.0) pos += total;
        idx = p.progLen - 1;
        for (int k = 0; k < p.progLen; ++k)
        {
            const double len = slotBeats (k);
            if (pos < len) { idx = k; phase = pos / len; break; }
            pos -= len;
        }
        code = p.slotDeg[idx] * 16 + p.slotType[idx];
    }
    if (mode != progWander) wanderOn = false;
    progPhaseNow = (float) juce::jlimit (0.0, 1.0, phase);

    const long long key = ((((((long long) mode * 16 + p.scale) * 8 + p.progColour) * 8 + p.droneChordUser) * 8
                            + p.dreamChordUser) * 1024) + code;
    if (key != harmKey)
    {
        harmKey = key;
        if (mode == 0)                curHarm = staticChord (p.scale, p.progColour, p.dreamChordUser, p.droneChordUser);
        else if (mode <= progTableLast) curHarm = tableChord (mode, p.scale, idx, p.progColour, p.droneChordUser);
        else if (mode == progWander)  curHarm = codeChord (p.scale, wanderCur, p.progColour, p.droneChordUser);
        else                          curHarm = slotChord (p.scale, p.slotDeg[idx], p.slotType[idx], p.progColour, p.droneChordUser);
        chordIvMask.store (curHarm.ivMask(), std::memory_order_relaxed);
        progOffset.store (curHarm.offset, std::memory_order_relaxed);
    }
    chordOffsetTarget = curHarm.offset;
    p.dreamChord = curHarm.dream >= 0 ? curHarm.dream : p.dreamChordUser;
    p.droneChord = curHarm.drone;
    if (progIndex.load (std::memory_order_relaxed) != idx) progIndex.store (idx, std::memory_order_relaxed);
}

void DriftwerkProcessor::pushWander (int code)
{
    const int n = wanderCount.load (std::memory_order_relaxed);
    if (n < harm::maxSlots)
    {
        wanderHist[(size_t) n].store (code, std::memory_order_relaxed);
        wanderCount.store (n + 1, std::memory_order_relaxed);
        return;
    }
    for (int k = 0; k + 1 < harm::maxSlots; ++k)
        wanderHist[(size_t) k].store (wanderHist[(size_t) k + 1].load (std::memory_order_relaxed), std::memory_order_relaxed);
    wanderHist[harm::maxSlots - 1].store (code, std::memory_order_relaxed);
}

// How long the next WANDER chord lasts, from RHYTHM.
double DriftwerkProcessor::wanderDuration (double even)
{
    const float r = juce::jlimit (0.0f, 0.999f, wanderRng.next() * 0.5f + 0.5f);
    double d = even;
    if (p.progRhythm == 1) d = r < 0.25f ? even * 2.0 : even;                                     // GENTLE
    else if (p.progRhythm == 2) { const double m[] { 0.5, 1.0, 1.0, 2.0 }; d = even * m[(int) (r * 4.0f)]; }   // VARIED
    else if (p.progRhythm == 3) { const double m[] { 0.25, 0.5, 0.5, 1.0 }; d = even * m[(int) (r * 4.0f)]; } // RESTLESS
    return juce::jmax (2.0, d);
}

void DriftwerkProcessor::advanceWander (double beat, double even)
{
    auto rnd = [this] { return juce::jlimit (0.0f, 1.0f, wanderRng.next() * 0.5f + 0.5f); };
    if (! wanderOn || beat < wanderStart - 1.0e-6 || beat > wanderStart + wanderLen + 64.0)
    {
        // Start on this bar from the home chord, or carry on from here after a jump in the song.
        wanderStart = std::floor (beat / 4.0) * 4.0;
        if (! wanderOn)
        {
            wanderCount.store (0, std::memory_order_relaxed);
            wanderCur = 0 * 16 + harm::TNinth;
            pushWander (wanderCur);
        }
        wanderLen = wanderDuration (even);
        wanderNext = harm::wanderPick (p.scale, wanderCur, rnd);
        wanderNextCode.store (wanderNext, std::memory_order_relaxed);
        wanderOn = true;
    }
    while (beat >= wanderStart + wanderLen)
    {
        wanderStart += wanderLen;
        wanderCur = wanderNext;
        pushWander (wanderCur);
        wanderLen = wanderDuration (even);
        wanderNext = harm::wanderPick (p.scale, wanderCur, rnd);
        wanderNextCode.store (wanderNext, std::memory_order_relaxed);
    }
}

//==============================================================================
// CHORDS layer: plays the same chord as the pads (and the arp in PADS mode), voice-led.
// With the arp on SCALE, chord tones outside the scale are left out, so it can never clash.
void DriftwerkProcessor::updateChordLayer()
{
    const int keyRoot = p.root + midiTranspose;
    const long long key = (((harmKey * 128 + (keyRoot + 64)) * 8 + p.chdVoices) * 8 + (p.chdOct + 4)) * 4 + p.arpFollow;
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
            const int iv = curHarm.iv[k];
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

    const int mode = juce::jlimit (0, chordStepsStyle, p.chdMode);
    if (! p.chdOn || mode != chordStepsStyle) hitStepNow.store (-1, std::memory_order_relaxed);
    if (! p.chdOn)
    {
        if (chdWasOn) chordLayer.hold (false);
        chdWasOn = false;
        chdLastBeat = -1;
        return;
    }
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
        case 4: if (unitChanged (4.0) || changed) chordLayer.strike ((int) spb, (int) (sr * 0.045)); break; // STRUM
        default:                                                                                  // STEPS: the HITS pattern
        {
            if (changed) chordLayer.retarget();
            const double rate = hitRateBeats[juce::jlimit (0, 4, p.chdRate)];
            const long long u = (long long) std::floor (curBeat / rate);
            if (u == chdLastBeat) break;
            chdLastBeat = u;
            const int steps = p.chdSteps;
            const int st = (int) (((u % steps) + steps) % steps);
            // VARY: at the top of each loop a few free steps get a new weight.
            if (st == 0 && p.chdVary > 0.0f)
                for (int i = 1; i < numHitSteps; ++i)
                    if (hitForce[(size_t) i].load (std::memory_order_relaxed) == 0 && hitRng.next() * 0.5f + 0.5f < p.chdVary * 0.5f)
                        hitWeight[(size_t) i].store (juce::jlimit (0.01f, 0.99f, hitRng.next() * 0.5f + 0.5f), std::memory_order_relaxed);
            hitStepNow.store (st, std::memory_order_relaxed);
            if (hitOn (st, p.chdDensity))
            {
                int gap = 1;                                    // GATE: a share of the time until the next hit
                while (gap < steps && ! hitOn ((st + gap) % steps, p.chdDensity)) ++gap;
                chordLayer.strike (juce::jmax (64, (int) ((double) gap * rate * spb * p.chdGate)), 0);
            }
            break;
        }
    }
}

// The notes the LOOPS may play: the chord's notes, the scale, or root and fifth, around middle C.
void DriftwerkProcessor::updateLoopPitches()
{
    const int keyRoot = p.root + midiTranspose;
    const long long key = (((harmKey * 128 + (keyRoot + 64)) * 4 + p.loopNotes) * 8 + (p.loopOct + 4)) * 4 + (p.tune != 0.0f ? 1 : 0);
    if (key == loopPitchKey) return;
    loopPitchKey = key;
    const float base = (float) (keyRoot + 24 + 12 * p.loopOct) + p.tune;
    int n = 0;
    if (p.loopNotes == 1)
    {
        for (int x : scales[(size_t) juce::jlimit (0, (int) scales.size() - 1, p.scale)]) loopPitches[n++] = base + (float) x;
    }
    else if (p.loopNotes == 2)
    {
        loopPitches[n++] = base + (float) chordOffsetTarget;
        loopPitches[n++] = base + (float) chordOffsetTarget + 7.0f;
    }
    else
    {
        const int m = curHarm.ivMask();
        for (int k = 0; k < 12; ++k) if ((m >> k) & 1) loopPitches[n++] = base + (float) (chordOffsetTarget + k);
    }
    loopPitchCount = n;
}

bool DriftwerkProcessor::hitOn (int i, float density) const
{
    if (i < 0 || i >= numHitSteps) return false;
    const int f = hitForce[(size_t) i].load (std::memory_order_relaxed);
    if (f != 0) return f > 0;
    if (i == 0) return density > 0.001f;                   // the first step anchors the pattern
    return hitWeight[(size_t) i].load (std::memory_order_relaxed) < density;
}

bool DriftwerkProcessor::hitOnNow (int i) const { return hitOn (i, apvts.getRawParameterValue ("chd.density")->load()); }

void DriftwerkProcessor::setHit (int i, bool on)
{
    if (i >= 0 && i < numHitSteps) hitForce[(size_t) i].store (on ? 1 : -1);
}

void DriftwerkProcessor::newHits()
{
    juce::Random r;
    for (int i = 0; i < numHitSteps; ++i)
    {
        hitWeight[(size_t) i].store (i == 0 ? 0.0f : 0.01f + 0.98f * r.nextFloat());
        hitForce[(size_t) i].store (0);
    }
}

juce::String DriftwerkProcessor::getHitString() const
{
    juce::StringArray w, f;
    for (int i = 0; i < numHitSteps; ++i) { w.add (juce::String (hitWeight[(size_t) i].load(), 3)); f.add (juce::String (hitForce[(size_t) i].load())); }
    return w.joinIntoString (",") + "|" + f.joinIntoString (",");
}

void DriftwerkProcessor::setHitString (const juce::String& s)
{
    juce::StringArray parts, w, f;
    parts.addTokens (s, "|", "");
    if (parts.size() < 2) return;
    w.addTokens (parts[0], ",", ""); f.addTokens (parts[1], ",", "");
    for (int i = 0; i < numHitSteps; ++i)
    {
        if (i < w.size()) hitWeight[(size_t) i].store (juce::jlimit (0.0f, 1.0f, w[i].getFloatValue()));
        if (i < f.size()) hitForce[(size_t) i].store (juce::jlimit (-1, 1, f[i].getIntValue()));
    }
}

//==============================================================================
// SLIME, PULSE, TEXTURE: the notes they may use (the chord now, and the scale).
void DriftwerkProcessor::updateWeirdPitches()
{
    const int keyRoot = p.root + midiTranspose;
    const long long key = ((harmKey * 128 + (keyRoot + 64)) * 16 + p.scale);
    if (key == weirdPitchKey) return;
    weirdPitchKey = key;
    const int m = curHarm.ivMask();
    chordPcCount = 0;
    for (int k = 0; k < 12; ++k)
        if ((m >> k) & 1) chordPcs[chordPcCount++] = (((keyRoot + chordOffsetTarget + k) % 12) + 12) % 12;
    if (chordPcCount == 0) chordPcs[chordPcCount++] = ((keyRoot % 12) + 12) % 12;
    scalePcCount = 0;
    for (int x : scales[(size_t) juce::jlimit (0, (int) scales.size() - 1, p.scale)])
        if (scalePcCount < 12) scalePcs[scalePcCount++] = (((keyRoot + x) % 12) + 12) % 12;
    texture.pitchCount = chordPcCount;
    for (int k = 0; k < chordPcCount; ++k) texture.pitches[k] = (float) chordPcs[k];
}

void DriftwerkProcessor::playSlime (const Organism::Event& e)
{
    auto& rng = weirdRng;
    const float w = p.slimeWeird;
    const int keyRoot = p.root + midiTranspose;
    const int base = keyRoot + 24 + 12 * p.slimeOct;
    // The ear's place left to right picks the note: chord tones, and with WEIRD sometimes the scale.
    const bool passing = dw::uni (rng) < w * 0.3f;
    const int* set = passing ? scalePcs : chordPcs;
    const int setN = passing ? scalePcCount : chordPcCount;
    int notes[40]; int n = 0;
    for (int m = base; m < base + 12 * p.slimeRange && n < 40; ++m)
        for (int k = 0; k < setN; ++k)
            if (((m % 12) + 12) % 12 == set[k]) { notes[n++] = m; break; }
    if (n == 0) return;
    const float pos = juce::jlimit (0.0f, 0.999f, e.x);
    const int idx = juce::jlimit (0, n - 1, (int) (pos * (float) n));
    using SV = dw::SlimeVoices;
    int type = p.slimeVoice;
    if (type == slimeMixed) type = e.y < 0.4f ? SV::Tine : (e.y < 0.7f ? SV::Drop : SV::Breath);
    else if (type == slimeRandom)   // RANDOM: every ear has a voice of its own
    {
        static const int perEar[Organism::maxEars] { SV::Glass, SV::Pluck, SV::Marimba, SV::Drop, SV::Vowel, SV::Chirp,
                                                     SV::Tine, SV::Bowed, SV::Bubble, SV::Breath, SV::Grain, SV::Glass };
        type = perEar[juce::jlimit (0, Organism::maxEars - 1, e.ear)];
    }
    else if (type > slimeMixed) type -= 1;   // the menu has MIXED at 3, the voices carry on after it
    const float pan = (pos * 2.0f - 1.0f) * 0.8f;
    const float vel = 0.3f + 0.45f * e.strength + 0.15f * dw::uni (rng);
    const float detune = dw::uni (rng) < w * 0.35f ? (dw::uni (rng) * 2.0f - 1.0f) * (15.0f + 30.0f * w) : 0.0f;
    static const float glides[] { -12.0f, -7.0f, -5.0f, 5.0f, 7.0f, 12.0f };
    const float glide = dw::uni (rng) < w * 0.3f ? glides[(int) (dw::uni (rng) * 6.0f)] : 0.0f;
    const bool reverse = dw::uni (rng) < w * 0.25f;
    const float tune = p.tune;
    // PLAYS AS: one note, a chord, or an arpeggio. ALL THREE lets each ear choose: a strong touch
    // more often a chord, a low ear an arpeggio.
    int style = p.slimeStyle;
    if (style == 3)
    {
        const float r = dw::uni (rng);
        style = e.strength > 0.6f && r < 0.55f ? 1 : (r < 0.25f + 0.25f * e.y ? 2 : 0);
    }
    if (style != 0)
    {
        // The chord's own tones over the slime's range (and an octave above, to stack on).
        int cn[48]; int nc = 0;
        for (int m = base; m < base + 12 * (p.slimeRange + 1) && nc < 48; ++m)
            for (int k = 0; k < chordPcCount; ++k)
                if (((m % 12) + 12) % 12 == chordPcs[k]) { cn[nc++] = m; break; }
        if (nc >= 3)
        {
            const int inRange = juce::jmax (1, nc - chordPcCount);   // where in the range the ear sits picks the lowest note
            const int low = juce::jlimit (0, nc - 3, (int) (pos * (float) inRange));
            int played = 0;
            if (style == 1)
            {
                // CHORD: three or four chord tones up from there, a little strummed (more with WEIRD).
                const int count = juce::jmin (nc - low, e.strength > 0.55f ? 4 : 3);
                const float strum = 0.006f + 0.035f * w * dw::uni (rng);
                const bool down = dw::uni (rng) < 0.3f;
                const float each = vel * (count > 3 ? 0.6f : 0.68f);
                for (int k = 0; k < count; ++k)
                {
                    const int order = down ? count - 1 - k : k;
                    const float spread = ((float) k / (float) juce::jmax (1, count - 1) - 0.5f) * 0.5f;
                    slimeVoices.trigger (type, (float) cn[low + k] + tune, each * (0.9f + 0.2f * dw::uni (rng)), pan + spread,
                                         strum * (float) order, k == 0 ? detune : detune * 0.5f, k == 0 ? glide : 0.0f, reverse && k == 0);
                    ++played;
                }
            }
            else
            {
                // ARPEGGIO: three to six chord tones, the first now and the rest on the arp's clock;
                // high ears climb, low ears fall, the middle goes up and back. WEIRD shuffles a few.
                const int count = 3 + (int) (e.strength * 3.0f);
                double step = (double) arp.clockPeriod;
                while (step < sr * 0.09) step *= 2.0;
                while (step > sr * 0.45) step *= 0.5;
                double first = arp.samplesSinceClock < arp.clockPeriod ? (double) (arp.clockPeriod - arp.samplesSinceClock) : step;   // the arp's next tick
                while (first < step * 0.35) first += step;
                const int shape = e.y < 0.35f ? 0 : (e.y < 0.65f ? 2 : 1);   // 0 up, 1 down, 2 up and down
                const int span = juce::jmin (nc - low, shape == 2 ? (count + 2) / 2 : count);
                for (int k = 0; k < count; ++k)
                {
                    int j;
                    if (shape == 0) j = k % span;
                    else if (shape == 1) j = span - 1 - (k % span);
                    else { const int cyc = juce::jmax (1, 2 * span - 2); const int q = k % cyc; j = q < span ? q : cyc - q; }
                    if (k > 0 && dw::uni (rng) < w * 0.25f) j = (int) (dw::uni (rng) * (float) span * 0.999f);
                    const float when = k == 0 ? 0.0f : (float) ((first + step * (k - 1)) / sr);
                    slimeVoices.trigger (type, (float) cn[low + j] + tune, vel * (0.72f - 0.05f * (float) k) * (0.9f + 0.2f * dw::uni (rng)),
                                         pan + (dw::uni (rng) - 0.5f) * 0.3f, when, k == 0 ? detune : 0.0f, k == 0 ? glide : 0.0f, false);
                    ++played;
                }
            }
            slimeEarNote[(size_t) juce::jlimit (0, Organism::maxEars - 1, e.ear)].store (cn[low], std::memory_order_relaxed);
            slimeNotes.fetch_add (played, std::memory_order_relaxed);
            return;
        }
    }
    slimeVoices.trigger (type, (float) notes[idx] + tune, vel, pan, 0.0f, detune, glide, reverse);
    if (type == SV::Bubble || type == SV::Chirp)   // bubbles come in a little cluster, birds call a few times
    {
        const int more = 1 + (int) (dw::uni (rng) * (type == SV::Chirp ? 4.0f : 3.0f));
        float delay = 0.0f;
        for (int x = 0; x < more; ++x)
        {
            delay += type == SV::Chirp ? 0.09f + 0.06f * dw::uni (rng) : 0.04f + 0.12f * dw::uni (rng);
            const float step = type == SV::Chirp ? (dw::uni (rng) < 0.5f ? 0.0f : (dw::uni (rng) < 0.5f ? -2.0f : 2.0f))
                                                 : 3.0f + 9.0f * dw::uni (rng);
            slimeVoices.trigger (type, (float) notes[idx] + tune + step, vel * (0.5f + 0.4f * dw::uni (rng)),
                                 pan + (dw::uni (rng) - 0.5f) * 0.3f, delay, 0.0f, 0.0f, false);
        }
    }
    // WEIRD: a swarm of quiet neighbours
    if (dw::uni (rng) < w * 0.3f)
    {
        float delay = 0.0f;
        const int extra = 2 + (int) (dw::uni (rng) * 3.0f);
        int j = idx;
        for (int x = 0; x < extra; ++x)
        {
            delay += 0.07f + 0.15f * dw::uni (rng);
            j = juce::jlimit (0, n - 1, j + (dw::uni (rng) < 0.5f ? -1 : 1) * (1 + (int) (dw::uni (rng) * 2.0f)));
            slimeVoices.trigger (type, (float) notes[j] + tune, vel * 0.45f, pan + (dw::uni (rng) - 0.5f) * 0.4f, delay, detune, 0.0f, false);
        }
    }
    slimeEarNote[(size_t) juce::jlimit (0, Organism::maxEars - 1, e.ear)].store (notes[idx], std::memory_order_relaxed);
    slimeNotes.fetch_add (1, std::memory_order_relaxed);
}

void DriftwerkProcessor::slimeDrum (const Organism::Event& e)
{
    // PULSE style ORGANISM: high ears tick, low ears thump.
    const int lane = e.y < 0.25f ? dw::Pulse::Tick : (e.y < 0.45f ? dw::Pulse::Wood : (e.y < 0.65f ? dw::Pulse::Brush : (e.y < 0.82f ? dw::Pulse::Skin : dw::Pulse::Thump)));
    const int variant = dw::uni (weirdRng) < 0.3f ? 1 : 0;
    const float vel = 0.4f + 0.5f * e.strength;
    pulseDrums.schedule (lane, vel, pulseFreq (lane, variant), (e.x * 2.0f - 1.0f) * 0.7f, (int) (dw::uni (weirdRng) * 0.01f * (float) sr), variant);
}

float DriftwerkProcessor::pulseFreq (int lane, int variant)
{
    auto fold = [] (int m, int lo) { while (m < lo) m += 12; while (m >= lo + 12) m -= 12; return m; };
    // Each kit tunes its drums to the key in its own register: low, snare, wood, (hat), tick.
    static const int base[dw::Pulse::NumKits][dw::Pulse::Lanes] {
        { 26, 45, 72, 0, 96 }, { 24, 52, 76, 0, 84 }, { 28, 52, 74, 0, 96 }, { 45, 69, 84, 0, 96 },
        { 36, 57, 64, 0, 76 }, { 36, 60, 84, 0, 100 }, { 33, 55, 79, 0, 98 } };
    const int* b = base[dw::Pulse::clampKit (p.pulseKit)];
    const int keyRoot = p.root + midiTranspose;
    auto& rng = weirdRng;
    const int tone = chordPcs[(int) (dw::uni (rng) * (float) juce::jmax (1, chordPcCount)) % juce::jmax (1, chordPcCount)];
    switch (lane)
    {
        case dw::Pulse::Thump: return dw::midiToHz ((float) fold (keyRoot, b[0]) + p.tune);
        case dw::Pulse::Skin:  return dw::midiToHz ((float) fold (keyRoot + chordOffsetTarget, b[1]) + p.tune);
        case dw::Pulse::Wood:  return dw::midiToHz ((float) fold (tone, b[2]) + p.tune - (variant == 2 ? 12.0f : 0.0f));
        case dw::Pulse::Brush: return 6200.0f * std::exp2 ((p.pulseTone - 0.5f) * 1.2f) * (0.85f + 0.3f * dw::uni (rng));
        default:               return dw::midiToHz ((float) fold (tone, b[4]) + p.tune);
    }
}

int DriftwerkProcessor::pulseLaneLength (int lengthMode, int lane)
{
    return pulseLengths[juce::jlimit (0, 5, lengthMode)][juce::jlimit (0, pulseLanes - 1, lane)];
}

void DriftwerkProcessor::pulseStep (long long k, double stepSamples)
{
    k = ((k % 720720) + 720720) % 720720;          // every lane length divides this, so the lanes stay in step
    pulseStepNow.store ((int) k, std::memory_order_relaxed);
    auto& rng = weirdRng;
    const bool organismStyle = p.pulseStyle == pulseOrganismStyle, drill = p.pulseStyle == pulseDrillStyle;
    const bool glitchy = p.pulseKit >= 5;            // GLITCH and IDM roll more
    const int inBar = (int) (k % 16), within = (int) (k % 4);
    const long long bar = k / 16;
    // CHOP: every four steps the beat may jump to another slice of the pattern, run backwards,
    // or repeat one step (a beat-slicer).
    if (within == 0)
    {
        chopChunk = -1; chopReverse = chopHold = false;
        if (! organismStyle && p.pulseChop > 0.0f && dw::uni (rng) < p.pulseChop * 0.75f)
        {
            chopChunk = (int) (dw::uni (rng) * 4.0f) & 3;
            chopReverse = dw::uni (rng) < p.pulseChop * 0.35f;
            chopHold = ! chopReverse && dw::uni (rng) < p.pulseChop * 0.3f;
        }
    }
    // FILL: the last four steps of every fourth bar may roll into the next.
    if (inBar == 0) fillNow = false;
    if (inBar == 12) fillNow = ! organismStyle && bar % 4 == 3 && dw::uni (rng) < p.pulseFill;
    const float swingDelay = (k % 2 == 1) ? p.pulseSwing * 0.5f * (float) stepSamples : 0.0f;
    const bool breakSolo = p.brkOn && ! p.brkDrums;   // BREAK on its own: the pattern still runs (for the grid), silently
    for (int lane = 0; lane < pulseLanes; ++lane)
    {
        const int len = pulseLaneLength (p.pulseLength, lane);
        const int pos = (int) (k % len);
        if (pos == 0)   // VARY: each time round, a few steps change for one loop (not when the slime drums)
            for (int s = 0; s < pulseSteps; ++s)
                pulseFlip[lane * pulseSteps + s] = ! organismStyle && dw::uni (rng) < p.pulseVary * 0.16f;
        int src = pos;
        if (chopChunk >= 0) src = (chopChunk * 4 + (chopHold ? 0 : (chopReverse ? 3 - within : within))) % len;
        pulseLanePos[(size_t) lane].store (src, std::memory_order_relaxed);
        if (breakSolo) continue;
        const int idx = lane * pulseSteps + src;
        bool on = pulseOn (lane, src, p.pulseDensity);
        const bool flipped = pulseFlip[idx];
        if (flipped) on = ! on;
        if (fillNow && inBar >= 12 && lane <= dw::Pulse::Wood) on = false;   // the fill takes over the drums
        if (! on) continue;
        if (dw::uni (rng) < p.pulseWeird * 0.1f) continue;               // WEIRD: a hit goes missing
        const float w = pulseW[(size_t) idx].load (std::memory_order_relaxed);
        float vel = pulseF[(size_t) idx].load (std::memory_order_relaxed) > 0 ? 0.8f : 0.4f + 0.5f * juce::jmin (1.0f, w);
        if (flipped) vel *= 0.55f;                                         // VARY's extra hits are ghost notes
        vel *= 1.0f + (dw::uni (rng) * 2.0f - 1.0f) * p.pulseHuman * 0.3f;
        const int wait = (int) (swingDelay + dw::uni (rng) * p.pulseHuman * 0.018f * (float) sr);
        int variant = 0;
        float pan = 0.0f;
        switch (lane)
        {
            case dw::Pulse::Thump: variant = (src % 8 == 0) ? 0 : 1; break;
            case dw::Pulse::Skin:  variant = (w < 0.6f && dw::uni (rng) < 0.4f) ? 1 : 0; pan = -0.25f; break;
            case dw::Pulse::Wood:  variant = dw::uni (rng) < 0.15f ? 1 : (dw::uni (rng) < p.pulseWeird * 0.3f ? 2 : 0); pan = (dw::uni (rng) * 2.0f - 1.0f) * 0.5f; break;
            case dw::Pulse::Brush: variant = dw::uni (rng) < p.pulseWeird * 0.3f ? 2 : ((src % 4 == 2 && dw::uni (rng) < 0.3f) ? 1 : 0); pan = (src % 4 < 2 ? -0.35f : 0.35f); break;
            default:               variant = dw::uni (rng) < 0.2f ? 1 : 0; pan = (dw::uni (rng) * 2.0f - 1.0f) * 0.7f; break;
        }
        if (chopReverse && (lane == dw::Pulse::Skin || lane == dw::Pulse::Brush) && dw::uni (rng) < 0.5f) variant = 2;   // backwards slices swell
        const float drop = lane != dw::Pulse::Brush && dw::uni (rng) < p.pulseWeird * 0.15f ? 0.75f : 1.0f;
        const float freq = pulseFreq (lane, variant) * drop;
        // STUTTER: the hit becomes a roll, faster or slower, its pitch sliding up or down.
        const float rollChance = p.pulseStutter * 0.22f * (drill ? 2.0f : 1.0f) * (glitchy ? 1.4f : 1.0f) + (drill ? 0.05f : 0.0f);
        if (dw::uni (rng) < rollChance)
        {
            static const int divs[] { 2, 3, 4, 6, 8 };
            const int div = divs[juce::jlimit (0, 4, (int) (dw::uni (rng) * ((drill || glitchy) ? 5.0f : 3.0f)))];
            const int reps = juce::jmin (2 * div, 2 + (int) (dw::uni (rng) * (2.0f + 6.0f * p.pulseStutter)));
            const float slide = (dw::uni (rng) < 0.5f ? -1.0f : 1.0f) * dw::uni (rng) * (2.0f + 10.0f * p.pulseStutter);
            const bool swell = dw::uni (rng) < 0.4f;
            for (int r = 0; r < reps; ++r)
            {
                const float fr = reps > 1 ? (float) r / (float) (reps - 1) : 0.0f;
                const float v = vel * (swell ? 0.35f + 0.65f * fr : 1.0f - 0.55f * fr);
                const float bend = std::exp2 (slide * fr / 12.0f);
                pulseDrums.schedule (lane, v, freq * bend, juce::jlimit (-1.0f, 1.0f, pan + (fr - 0.5f) * 0.4f),
                                     wait + (int) ((double) r * stepSamples / (double) div), variant, drop * bend);
            }
            continue;
        }
        pulseDrums.schedule (lane, vel, freq, pan, wait, variant, drop);
        if (dw::uni (rng) < p.pulseWeird * 0.15f)                          // WEIRD: a ratchet
        {
            const int reps = 1 + (int) (dw::uni (rng) * 2.0f);
            for (int r = 1; r <= reps; ++r)
                pulseDrums.schedule (lane, vel * std::pow (0.55f, (float) r), freq, pan, wait + (int) ((double) r * stepSamples / 3.0), variant, drop);
        }
    }
    // The fill itself: snare and wood climbing, in 32nds when FILL is high, and a backwards
    // swell that lands on the next downbeat.
    if (fillNow && inBar >= 12 && ! breakSolo)
    {
        const int i = inBar - 12;
        const int subs = (p.pulseFill > 0.5f && dw::uni (rng) < p.pulseFill) ? 2 : 1;
        for (int s = 0; s < subs; ++s)
        {
            const float pos = ((float) i + (float) s / (float) subs) / 4.0f;          // 0..1 through the fill
            const int wait = (int) (swingDelay + (double) s * stepSamples / (double) subs + dw::uni (rng) * p.pulseHuman * 0.01f * (float) sr);
            const int lane = ((i + s) % 2 == 0) ? dw::Pulse::Skin : dw::Pulse::Wood;
            const float pitch = std::exp2 ((lane == dw::Pulse::Skin ? 7.0f : -5.0f) * pos / 12.0f);
            pulseDrums.schedule (lane, 0.45f + 0.45f * pos, pulseFreq (lane, 0) * pitch, -0.5f + pos, wait, 0, pitch);
        }
        if (i == 0) pulseDrums.schedule (dw::Pulse::Thump, 0.6f, pulseFreq (dw::Pulse::Thump, 1), 0.0f, (int) swingDelay, 1);
        if (i == 3)
            pulseDrums.schedule (dw::Pulse::Brush, 0.7f, pulseFreq (dw::Pulse::Brush, 2), 0.0f,
                                 juce::jmax (0, (int) (stepSamples - 0.16 * sr)), 2);
    }
}

void DriftwerkProcessor::generatePulse (int style)
{
    auto& rng = weirdRng;
    for (int lane = 0; lane < pulseLanes; ++lane)
        for (int step = 0; step < pulseSteps; ++step)
        {
            const int k = lane * pulseSteps + step;
            float w = 0.0f;
            if (style >= 0 && style < pulseNumStyles)
            {
                const float sw = pulseStyleW[style][lane][step];
                w = sw > 0.0f ? sw * (0.62f + 0.76f * dw::uni (rng)) : (dw::uni (rng) < 0.08f ? 0.18f * dw::uni (rng) : 0.0f);
            }
            pulseW[(size_t) k].store (juce::jlimit (0.0f, 1.0f, w), std::memory_order_relaxed);
            pulseF[(size_t) k].store (0, std::memory_order_relaxed);
        }
}

bool DriftwerkProcessor::pulseOn (int lane, int step, float density) const
{
    if (lane < 0 || lane >= pulseLanes || step < 0 || step >= pulseSteps) return false;
    const int k = lane * pulseSteps + step;
    const int f = pulseF[(size_t) k].load (std::memory_order_relaxed);
    if (f != 0) return f > 0;
    const float w = pulseW[(size_t) k].load (std::memory_order_relaxed);
    return density > 0.001f && w > 0.0f && w >= 1.0f - density;
}

bool DriftwerkProcessor::pulseOnNow (int lane, int step) const { return pulseOn (lane, step, apvts.getRawParameterValue ("pulse.density")->load()); }

void DriftwerkProcessor::setPulse (int lane, int step, bool on)
{
    if (lane >= 0 && lane < pulseLanes && step >= 0 && step < pulseSteps)
    {
        const int k = lane * pulseSteps + step;
        pulseF[(size_t) k].store (on ? 1 : -1);
        if (on && pulseW[(size_t) k].load() < 0.5f) pulseW[(size_t) k].store (0.75f);
    }
}

juce::String DriftwerkProcessor::getPulseString() const
{
    juce::String out;
    for (int k = 0; k < pulseLanes * pulseSteps; ++k)
    {
        const int f = pulseF[(size_t) k].load();
        out << (f > 0 ? "+" : (f < 0 ? "-" : ".")) << juce::String (juce::jlimit (0, 99, (int) std::lround (pulseW[(size_t) k].load() * 99.0f))).paddedLeft ('0', 2);
    }
    return out;
}

void DriftwerkProcessor::setPulseString (const juce::String& s)
{
    if (s.length() < pulseLanes * pulseSteps * 3) return;
    for (int k = 0; k < pulseLanes * pulseSteps; ++k)
    {
        const auto c = s[k * 3];
        pulseF[(size_t) k].store (c == '+' ? 1 : (c == '-' ? -1 : 0));
        pulseW[(size_t) k].store (juce::jlimit (0.0f, 1.0f, (float) s.substring (k * 3 + 1, k * 3 + 3).getIntValue() / 99.0f));
    }
}

//==============================================================================
void DriftwerkProcessor::renderSample (float& outL, float& outR)
{
    const float fsr = (float) sr;
    float* const lg = layerGain;
    for (int k = 0; k < numLayers; ++k)
        if (lg[k] != layerTarget[k])
        {
            lg[k] += (layerTarget[k] - lg[k]) * layerCoef;
            if (std::abs (lg[k] - layerTarget[k]) < 1.0e-4f) lg[k] = layerTarget[k];
        }
    auto meter = [this] (int k, float a, float b) { layerPeak[k] = std::max (layerPeak[k], std::max (std::abs (a), std::abs (b))); };

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
        outs[O_DRONE] = (droneLp + air) * 0.2f * p.droneLevel * lg[L_DRONE];
        meter (L_DRONE, outs[O_DRONE], 0.0f);
    }

    // ---- VCOs
    {
        const float n1 = baseNote + 12.0f * (float) p.vco1Oct + p.vco1Fine
                       + 12.0f * (in (I_VCO1_PITCH) + in (I_VCO1_FM) * p.vco1Fm * 2.0f);
        const double f1 = midiToHz (n1);
        const float pw1 = p.vco1Pw + in (I_VCO1_PWM) * p.vco1Pwm * 0.42f;
        // WAVE: when it is not PATCH, both main jacks play the chosen wave, so whatever is patched
        // plays it. MOVE lets SHAPE follow the note (ENV 1) and a slow drift; PWM moves it too.
        float shapeNow = p.arpShape;
        if (p.arpWave != 0)
        {
            waveDriftPh += 0.07f / (float) sr; if (waveDriftPh >= 1.0f) waveDriftPh -= 1.0f;
            shapeNow = clampf (p.arpShape + p.arpMove * (0.65f * outs[O_ENV1] + 0.3f * std::sin (kTwoPi * waveDriftPh))
                               + in (I_VCO1_PWM) * p.vco1Pwm * 0.42f, 0.0f, 1.0f);
            outs[O_VCO1_SAW] = outs[O_VCO1_PULSE] = 0.5f * wave1.process (p.arpWave, f1, sr, shapeNow, p.arpFold, p.arpTable);
        }
        else
        {
            outs[O_VCO1_SAW] = 0.5f * vco1Saw.process (BlepOsc::Saw, f1, sr);
            outs[O_VCO1_PULSE] = 0.5f * vco1Pulse.process (BlepOsc::Square, f1, sr, pw1);
        }
        outs[O_VCO1_SUB] = 0.5f * vco1Sub.process (BlepOsc::Square, f1 * 0.5, sr);
        arpShapeNowLocal = shapeNow;

        const float n2 = baseNote + 12.0f * (float) p.vco2Oct + p.vco2Fine
                       + 12.0f * (in (I_VCO2_PITCH) + in (I_VCO2_FM) * p.vco2Fm * 2.0f);
        const double f2 = midiToHz (n2);
        const float pw2 = p.vco2Pw + in (I_VCO2_PWM) * p.vco2Pwm * 0.42f;
        if (p.arpWave != 0)
            outs[O_VCO2_SAW] = outs[O_VCO2_PULSE] = 0.5f * wave2.process (p.arpWave, f2, sr, shapeNow, p.arpFold, p.arpTable);
        else
        {
            outs[O_VCO2_SAW] = 0.5f * vco2Saw.process (BlepOsc::Saw, f2, sr);
            outs[O_VCO2_PULSE] = 0.5f * vco2Pulse.process (BlepOsc::Square, f2, sr, pw2);
        }
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
        const float dm = dream.process (curHarm.iv, 6, chordBase,
                                        p.dreamRate, p.dreamTone, p.dreamWarp, p.dreamSound,
                                        dreamMine.load (std::memory_order_acquire), dl, dr);
        outs[O_DREAM] = dm * p.dreamLevel * 1.6f * lg[L_DREAM];
        dl *= p.dreamLevel * lg[L_DREAM]; dr *= p.dreamLevel * lg[L_DREAM];
        meter (L_DREAM, dl, dr);

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
        outs[O_GRAIN] = gm * p.gLevel * lg[L_GRAIN];
        gl *= p.gLevel * lg[L_GRAIN]; gr *= p.gLevel * lg[L_GRAIN];
        meter (L_GRAIN, gl, gr);
    }
    // ---- CHORDS layer (stereo, straight to the master bus)
    float cl = 0.0f, cr = 0.0f;
    updateChordLayer();
    chordLayer.process (chordGlideCoef, p.chdWave, p.chdTone, p.chdAttack, p.chdRelease, p.chdWidth, cl, cr);
    cl *= p.chdLevel * 0.9f * lg[L_CHORDS]; cr *= p.chdLevel * 0.9f * lg[L_CHORDS];
    meter (L_CHORDS, cl, cr);
    // ---- LOOPS layer (stereo, straight to the master bus)
    {
        float ll = 0.0f, lr = 0.0f;
        if (p.loopOn || tapeLoops.anySounding())
        {
            updateLoopPitches();
            tapeLoops.process (loopPitches, loopPitchCount, p.loopOn ? p.loopVoices : 0, p.loopLength, p.loopColour, p.loopTone,
                               p.loopSwell, p.loopDecay, p.loopDrift, p.loopWeird, ll, lr);
        }
        ll *= p.loopLevel * 1.4f * lg[L_LOOPS]; lr *= p.loopLevel * 1.4f * lg[L_LOOPS];
        meter (L_LOOPS, ll, lr);
        cl += ll; cr += lr;
    }
    // ---- SLIME notes (the organism's ears)
    if (p.slimePlay || slimeVoices.anySounding())
    {
        float a = 0.0f, b = 0.0f;
        slimeVoices.process (p.slimeTone, a, b);
        a *= p.slimeLevel * 3.0f * lg[L_SLIME]; b *= p.slimeLevel * 3.0f * lg[L_SLIME];
        meter (L_SLIME, a, b);
        cl += a; cr += b;
    }
    // ---- PULSE (part through the space, part dry after it)
    float pdl = 0.0f, pdr = 0.0f;
    if (p.pulseOn)
    {
        const double upb = p.pulseSpeed == 0 ? 4.0 : (p.pulseSpeed == 1 ? 2.0 : 1.0);
        const long long k = (long long) std::floor (beat * upb);
        if (k != pulseLastK)
        {
            if (pulseLastK == pulseNoStep || k == pulseLastK + 1) pulseStep (k, samplesPerBeat / upb);
            pulseLastK = k;
        }
        // BREAK: one slice per sixteenth, whatever STEP the drums use
        if (p.brkOn)
        {
            const long long k16 = (long long) std::floor (beat * 4.0);
            if (k16 != breakLastK)
            {
                if (breakLastK == pulseNoStep || k16 == breakLastK + 1)
                {
                    const auto* bl = breakNow.load (std::memory_order_acquire);
                    double rate = (p.brkFit && bl != nullptr) ? (60.0 * sr / samplesPerBeat) / bl->bpm : 1.0;
                    rate = juce::jlimit (0.25, 4.0, rate * std::exp2 ((double) p.brkPitch / 12.0));
                    const bool inSync = p.brkFit && std::abs (p.brkPitch) < 0.01f && rate > 0.2501 && rate < 3.999;
                    breakPlayer.step (bl, k16, samplesPerBeat / 4.0, rate, inSync, p.brkChop, p.brkStutter, p.brkReverse);
                }
                breakLastK = k16;
            }
        }
        else { breakLastK = pulseNoStep; breakPlayer.rollLeft = 0; }
    }
    else { pulseLastK = pulseNoStep; breakLastK = pulseNoStep; breakPlayer.rollLeft = 0; }
    if (p.pulseOn || pulseDrums.busy() || breakPlayer.busy())
    {
        float a = 0.0f, b = 0.0f;
        pulseDrums.process (p.pulseTone, a, b);
        const float g = p.pulseLevel * 2.2f * lg[L_PULSE];
        a *= g; b *= g;
        meter (L_PULSE, a, b);
        if (breakPlayer.busy())
        {
            float x = 0.0f, y = 0.0f;
            breakPlayer.process (p.brkDust, x, y);
            const float gb = p.brkLevel * breakGain * lg[L_BREAK];
            meter (L_BREAK, x * gb, y * gb);
            a += x * gb; b += y * gb;
            breakSliceNow.store (breakPlayer.sliceNow, std::memory_order_relaxed);
            breakPosNow.store (breakPlayer.posNow, std::memory_order_relaxed);
            breakHits.store (breakPlayer.hits, std::memory_order_relaxed);
        }
        cl += a * p.pulseSpace; cr += b * p.pulseSpace;
        pdl = a * (1.0f - 0.8f * p.pulseSpace); pdr = b * (1.0f - 0.8f * p.pulseSpace);
    }
    // ---- TEXTURE (the same)
    float tdl = 0.0f, tdr = 0.0f;
    if (p.texOn)
    {
        float a = 0.0f, b = 0.0f;
        texture.process (p.texA, p.texB - 1, p.texBlend, p.texDensity, p.texTone, p.texMotion, p.texKey, a, b);
        const float g = p.texLevel * 1.3f * lg[L_TEXTURE];
        meter (L_TEXTURE, a * g, b * g);
        cl += a * g * p.texSpace; cr += b * g * p.texSpace;
        tdl = a * g * (1.0f - 0.8f * p.texSpace); tdr = b * g * (1.0f - 0.8f * p.texSpace);
    }

    // ---- WORDS (the same: some into the space, the rest dry)
    float wdl = 0.0f, wdr = 0.0f;
    if (p.wordsOn || wordsPlayer.busy())
    {
        float a = 0.0f, b = 0.0f;
        wordsPlayer.process (p.wordsWobble, p.wordsRadio, a, b);
        const float g = p.wordsLevel * 1.8f * lg[L_WORDS];
        meter (L_WORDS, a * g, b * g);
        cl += a * g * p.wordsSpace; cr += b * g * p.wordsSpace;
        wdl = a * g * (1.0f - 0.75f * p.wordsSpace); wdr = b * g * (1.0f - 0.75f * p.wordsSpace);
    }

    bool dreamPatched = false, grainPatched = false;
    for (int k = 0; k < NumIns; ++k)
    {
        dreamPatched |= patchCache[(size_t) k] == O_DREAM;
        grainPatched |= patchCache[(size_t) k] == O_GRAIN;
    }

    // ---- OUTPUT + SPACE
    const float s1 = in (I_OUT1) * p.lvl1 * lg[L_OUT1];
    const float s2 = in (I_OUT2) * p.lvl2 * lg[L_OUT2];
    meter (L_OUT1, s1, 0.0f);
    meter (L_OUT2, s2, 0.0f);
    const float a1 = (p.pan1 + 1.0f) * 0.25f * kPi;
    const float a2 = (p.pan2 + 1.0f) * 0.25f * kPi;
    float l = s1 * std::cos (a1) + s2 * std::cos (a2);
    float r = s1 * std::sin (a1) + s2 * std::sin (a2);
    if (! dreamPatched) { l += dl; r += dr; }
    if (! grainPatched) { l += gl; r += gr; }
    // ---- BASS (mostly dry, in the middle, a little into the space)
    float bdry = 0.0f;
    if (p.bassOn || bassVoice.busy())
    {
        const float y = bassVoice.process (p.bassWave + 1, p.bassTable, p.bassShape, p.bassSub, p.bassCutoff, p.bassReso, p.bassEnv,
                                           p.bassDecay, p.bassDrive, p.bassGlide) * p.bassLevel * bassGain * lg[L_BASS];
        meter (L_BASS, y, 0.0f);
        cl += y * 0.15f; cr += y * 0.15f;
        bdry = y * 0.88f;
    }
    {
        const int keyRoot = p.root + midiTranspose;
        outs[O_BASS_PITCH] = (bassVoice.pitch - p.tune - (float) keyRoot) / 12.0f + 1.0f;   // the bass line, an octave up on a VCO
        outs[O_BASS_GATE] = p.bassOn && bassVoice.gate ? 1.0f : 0.0f;
    }
    l += cl; r += cr;

    // ---- MEMORY: everything is remembered (and, with DISSOLVE, the memories too); two heads
    // drift back into the past and play it in again, so it goes through the space once more.
    {
        float ml = 0.0f, mr = 0.0f;
        if (p.memOn) memory.process (true, p.memAgo, p.memLength, p.memSpeed, p.memBlur, p.memScatter, ml, mr);
        else memory.h[0].dur = memory.h[1].dur = 0.0f;
        memory.write (0.5f * (l + r) + 0.5f * (ml + mr) * (p.memOn ? p.memDissolve : 0.0f));
        ml *= p.memLevel * 1.5f * lg[L_MEMORY]; mr *= p.memLevel * 1.5f * lg[L_MEMORY];
        meter (L_MEMORY, ml, mr);
        l += ml; r += mr;
    }

    ensemble.process (l, r, p.ens);
    tape.process (l, r, p.tWow, p.tAge, p.tHiss, p.tCrush);
    echo.process (l, r, p.echoTime, p.echoFb, p.echoMix, p.echoWow);
    reverb.process (l, r, p.revSize, p.revDamp, p.revMix, p.shimmer);
    l += pdl + tdl + wdl + bdry; r += pdr + tdr + wdr + bdry;

    l *= p.vol * 2.0f; r *= p.vol * 2.0f;
    // DC blocker
    dcL = l - dcInL + 0.9995f * dcL; dcInL = l;
    dcR = r - dcInR + 0.9995f * dcR; dcInR = r;
    outL = dcL; outR = dcR;
    limiter.process (outL, outR);
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

    blockLen = juce::jmax (1, buffer.getNumSamples());
    updateBlockParams();

    // ---- Chord progression (changes exactly on bar lines, see applyProgression)
    {
        const float glideSec = (0.03f + p.progGlide * p.progGlide * 5.0f) * (p.progMode == 4 ? 2.0f : 1.0f);
        chordGlideCoef = onePoleCoef (glideSec / 4.0f, (float) sr);
        applyProgression (hostSynced ? hostPpqAtBlock : beatPos);
        keyPcNow.store ((((p.root + midiTranspose) % 12) + 12) % 12, std::memory_order_relaxed);
    }

    // ---- The organism: feed it (hand, loudness) and listen to its ears
    updateWeirdPitches();
    {
        organism.listening.store (p.slimePlay || (p.pulseOn && p.pulseStyle == pulseOrganismStyle), std::memory_order_relaxed);
        organism.handX.store (outs[O_CAM_X], std::memory_order_relaxed);
        organism.handY.store (outs[O_CAM_Y], std::memory_order_relaxed);
        organism.motion.store (outs[O_CAM_MOTION], std::memory_order_relaxed);
        organism.level.store (juce::jlimit (0.0f, 1.0f, outputLevel.load (std::memory_order_relaxed) * 1.6f), std::memory_order_relaxed);
        scheduleWords (buffer.getNumSamples());
        slimeVoices.grainSound = currentSample.load (std::memory_order_acquire);   // GRAIN SOUND plays this (never freed while we run)
        slimeVoices.grainStart = p.gPos;
        Organism::Event e;
        while (organism.popEvent (e))
        {
            if (p.slimePlay) playSlime (e);
            if (p.pulseOn && p.pulseStyle == pulseOrganismStyle) slimeDrum (e);
        }
    }
    // ---- PULSE pattern: a new style, or NEW
    if (p.pulseStyle != pulseStyleKnown.load (std::memory_order_relaxed)) { pulseStyleKnown.store (p.pulseStyle); generatePulse (p.pulseStyle); }
    if (const int req = pulseNewReq.load (std::memory_order_relaxed); req != pulseNewSeen) { pulseNewSeen = req; generatePulse (p.pulseStyle); }
    // ---- MEMORY: FORGET
    if (const int req = memForgetReq.load (std::memory_order_relaxed); req != memForgetSeen)
    {
        memForgetSeen = req;
        memory.forget();
        for (auto& x : memPeaks) x.store (0.0f, std::memory_order_relaxed);
        memPeakSeen = memory.peakHead;
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
    for (int k = 0; k < numLayers; ++k) { layerMeter[(size_t) k].store (layerPeak[k], std::memory_order_relaxed); layerPeak[k] = 0.0f; }
    grainPlayPos.store (grains.lastPlayPos, std::memory_order_relaxed);
    {
        float pk = 0.0f;
        for (int s2 = 0; s2 < n; ++s2) pk = std::max (pk, std::abs (L[s2]));
        outputLevel.store (std::max (pk, outputLevel.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
    }
    arpStep.store (arp.step);
    arpLength.store (p.length);
    progPhase.store (progPhaseNow, std::memory_order_relaxed);
    {
        const int req = loopShuffleReq.load (std::memory_order_relaxed);
        if (req != loopShuffleSeen) { loopShuffleSeen = req; tapeLoops.shuffle(); }
        for (int i = 0; i < dw::TapeLoops::V; ++i)
        {
            const auto& vo = tapeLoops.v[i];
            loopPhase[(size_t) i].store ((float) vo.phase, std::memory_order_relaxed);
            loopEnv[(size_t) i].store (vo.stage != 0 ? vo.env : 0.0f, std::memory_order_relaxed);
            loopNote[(size_t) i].store ((int) std::lround (vo.note - p.tune), std::memory_order_relaxed);
            loopRatio[(size_t) i].store (vo.ratio, std::memory_order_relaxed);
        }
    }
    {
        for (int l = 0; l < pulseLanes; ++l) pulseLaneHits[(size_t) l].store (pulseDrums.laneHits[l], std::memory_order_relaxed);
        arpShapeNow.store (arpShapeNowLocal, std::memory_order_relaxed);
        wordsNowWord.store (wordsPlayer.nowWord, std::memory_order_relaxed);
        wordsNowLevel.store (wordsPlayer.nowLevel, std::memory_order_relaxed);
        wordsPlayed.store (wordsPlayer.played, std::memory_order_relaxed);
        texEnergy[0].store (texture.s[0].energy, std::memory_order_relaxed);
        texEnergy[1].store (texture.s[1].energy, std::memory_order_relaxed);
        texEvents.store (texture.s[0].events + texture.s[1].events, std::memory_order_relaxed);
        const int ph = memory.peakHead;
        while (memPeakSeen != ph)
        {
            memPeaks[(size_t) memPeakSeen].store (memory.peaks[memPeakSeen], std::memory_order_relaxed);
            memPeakSeen = (memPeakSeen + 1) % dw::Memory::NumPeaks;
        }
        memPeakHead.store (ph, std::memory_order_relaxed);
        memSeconds.store ((float) ((double) memory.written / (double) dw::Memory::MSR), std::memory_order_relaxed);
        for (int k = 0; k < 2; ++k)
        {
            memHeadAgo[(size_t) k].store (memory.headAgoSeconds (k), std::memory_order_relaxed);
            memHeadWin[(size_t) k].store (p.memOn && memory.h[k].live ? memory.h[k].window : 0.0f, std::memory_order_relaxed);
            memHeadSpeed[(size_t) k].store ((float) memory.h[k].speed, std::memory_order_relaxed);
        }
    }

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
// Loudness: how much each preset's master volume is trimmed (dB) so they all play about as
// loud as each other (measured over 40 s, aiming at -16 LUFS). Presets not listed are near enough.
const std::pair<const char*, float> presetTrims[] {
    { "Singing Bowls", -3.0f }, { "Low Tide Breathing", 6.0f }, { "Airport Tapes", -1.5f },
    { "Berlin Night Drive", 2.5f }, { "Glass Harp", 3.5f }, { "Kalimba Rain", 5.5f },
    { "Toy Piano Ghost", 1.5f }, { "Black Tide", 3.5f }, { "Warped Cassette", -2.0f },
    { "Insect Choir", 6.0f }, { "Gravity Well", 3.0f }, { "Choir Of Rooms", -3.5f },
    { "Organ Sunday", 1.0f }, { "Glass Weather", 1.0f }, { "String Machine Dawn", 1.5f },
    { "Rain Stabs", 1.5f }, { "Bowed Iron Creep", 2.5f }, { "Wandering Hymn", -1.5f },
    { "Postcard Loop", 2.0f }, { "Orbiting Bells", 3.5f }, { "Reverse Garden", 1.5f },
    { "Slime Choir", -1.5f }, { "Heartbeat Moss", 1.0f }, { "Lazy Tape Afternoon", -1.5f },
    { "Rain on the Organism", 4.5f }, { "Memory Hall", 1.0f }, { "Night Creatures", 2.0f },
    { "Educational Film", -3.5f }, { "Broken Tape Break", -3.5f }, { "Drill Garden", -3.5f },
    { "Jungle Memory", -4.0f }, { "Wavetable Choir", 1.0f }, { "Puppet Strings", 1.5f },
    { "Theremin Fog", 3.5f }
};

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

juce::File DriftwerkProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Timewind").getChildFile ("Presets");
}

juce::Array<juce::File> DriftwerkProcessor::userPresets()
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.timewind");
    struct ByName { static int compareElements (const juce::File& a, const juce::File& b)
                    { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()); } };
    ByName cmp;
    files.sort (cmp);
    return files;
}

bool DriftwerkProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim()).substring (0, 60);
    if (clean.isEmpty()) return false;
    const auto folder = userPresetFolder();
    if (! folder.createDirectory()) return false;
    userPresetName = clean;
    juce::MemoryBlock mb;
    getStateInformation (mb);
    auto xml = getXmlFromBinary (mb.getData(), (int) mb.getSize());
    if (xml == nullptr) return false;
    return xml->writeTo (folder.getChildFile (clean + ".timewind"));
}

bool DriftwerkProcessor::loadUserPreset (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return false;
    juce::MemoryBlock mb;
    copyXmlToBinary (*xml, mb);
    loadingUserPreset = true;
    setStateInformation (mb.getData(), (int) mb.getSize());
    loadingUserPreset = false;
    userPresetName = f.getFileNameWithoutExtension();
    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
    return true;
}

void DriftwerkProcessor::loadPreset (int index)
{
    if (index < 0 || index >= getNumPresets()) return;
    ++stateLoads;   // the morph carries on from the new preset (your A and B stay stored)
    currentProgram = index;
    userPresetName.clear();
    const auto& pr = presets()[(size_t) index];

    // Work out every parameter's final value first (preset value, else default), then send
    // each one to the host once, as a complete gesture, and only if it actually changes.
    std::map<juce::String, float> target;
    for (auto& [id, value] : pr.values)
        if (auto* rp = apvts.getParameter (id))
            target[id] = rp->convertTo0to1 (value);
    for (const auto& [name, db] : presetTrims)
        if (juce::String (name) == pr.name)
            if (auto* vol = apvts.getParameter ("out.vol"))
            {
                const auto it = target.find ("out.vol");
                const float base = it != target.end() ? vol->convertFrom0to1 (it->second) : vol->convertFrom0to1 (vol->getDefaultValue());
                target["out.vol"] = vol->convertTo0to1 (juce::jlimit (-40.0f, 6.0f, base + db));
            }
    // Your own sound stays loaded in GRAIN when you change presets.
    if (auto* src = apvts.getParameter ("grain.src"))
        if ((int) std::lround (apvts.getRawParameterValue ("grain.src")->load()) == myFileSource && userSample != nullptr)
            target["grain.src"] = src->getValue();
    // ...and your own break...
    if (auto* bs = apvts.getParameter ("brk.src"))
        if ((int) std::lround (apvts.getRawParameterValue ("brk.src")->load()) == 1 && hasMyBreak())
            target["brk.src"] = bs->getValue();
    // ...and so does your own sound in DREAM.
    if (auto* ds = apvts.getParameter ("dream.sound"))
        if ((int) std::lround (apvts.getRawParameterValue ("dream.sound")->load()) == dreamMySound && hasDreamSound())
            target["dream.sound"] = ds->getValue();

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (rp->getParameterID() == "morph.time") continue;   // how long your morphs take stays as you set it
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
    ++stateLoads;
}

void DriftwerkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("patch", getPatchString(), nullptr);
    state.setProperty ("mods", getModString(), nullptr);
    state.setProperty ("sample", samplePath, nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", getPresetName (currentProgram), nullptr);
    state.setProperty ("userPreset", userPresetName, nullptr);
    state.setProperty ("seq", getSequenceString(), nullptr);
    state.setProperty ("hits", getHitString(), nullptr);
    state.setProperty ("pulse", getPulseString(), nullptr);
    {
        const juce::ScopedLock sl (wordBanksLock);
        if (wordsPath.isNotEmpty()) state.setProperty ("wordsPath", wordsPath, nullptr);
    }
    if (dreamPath.isNotEmpty()) state.setProperty ("dreamPath", dreamPath, nullptr);
    if (breakPath.isNotEmpty()) state.setProperty ("breakPath", breakPath, nullptr);
    {
        juce::StringArray ps;
        for (auto& sp : pulseSamplePath) ps.add (sp);
        if (ps.joinIntoString ("").isNotEmpty()) state.setProperty ("pulseSamples", ps.joinIntoString ("\n"), nullptr);
    }
    for (int w = 0; w < 2; ++w)
    {
        const auto ms = getMorphString (w);
        if (ms.isNotEmpty()) state.setProperty (w == 0 ? "morphA" : "morphB", ms, nullptr);
        else state.removeProperty (w == 0 ? "morphA" : "morphB", nullptr);
    }
    state.setProperty ("srcVersion", 2, nullptr);
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
            // Sets saved before GRAIN had more built-in sounds: MY FILE used to come right after the first 8.
            if (! state.hasProperty ("srcVersion"))
            {
                auto src = state.getChildWithProperty ("id", "grain.src");
                if (src.isValid() && (int) src.getProperty ("value") == 8) src.setProperty ("value", (double) myFileSource, nullptr);
            }
            ++stateLoads;
            apvts.replaceState (state);
            // MORPH snapshots: a set always brings its own back; a preset of yours only when its
            // morph is moving (so you can still load one, STORE A, load another and STORE B).
            if (! loadingUserPreset || (int) std::lround (apvts.getRawParameterValue ("morph.move")->load()) != 0)
                for (int w = 0; w < 2; ++w)
                {
                    const char* key = w == 0 ? "morphA" : "morphB";
                    if (state.hasProperty (key)) setMorphFromString (w, state.getProperty (key).toString());
                }
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
            const juce::String dp = state.getProperty ("dreamPath", {}).toString();
            if (dp.isNotEmpty() && dp != dreamPath && juce::File (dp).existsAsFile()) loadDreamSound (juce::File (dp));
            const juce::String bp = state.getProperty ("breakPath", {}).toString();
            if (bp.isNotEmpty() && bp != breakPath && juce::File (bp).existsAsFile()) loadBreak (juce::File (bp), false);
            {
                const auto ps = juce::StringArray::fromLines (state.getProperty ("pulseSamples", {}).toString());
                for (int l = 0; l < 5; ++l)
                {
                    const juce::String path = l < ps.size() ? ps[l] : juce::String();
                    if (path == pulseSamplePath[l]) continue;
                    if (path.isNotEmpty() && juce::File (path).existsAsFile()) loadPulseSample (l, juce::File (path));
                    else clearPulseSample (l);
                }
            }
            if (juce::MessageManager::existsAndIsCurrentThread()) syncBreak();
            currentProgram = state.getProperty ("program", 0);
            const juce::String pn = state.getProperty ("presetName", {}).toString();
            for (int i = 0; i < getNumPresets(); ++i)
                if (pn.isNotEmpty() && getPresetName (i) == pn) currentProgram = i;
            currentProgram = juce::jlimit (0, getNumPresets() - 1, currentProgram);
            userPresetName = state.getProperty ("userPreset", {}).toString();
            const juce::String seq = state.getProperty ("seq", {}).toString();
            if (seq.isNotEmpty()) setSequenceString (seq);
            const juce::String hits = state.getProperty ("hits", {}).toString();
            if (hits.isNotEmpty()) setHitString (hits);
            const juce::String pulse = state.getProperty ("pulse", {}).toString();
            if (pulse.isNotEmpty())
            {
                pulseStyleKnown.store ((int) std::lround (apvts.getRawParameterValue ("pulse.style")->load()));
                setPulseString (pulse);
            }
            const juce::String wp = state.getProperty ("wordsPath", {}).toString();
            juce::String have;
            { const juce::ScopedLock sl (wordBanksLock); have = wordsPath; }
            if (wp.isNotEmpty() && wp != have)
            {
                juce::StringArray paths;
                for (const auto& line : juce::StringArray::fromLines (wp))
                    if (juce::File::isAbsolutePath (line) && juce::File (line).exists()) paths.add (line);
                loadWords (paths);
            }
            ++stateLoads;
        }
    }
}

//==============================================================================
// MORPH: two snapshots of every knob and a glide between them.
bool DriftwerkProcessor::morphSkips (const juce::String& id)
{
    return id.startsWith ("morph.") || id.startsWith ("cam.") || id == "clk.run" || id == "clk.bpm"
        || id == "out.vol" || id == "play.on";
}

juce::String DriftwerkProcessor::morphTimeText (float seconds)
{
    const int t = juce::roundToInt (seconds);
    if (t < 60) return juce::String (t) + " s";
    return juce::String (t / 60) + ":" + juce::String (t % 60).paddedLeft ('0', 2) + " min";
}

bool DriftwerkProcessor::hasMorph (int which) const
{
    const juce::ScopedLock sl (morphLock);
    return which >= 0 && which < 2 && morphSnap[which].size() == paramList.size();
}

void DriftwerkProcessor::storeMorph (int which)
{
    if (which < 0 || which > 1) return;
    {
        const juce::ScopedLock sl (morphLock);
        auto& snap = morphSnap[which];
        snap.resize (paramList.size());
        for (size_t k = 0; k < paramList.size(); ++k) snap[k] = paramList[k]->getValue();
    }
    // You are at the one you just stored: the fader goes there, and no knob moves.
    if (auto* mp = apvts.getParameter ("morph.pos"))
    {
        mp->beginChangeGesture();
        mp->setValueNotifyingHost (which == 0 ? 0.0f : 1.0f);
        mp->endChangeGesture();
    }
    ++stateLoads;
}

juce::String DriftwerkProcessor::getMorphString (int which) const
{
    const juce::ScopedLock sl (morphLock);
    if (morphSnap[which].size() != paramList.size()) return {};
    juce::StringArray items;
    for (size_t k = 0; k < paramList.size(); ++k)
        if (! morphSkip[k]) items.add (paramList[k]->getParameterID() + "=" + juce::String (morphSnap[which][k], 5));
    return items.joinIntoString (";");
}

void DriftwerkProcessor::setMorphFromString (int which, const juce::String& s)
{
    const juce::ScopedLock sl (morphLock);
    auto& snap = morphSnap[which];
    if (s.isEmpty()) { snap.clear(); return; }
    snap.resize (paramList.size());
    for (size_t k = 0; k < paramList.size(); ++k) snap[k] = paramList[k]->getValue();   // knobs it doesn't know stay where they are
    juce::StringArray items;
    items.addTokens (s, ";", "");
    for (const auto& it : items)
    {
        const int k = paramIndex (it.upToFirstOccurrenceOf ("=", false, false));
        if (k >= 0) snap[(size_t) k] = juce::jlimit (0.0f, 1.0f, it.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
    }
}

void DriftwerkProcessor::morphTick (double seconds)
{
    const int kp = paramIndex ("morph.pos");
    if (kp < 0) return;
    auto* posParam = paramList[(size_t) kp];
    // Work on a copy, so no lock is held while the host hears about the knobs.
    bool ready;
    {
        const juce::ScopedLock sl (morphLock);
        ready = morphSnap[0].size() == paramList.size() && morphSnap[1].size() == paramList.size();
        if (ready) { morphWork[0] = morphSnap[0]; morphWork[1] = morphSnap[1]; }
    }
    float pos = posParam->getValue();
    // With an LFO or drift on the A-B fader, the morph follows the moving value.
    auto applied = [&] (float p)
    {
        return mods[(size_t) kp]->source.load() >= 0 ? juce::jlimit (0.0f, 1.0f, mods[(size_t) kp]->now.load()) : p;
    };
    const int move = (int) std::lround (apvts.getRawParameterValue ("morph.move")->load());
    const int loads = stateLoads.load();
    if (loads != morphLoadsSeen || ! ready)
    {
        // A preset or a set just loaded, or you stored A or B: carry on from here, change nothing.
        morphLoadsSeen = loads;
        morphLastPos = applied (pos);
        morphWritten = pos;
        morphWanderT = 1.0f;
        morphLastMove = move;
        return;
    }

    // Move the A-B fader.
    const float len = juce::jmax (1.0f, apvts.getRawParameterValue ("morph.time")->load());
    const bool youMovedIt = std::abs (pos - morphWritten) > 0.002f;   // by hand, or host automation
    if (move != morphLastMove) { morphWanderT = 1.0f; morphLastMove = move; }
    const float step = (float) seconds / len;
    float np = pos;
    switch (move)
    {
        case 1: np = juce::jmin (1.0f, pos + step); break;
        case 2: np = juce::jmax (0.0f, pos - step); break;
        case 3:
            if (pos >= 1.0f) morphDir = -1;
            else if (pos <= 0.0f) morphDir = 1;
            np = juce::jlimit (0.0f, 1.0f, pos + (float) morphDir * step);
            break;
        case 4:
        {
            // WANDER: from one place between A and B to another, each glide as long as TIME
            // makes it for that distance (give or take), easing in and out.
            if (youMovedIt || morphWanderT >= 1.0f)
            {
                morphWanderFrom = pos;
                float to = morphRng.next() * 0.5f + 0.5f;
                for (int tries = 0; tries < 6 && std::abs (to - pos) < 0.25f; ++tries) to = morphRng.next() * 0.5f + 0.5f;
                morphWanderTo = to;
                morphWanderT = 0.0f;
                morphWanderLen = juce::jmax (2.0f, len * std::abs (to - pos) * (0.7f + 0.6f * (morphRng.next() * 0.5f + 0.5f)));
            }
            morphWanderT = juce::jmin (1.0f, morphWanderT + (float) seconds / morphWanderLen);
            const float e = morphWanderT * morphWanderT * (3.0f - 2.0f * morphWanderT);
            np = morphWanderFrom + (morphWanderTo - morphWanderFrom) * e;
            break;
        }
        default: break;
    }
    if (move != 0 && std::abs (np - pos) > 1.0e-7f)
    {
        posParam->setValueNotifyingHost (np);
        pos = posParam->getValue();
    }
    morphWritten = pos;

    // Turn the knobs to match. Floats and whole numbers glide; menus, switches and the key
    // flip over at their own point between A and B.
    const float a = applied (pos);
    if (morphLastPos >= 0.0f && std::abs (a - morphLastPos) < 1.0e-6f) return;
    morphLastPos = a;
    const bool atEnd = a <= 0.0f || a >= 1.0f;
    for (size_t k = 0; k < paramList.size(); ++k)
    {
        if (morphSkip[k]) continue;
        const float va = morphWork[0][k], vb = morphWork[1][k];
        if (va == vb) continue;
        auto* rp = paramList[k];
        float v;
        if (morphStepped[k]) v = a >= morphFlipAt[k] ? vb : va;
        else v = rp->convertTo0to1 (rp->getNormalisableRange().snapToLegalValue (rp->convertFrom0to1 (va + (vb - va) * a)));
        const float tol = morphStepped[k] || atEnd ? 1.0e-6f : 0.0004f;
        if (std::abs (v - rp->getValue()) <= tol) continue;
        rp->setValueNotifyingHost (v);
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
        int m = chordIvMask.load();
        if (m == 0) m = 1;
        int n = 0;
        for (int k = 0; k < 12; ++k) if ((m >> k) & 1) out[n++] = k;
        return n;
    }
    if (follow == 2) { out[0] = 0; out[1] = 7; return 2; }
    const auto& sc = scales[(size_t) juce::jlimit (0, (int) scales.size() - 1, iv ("arp.scale"))];
    for (size_t k = 0; k < sc.size(); ++k) out[k] = sc[k];
    return (int) sc.size();
}

void DriftwerkProcessor::setParam (const juce::String& id, float realValue)
{
    if (auto* rp = apvts.getParameter (id))
    {
        const float v = rp->convertTo0to1 (realValue);
        if (std::abs (rp->getValue() - v) < 1.0e-6f) return;
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (v);
        rp->endChangeGesture();
    }
}

DriftwerkProcessor::ProgView DriftwerkProcessor::getProgressionView() const
{
    using namespace harm;
    auto iv = [this] (const char* id) { return (int) std::lround (apvts.getRawParameterValue (id)->load()); };
    ProgView v;
    v.mode = juce::jlimit (0, numProgModes - 1, iv ("prog.mode"));
    v.scale = juce::jlimit (0, 7, iv ("arp.scale"));
    v.keyPc = keyPcNow.load();
    v.spell = Spelling (v.keyPc, v.scale);
    v.keyName = keyName (v.spell);
    v.phase = progPhase.load();
    const int colour = iv ("prog.colour"), drone = iv ("drn.chord"), dc = iv ("dream.chord");
    const double even = 4.0 * progBars[juce::jlimit (0, 4, iv ("prog.bars"))];
    if (v.mode > 0 && v.mode <= progTableLast)
    {
        v.count = 4;
        for (int k = 0; k < 4; ++k) { v.chords[k] = tableChord (v.mode, v.scale, k, colour, drone); v.beats[k] = even; }
    }
    else if (v.mode == progWander)
    {
        v.count = juce::jlimit (0, maxSlots, wanderCount.load());
        for (int k = 0; k < v.count; ++k) { v.chords[k] = codeChord (v.scale, wanderHist[(size_t) k].load(), colour, drone); v.beats[k] = even; }
        v.next = codeChord (v.scale, wanderNextCode.load(), colour, drone);
    }
    else if (v.mode == progMine)
    {
        v.count = juce::jlimit (2, maxSlots, iv ("prog.len") + 2);
        for (int k = 0; k < v.count; ++k)
        {
            v.chords[k] = slotChord (v.scale, iv (slotDegIds[k]), iv (slotTypeIds[k]), colour, drone);
            const int b = juce::jlimit (0, 6, iv (slotLenIds[k]));
            v.beats[k] = b == 0 ? even : slotLenBeats[b];
        }
    }
    if (v.count > 0)
    {
        v.current = v.mode == progWander ? v.count - 1 : juce::jlimit (0, v.count - 1, progIndex.load());
        v.cur = v.chords[v.current];
        if (v.mode != progWander) v.next = v.chords[(v.current + 1) % v.count];
    }
    else
        v.cur = v.next = staticChord (v.scale, colour, dc, drone);
    for (int k = 0; k < v.count; ++k)
    {
        v.names[k] = chordName (v.chords[k], v.spell);
        v.numerals[k] = numeral (v.chords[k]);
    }
    v.curName = chordName (v.cur, v.spell);   v.curNumeral = numeral (v.cur);
    v.nextName = chordName (v.next, v.spell); v.nextNumeral = numeral (v.next);
    return v;
}

namespace
{
int nearestDegree (int scale, int offset)
{
    const auto& sc = harm::scaleOf (harm::parentScale (scale));
    const int pc = ((offset % 12) + 12) % 12;
    int best = 0, bestD = 99;
    for (size_t k = 0; k < sc.size(); ++k)
    {
        const int d = std::min ((pc - sc[k] + 12) % 12, (sc[k] - pc + 12) % 12);
        if (d < bestD) { bestD = d; best = (int) k; }
    }
    return best;
}

// MY CHORDS length option for a chord this many beats long (0 = EVEN).
int lengthOption (double beats, double even)
{
    if (std::abs (beats - even) < 0.01) return 0;
    int best = 1;
    for (int b = 1; b < 7; ++b)
        if (std::abs (std::log2 (harm::slotLenBeats[b] / beats)) < std::abs (std::log2 (harm::slotLenBeats[best] / beats))) best = b;
    return best;
}
} // namespace

void DriftwerkProcessor::setMyChord (int slot, int degree, int type, int length)
{
    if (slot < 0 || slot >= harm::maxSlots) return;
    if (degree >= 0) setParam (slotDegIds[slot], (float) juce::jlimit (0, 6, degree));
    if (type >= 0) setParam (slotTypeIds[slot], (float) juce::jlimit (0, harm::NumTypes - 1, type));
    if (length >= 0) setParam (slotLenIds[slot], (float) juce::jlimit (0, 6, length));
    const int len = (int) std::lround (apvts.getRawParameterValue ("prog.len")->load()) + 2;
    if (slot >= len) setParam ("prog.len", (float) (slot + 1 - 2));
}

// EDIT THESE: whatever is playing becomes MY CHORDS, so each chord can be changed.
void DriftwerkProcessor::copyProgressionToMine()
{
    using namespace harm;
    const auto v = getProgressionView();
    if (v.mode == progMine) return;
    if (v.count > 0)
    {
        auto iv = [this] (const char* id) { return (int) std::lround (apvts.getRawParameterValue (id)->load()); };
        const bool coloured = iv ("prog.colour") > 0;
        const double even = 4.0 * progBars[juce::jlimit (0, 4, iv ("prog.bars"))];
        Harm list[maxSlots];
        double beats[maxSlots];
        int n = 0;
        for (int k = 0; k < v.count; ++k) { list[n] = v.chords[k]; beats[n++] = v.beats[k]; }
        if (n < 2) { list[n] = v.next; beats[n++] = even; }
        const int dreamType[] { TSeventh, TNinth, TSus2, TSeventh, TNinth, TQuartal };
        for (int k = 0; k < n; ++k)
        {
            const auto& h = list[k];
            const int deg = h.degree >= 0 ? h.degree : nearestDegree (v.scale, h.offset);
            const int type = coloured ? 0 : (h.dream >= 0 ? dreamType[h.dream] : juce::jmax (1, h.type));
            setMyChord (k, deg, type, lengthOption (beats[k], even));
        }
        setParam ("prog.len", (float) (n - 2));
    }
    setParam ("prog.mode", (float) progMine);
}

// NEW CHORDS: a fresh progression of LENGTH chords. Chords follow each other the way WANDER
// moves; RHYTHM decides how long each one lasts.
void DriftwerkProcessor::randomiseMyChords()
{
    using namespace harm;
    juce::Random r;
    auto rnd = [&r] { return r.nextFloat(); };
    auto iv = [this] (const char* id) { return (int) std::lround (apvts.getRawParameterValue (id)->load()); };
    const int scale = iv ("arp.scale"), rhythm = iv ("prog.rhythm");
    const bool coloured = iv ("prog.colour") > 0;
    const int n = juce::jlimit (2, maxSlots, iv ("prog.len") + 2);
    const double even = 4.0 * progBars[juce::jlimit (0, 4, iv ("prog.bars"))];

    int codes[maxSlots] { 0 * 16 + TNinth };
    for (int k = 1; k < n; ++k)
    {
        int c = wanderPick (scale, codes[k - 1], rnd);
        for (int tries = 0; k == n - 1 && tries < 8 && c / 16 == 0; ++tries) c = wanderPick (scale, codes[k - 1], rnd);
        codes[k] = c;
    }
    int lens[maxSlots] {};
    for (int k = 0; k < n; ++k)
    {
        double m = 1.0;
        const int pick = r.nextInt (4);
        if (rhythm == 1) m = r.nextFloat() < 0.25f ? 2.0 : 1.0;
        else if (rhythm == 2) { const double ms[] { 0.5, 1.0, 1.0, 2.0 }; m = ms[pick]; }
        else if (rhythm == 3) { const double ms[] { 0.25, 0.5, 0.5, 1.0 }; m = ms[pick]; }
        lens[k] = lengthOption (juce::jlimit (2.0, 64.0, even * m), even);
    }
    // Keep the loop a whole number of bars: half-bar chords come in pairs.
    int halves = 0, lastHalf = -1;
    for (int k = 0; k < n; ++k) if (lens[k] == 1) { ++halves; lastHalf = k; }
    if ((halves & 1) != 0 && lastHalf >= 0) lens[lastHalf] = 2;

    for (int k = 0; k < n; ++k)
    {
        int type = 0;
        if (! coloured) type = (k > 0 && r.nextFloat() < 0.15f) ? (int) TTriad : codes[k] % 16;
        setMyChord (k, codes[k] / 16, type, lens[k]);
    }
    setParam ("prog.mode", (float) progMine);
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
    return harm::noteName (note, harm::Spelling ((int) std::lround (apvts.getRawParameterValue ("arp.root")->load()),
                                                 (int) std::lround (apvts.getRawParameterValue ("arp.scale")->load())));
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
