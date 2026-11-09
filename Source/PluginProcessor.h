#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <unordered_map>
#include "DSP.h"
#include "Patch.h"
#include "CameraTracker.h"

class DriftwerkProcessor : public juce::AudioProcessor
{
public:
    DriftwerkProcessor();
    ~DriftwerkProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Driftwerk"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 40.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Patch cables: for each input jack, the index of the output feeding it (-1 = none).
    std::array<std::atomic<int>, dw::NumIns> patch;
    void setPatchFromString (const juce::String& s);
    juce::String getPatchString() const;
    void connect (int out, int in) { if (in >= 0 && in < dw::NumIns) patch[(size_t) in].store (out); }
    void disconnect (int in) { if (in >= 0 && in < dw::NumIns) patch[(size_t) in].store (-1); }
    void randomisePatch();

    // Knob modulation: any parameter can follow a jack output (hand, motion, LFO...).
    struct ModSlot { std::atomic<int> source { -1 }; std::atomic<float> depth { 0.5f }; std::atomic<float> now { -1.0f }; };
    std::vector<std::unique_ptr<ModSlot>> mods;          // one per parameter, same order as paramList
    std::vector<juce::RangedAudioParameter*> paramList;
    int paramIndex (const juce::String& id) const;
    juce::String getModString() const;
    void setModFromString (const juce::String& s);

    juce::AudioProcessorValueTreeState apvts;

    // UI feedback (written by audio thread, read by the editor).
    std::array<std::atomic<float>, dw::NumOuts> monitor;
    std::atomic<int> arpStep { 0 };
    std::atomic<int> arpLength { 16 };
    CameraTracker camera;
    static constexpr int scopeSize = 1024;
    float scope[scopeSize] {};
    std::atomic<int> scopeWrite { 0 };

private:
    void renderSample (float& outL, float& outR);
    void updateBlockParams();
    std::unordered_map<std::string, int> paramIndexMap;
    std::vector<float> effective;
    float in (int jack, float normalled = 0.0f) const
    {
        const int src = patchCache[(size_t) jack];
        return src >= 0 ? outs[(size_t) src] : normalled;
    }

    std::array<int, dw::NumIns> patchCache {};
    std::array<float, dw::NumOuts> outs {};

    // --- modules
    // Clock
    double beatPos = 0.0, samplesPerBeat = 22050.0;
    bool hostSynced = false;
    double hostPpqAtBlock = 0.0;
    long long blockSampleIndex = 0;
    long long lastTick = -1;
    int tickCount = 0;
    bool clockHigh = false, clock4High = false;

    // Random arpeggiator (shift-register style)
    struct Arp
    {
        int degree[16] {};
        bool gate[16] {};
        float rnd[16] {};
        int step = 0;
        bool lastClk = false;
        int samplesSinceClock = 0;
        int clockPeriod = 5512;
        int gateLeft = 0;
        float pitchOut = 0.0f, rndOut = 0.0f;
        bool gateOut = false;
    } arp;
    dw::Random arpRng { 0x5EED1234u };
    void arpClock();
    int scaleSemitone (int degree) const;

    // VCOs
    dw::BlepOsc vco1Saw, vco1Pulse, vco1Sub, vco2Saw, vco2Pulse, vco2Sub;
    // Drone bank: 6 detuned saws with independent drift
    dw::BlepOsc droneOsc[6];
    float droneDrift[6] {}, droneDriftTarget[6] {};
    int droneDriftCounter = 0;
    dw::Random droneRng { 0xD20E5EEDu };
    // Noise / S&H
    dw::Random noiseRng { 0xC0FFEEu };
    float snhHeld = 0.0f, snhOut = 0.0f;
    bool snhLastClk = false;
    // LFOs
    dw::LFO lfo1, lfo2;
    // Envelopes
    dw::Envelope env1, env2;
    bool env1LastGate = false, env2LastGate = false;
    // Filters
    dw::Ladder vcf1, vcf2;
    // Camera
    float camX = 0, camY = 0, camMot = 0;
    int camTrigSeen = 0, camTrigLeft = 0;
    struct CameraPoller : juce::Timer
    {
        DriftwerkProcessor& owner;
        explicit CameraPoller (DriftwerkProcessor& o) : owner (o) { startTimer (250); }
        void timerCallback() override;
    };
    std::unique_ptr<CameraPoller> cameraPoller;
    // FX
    dw::Ensemble ensemble;
    dw::TapeEcho echo;
    dw::HallReverb reverb;

    // Cached parameter values (per block)
    struct P
    {
        int clkDiv = 3; float clkBpm = 100; bool clkRun = true;
        int scale = 0, root = 36, range = 2, length = 16;
        float density = 0.85f, mutate = 0.15f, walk = 0.6f, gateLen = 0.5f;
        int vco1Oct = 0, vco2Oct = 0; float vco1Fine = 0, vco2Fine = 0, vco1Pw = 0.5f, vco2Pw = 0.5f;
        float vco1Pwm = 0.5f, vco2Pwm = 0.5f, vco1Fm = 0.0f, vco2Fm = 0.0f;
        int droneChord = 1, droneOct = 0; float droneDetune = 0.4f, droneDriftAmt = 0.4f, droneLevel = 0.6f;
        float snhSlew = 0.3f;
        float lfo1Rate = 0.03f, lfo2Rate = 0.07f; int lfo1Shape = 0, lfo2Shape = 1;
        float e1a = 0.005f, e1d = 0.3f, e1s = 0, e1r = 0.3f, e2a = 2, e2d = 4, e2s = 0.7f, e2r = 6;
        float mix[4] { 0.7f, 0.6f, 0.5f, 0.5f };
        float f1Cut = 500, f1Res = 0.5f, f1Drive = 0.3f, f1Cv1 = 0.6f, f1Cv2 = 0.4f;
        float f2Cut = 900, f2Res = 0.3f, f2Drive = 0.2f, f2Cv1 = 0.5f, f2Cv2 = 0.1f;
        float a1Gain = 0, a1Cv = 1, a2Gain = 1, a2Cv = 1;
        float lvl1 = 0.7f, lvl2 = 0.6f, pan1 = 0, pan2 = 0, vol = 0.5f, ens = 0.4f;
        float echoTime = 0.3f, echoFb = 0.5f, echoMix = 0.3f, echoWow = 0.3f;
        float revSize = 0.75f, revDamp = 0.5f, revMix = 0.4f;
        float camSlew = 0.3f, tune = 0.0f;
    } p;

    double sr = 44100.0;
    int midiTranspose = 0;
    float dcL = 0, dcR = 0, dcInL = 0, dcInR = 0;
    int currentProgram = 0;
    int monitorCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftwerkProcessor)
};
