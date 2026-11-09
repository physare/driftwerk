#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <unordered_map>
#include "DSP.h"
#include "Patch.h"
#include "CameraTracker.h"
#include "Sounds.h"
#include "Harmony.h"
#include "Weird.h"
#include "Waves.h"
#include "Dream.h"
#include "Words.h"
#include "Organism.h"

class DriftwerkProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater
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

    const juce::String getName() const override { return "Timewind"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 40.0; }

    // Presets live inside the plugin's own menu. The host sees a single program, so it can
    // never push an old program number back at us (that made presets snap back in Ableton).
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return getPresetName (currentProgram); }
    void changeProgramName (int, const juce::String&) override {}

    static int getNumPresets();
    static juce::String getPresetName (int index);
    static juce::String getPresetGroup (int index);
    int getCurrentPreset() const { return currentProgram; }
    void loadPreset (int index);

    // MY PRESETS: your own, saved as files in Timewind's folder (AppData/Roaming/Timewind/Presets).
    static juce::File userPresetFolder();
    static juce::Array<juce::File> userPresets();
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& f);
    juce::String userPresetName;   // the one of yours playing now (empty: a built-in one)

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
    std::atomic<float> grainPlayPos { 0.0f };
    std::atomic<float> outputLevel { 0.0f };

    // Sounds for the GRAIN module (message thread). grain.src picks one of the built-in
    // sounds or MY FILE (the last file you loaded).
    static constexpr int myFileSource = dw::numBuiltInSounds;
    static juce::StringArray grainSourceNames();
    bool loadSample (const juce::File& f);
    void syncGrainSource();                    // makes the sound named by grain.src the one GRAIN plays
    const dw::SampleData* getSampleForDisplay() const { return currentSample.load(); }
    juce::String getSampleName() const;
    bool hasUserSample() const { return userSample != nullptr; }

    // DREAM's MY SOUND: a file the loops play, pitched to the chord (message thread).
    static constexpr int dreamMySound = dw::DreamBank::MySound;
    static juce::StringArray dreamSoundNames();
    bool loadDreamSound (const juce::File& f);
    bool hasDreamSound() const { return dreamMine.load() != nullptr; }
    juce::String dreamPath;

    // A fresh random scene: key, scale, chords, rates, grain pitch and modulation patch.
    void rollScene();
    std::atomic<int> sceneCounter { 0 };

    //==========================================================================
    // Step sequence (the arp's 16 steps). In HOLD the pattern repeats exactly and can be
    // edited; otherwise the arp keeps mutating it. Notes are scale/chord degrees, so they
    // always follow the HARMONY setting.
    static constexpr int numSteps = 16, numSlots = 8;
    std::array<std::atomic<int>, numSteps> stepNote;
    std::array<std::atomic<bool>, numSteps> stepOn;
    void setStep (int i, int degree, bool on);          // message thread; switches HOLD on
    juce::String getSequenceString() const;
    void setSequenceString (const juce::String&);
    void newRandomSequence();
    int maxDegree() const;                               // highest degree reachable with the current RANGE
    juce::String degreeName (int degree) const;          // e.g. "D#3"

    // Saved sequences, shared by every Timewind on this computer.
    void storeSlot (int slot);
    bool recallSlot (int slot);
    bool slotUsed (int slot) const { return slot >= 0 && slot < numSlots && slots[slot].isNotEmpty(); }

    // Play notes on a MIDI keyboard into the running sequence.
    std::atomic<bool> recordNotes { false };

    // Chord progression state for the editor (written by the audio thread).
    std::atomic<int> progIndex { -1 }, progOffset { 0 }, chordIvMask { 0 }, keyPcNow { 0 };
    std::atomic<float> progPhase { 0.0f };                  // 0..1 through the chord playing now
    std::atomic<int> arpNotePc { 0 }, arpNoteCount { 0 };   // the arp's last note, for the chord wheel
    std::array<std::atomic<int>, harm::maxSlots> wanderHist;  // WANDER: the chords so far (degree * 16 + type), oldest first
    std::atomic<int> wanderCount { 0 }, wanderNextCode { 0 };
    void setRecordNotes (bool on);

    // Everything the Harmony tab shows about the progression.
    struct ProgView
    {
        int mode = 0, scale = 0, keyPc = 0, count = 0, current = -1;
        harm::Spelling spell;
        juce::String keyName;
        harm::Harm chords[harm::maxSlots], cur, next;
        juce::String names[harm::maxSlots], numerals[harm::maxSlots], curName, curNumeral, nextName, nextNumeral;
        double beats[harm::maxSlots] {};
        float phase = 0.0f;
    };
    ProgView getProgressionView() const;
    void copyProgressionToMine();                         // EDIT THESE: the progression becomes MY CHORDS
    void randomiseMyChords();                             // NEW CHORDS: a fresh MY CHORDS progression
    void setMyChord (int slot, int degree, int type, int length);   // -1 leaves that part alone
    void setParam (const juce::String& id, float realValue);

    // LOOPS layer (for the editor): where each loop is, how loud its note is, and which note.
    std::array<std::atomic<float>, dw::TapeLoops::V> loopPhase, loopEnv;
    std::array<std::atomic<int>, dw::TapeLoops::V> loopNote;
    std::array<std::atomic<float>, dw::TapeLoops::V> loopRatio;
    std::atomic<int> loopShuffleReq { 0 };                 // bump to give the loops new lengths and notes

    // The organism (the slime): it lives on its own thread; its ears play SLIME notes and,
    // in PULSE style ORGANISM, the drums. The editor shows this same slime.
    Organism organism;
    std::array<std::atomic<int>, Organism::maxEars> slimeEarNote;   // the last note each ear played
    std::atomic<int> slimeNotes { 0 };

    // PULSE: soft generative percussion. Each lane/step has a weight; DENSITY lights the steps
    // whose weight is high enough. A click forces a step on (1) or off (-1). NEW reshuffles.
    static constexpr int pulseLanes = dw::Pulse::Lanes, pulseSteps = dw::Pulse::Steps;
    std::array<std::atomic<float>, pulseLanes * pulseSteps> pulseW;
    std::array<std::atomic<int>, pulseLanes * pulseSteps> pulseF;
    std::array<std::atomic<int>, pulseLanes> pulseLaneHits;
    std::atomic<int> pulseStepNow { -1 }, pulseNewReq { 0 }, pulseStyleKnown { -1 };
    std::atomic<float> arpShapeNow { 0.0f };   // the arp WAVE's SHAPE as it moves (for the editor)

    // WORDS: your recordings cut into words and phrases (or the synthetic voice), drifting in.
    void loadWords (const juce::StringArray& paths);       // files and/or folders; reads in the background
    const dw::WordBank* wordsBankNow() const;               // what WORDS is speaking from (never freed while we run)
    const dw::WordBank* myWords() const { return userWords.load(); }
    juce::String wordsErrorText() const { const juce::ScopedLock sl (wordBanksLock); return wordsError; }
    std::atomic<bool> wordsLoading { false };
    std::atomic<int> wordsNowWord { -1 }, wordsPlayed { 0 };
    std::atomic<float> wordsNowLevel { 0.0f };
    std::atomic<const dw::WordBank*> wordsNowBank { nullptr };
    juce::String wordsPath, wordsError;
    std::array<std::atomic<int>, pulseLanes> pulseLanePos;   // the step each lane last played (CHOP moves it about)
    static int pulseLaneLength (int lengthMode, int lane);
    bool pulseOn (int lane, int step, float density) const;
    bool pulseOnNow (int lane, int step) const;
    void setPulse (int lane, int step, bool on);
    juce::String getPulseString() const;
    void setPulseString (const juce::String&);

    // TEXTURE: how busy each texture slot is (for the editor).
    std::array<std::atomic<float>, 2> texEnergy;
    std::atomic<int> texEvents { 0 };

    // MEMORY: what it remembers (peaks, newest at memPeakHead - 1) and where its two heads are.
    std::array<std::atomic<float>, dw::Memory::NumPeaks> memPeaks;
    std::atomic<int> memPeakHead { 0 }, memForgetReq { 0 };
    std::atomic<float> memSeconds { 0.0f };
    std::array<std::atomic<float>, 2> memHeadAgo, memHeadWin, memHeadSpeed;

    // CHORDS layer: the notes it is playing now (for the editor).
    std::array<std::atomic<int>, dw::ChordLayer::V> chordNotes;
    std::atomic<int> chordNoteCount { 0 };

    // CHORDS layer HITS pattern (style STEPS). Each step has a weight; DENSITY lights the steps
    // whose weight is below it. A click forces a step on (1) or off (-1); NEW reshuffles.
    static constexpr int numHitSteps = 16;
    std::array<std::atomic<float>, numHitSteps> hitWeight;
    std::array<std::atomic<int>, numHitSteps> hitForce;
    std::atomic<int> hitStepNow { -1 };
    bool hitOn (int i, float density) const;
    bool hitOnNow (int i) const;                           // from the DENSITY knob (editor)
    void setHit (int i, bool on);
    void newHits();
    juce::String getHitString() const;
    void setHitString (const juce::String&);

    //==========================================================================
    // Record everything Timewind plays to a WAV file (Music/Timewind).
    bool startRecording();
    void stopRecording();
    bool isRecording() const { return activeWriter.load() != nullptr; }
    double recordingSeconds() const { return recordedSamples.load() / juce::jmax (1.0, sr); }
    juce::File lastRecording;

    static constexpr int scopeSize = 1024;
    float scope[scopeSize] {};
    std::atomic<int> scopeWrite { 0 };

private:
    void parameterChanged (const juce::String& id, float) override { if (id == "grain.src") triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { syncGrainSource(); }
    std::unique_ptr<dw::SampleData> builtIns[dw::numBuiltInSounds];
    dw::SampleData* userSample = nullptr;

    // CHORDS layer state
    dw::ChordLayer chordLayer;
    long long chdLastKey = -1;
    int chdLastMode = -1;
    long long chdLastBeat = -1;
    bool chdLastArpGate = false, chdWasOn = false;
    double curBeat = 0.0;
    dw::Random hitRng { 0x417517u };
    void updateChordLayer();

    // LOOPS layer
    dw::TapeLoops tapeLoops;
    float loopPitches[12] {};
    int loopPitchCount = 0, loopShuffleSeen = 0;
    long long loopPitchKey = -1;
    void updateLoopPitches();

    // SLIME, PULSE, TEXTURE, MEMORY
    dw::SlimeVoices slimeVoices;
    dw::Pulse pulseDrums;
    dw::Texture texture;
    dw::Memory memory;
    dw::Random weirdRng { (uint32_t) juce::Random::getSystemRandom().nextInt() | 1u };
    int chordPcs[12] {}, chordPcCount = 0, scalePcs[12] {}, scalePcCount = 0;
    long long weirdPitchKey = -1;
    void updateWeirdPitches();
    void playSlime (const Organism::Event& e);
    void slimeDrum (const Organism::Event& e);
    long long pulseLastK = -1;
    bool pulseFlip[dw::Pulse::Lanes * dw::Pulse::Steps] {};
    void pulseStep (long long k, double stepSamples);
    int chopChunk = -1;
    bool chopReverse = false, chopHold = false, fillNow = false;
    std::atomic<float>* pulseLaneParams[pulseLanes][3] {};
    void generatePulse (int style);
    float pulseFreq (int lane, int variant);
    int pulseNewSeen = 0, memForgetSeen = 0, memPeakSeen = 0;

    // Harmony: the chord playing now, rebuilt only when it changes.
    harm::Harm curHarm;
    long long harmKey = -1;
    float progPhaseNow = 0.0f;
    void applyProgression (double beat);
    // WANDER: a progression that never repeats.
    dw::Random wanderRng { (uint32_t) juce::Random::getSystemRandom().nextInt() | 1u };
    bool wanderOn = false;
    double wanderStart = 0.0, wanderLen = 16.0;
    int wanderCur = 0 * 16 + harm::TNinth, wanderNext = 0;
    void advanceWander (double beat, double evenBeats);
    double wanderDuration (double evenBeats);
    void pushWander (int code);

    juce::StringArray slots;
    juce::File slotsFile() const;
    void loadSlots();
    void saveSlots() const;
    bool holdLatched = false;
    int pendingRecStep = -1, pendingRecNote = 0;
    int pitchSetFromParams (int* out) const;

    juce::TimeSliceThread writerThread { "Timewind recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::CriticalSection writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    std::atomic<double> recordedSamples { 0.0 };

    // Drone voice shaping state
    float droneBreathPh[6] {}, droneSubPh = 0.0f, droneLp = 0.0f, droneAirLp = 0.0f, droneAirHp = 0.0f;
    dw::BlepOsc droneOsc2[6];                     // STRINGS: the second saw of each voice
    float droneVibPh[6] {}, droneHp = 0.0f;
    dw::Svf droneForm[3];                         // CHOIR: vowel formants
    int droneFormCounter = 0;
    float arpGlided = 0.0f;
    int chordOffsetTarget = 0;
    float chordOffsetNow = 0.0f, chordGlideCoef = 1.0f;

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
        // degrees live in stepNote / stepOn (public, shared with the editor)
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
    int arpPitchSet (int* out) const;

    // VCOs
    dw::BlepOsc vco1Saw, vco1Pulse, vco1Sub, vco2Saw, vco2Pulse, vco2Sub;
    dw::WaveOsc wave1, wave2;          // the arp's WAVE (when it is not PATCH)
    float waveDriftPh = 0.0f, arpShapeNowLocal = 0.0f;
    std::unique_ptr<dw::WordBank> synthWords;
    std::atomic<const dw::WordBank*> userWords { nullptr };
    std::vector<std::unique_ptr<dw::WordBank>> wordBanks;   // every bank ever loaded stays alive (voices may hold one)
    juce::CriticalSection wordBanksLock;
    juce::ThreadPool wordsLoader { 1 };
    dw::WordsPlayer wordsPlayer;
    double wordsWait = 4.0;
    void scheduleWords (int numSamples);
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
    float camPinch = 0, camSpread = 0, camFist = 0, camTilt = 0;
    int camTrigSeen = 0, camTrigLeft = 0;
    struct CameraPoller : juce::Timer
    {
        DriftwerkProcessor& owner;
        explicit CameraPoller (DriftwerkProcessor& o) : owner (o) { startTimer (250); }
        void timerCallback() override;
    };
    std::unique_ptr<CameraPoller> cameraPoller;
    // Ambient engine
    dw::DreamBank dream;
    dw::GrainCloud grains;
    dw::TapeFX tape;
    bool grainLastTrig = false;
    std::atomic<dw::SampleData*> currentSample { nullptr };
    std::vector<std::unique_ptr<dw::SampleData>> sampleStore; // keeps old samples alive (message thread)
    juce::String samplePath;
    // DREAM files are never freed while the plugin lives: a loop may still be ringing one out.
    std::vector<std::unique_ptr<dw::SampleData>> dreamStore;
    std::atomic<const dw::SampleData*> dreamMine { nullptr };
    int lastCamTrigForScene = 0;
    double lastSceneTime = 0.0;
    void applyPlayMode();
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
        bool arpOn = true; int arpFollow = 1; bool hold = false; int arpOct = 0; float glide = 0.0f;
        float droneTone = 1.0f, droneWave = 0.0f, droneBreath = 0.0f, droneAir = 0.0f, droneSub = 0.0f;
        int droneType = 0; bool grainKey = true;
        bool chdOn = false; int chdMode = 0, chdVoices = 4, chdOct = 0;
        float chdLevel = 0.45f, chdWave = 0.3f, chdTone = 0.55f, chdAttack = 0.9f, chdRelease = 2.5f, chdWidth = 0.6f;
        int progMode = 0, progBars = 8; float progGlide = 0.2f;
        int progColour = 0, progLen = 4, progRhythm = 0, droneChordUser = 1, dreamChordUser = 1;
        int slotDeg[harm::maxSlots] {}, slotType[harm::maxSlots] {}, slotLen[harm::maxSlots] {};
        int chdSteps = 8, chdRate = 1; float chdDensity = 0.5f, chdGate = 0.7f, chdVary = 0.0f;
        bool loopOn = false; int loopNotes = 0, loopVoices = 5, loopOct = 0;
        float loopLevel = 0.5f, loopLength = 14.0f, loopColour = 0.3f, loopTone = 0.55f, loopSwell = 0.4f, loopDecay = 6.0f,
              loopDrift = 0.3f, loopWeird = 0.25f;
        bool slimePlay = false; int slimeVoice = 3, slimeRange = 2, slimeOct = 1;
        float slimeLevel = 0.5f, slimeTone = 0.6f, slimeWeird = 0.3f;
        bool pulseOn = false; int pulseStyle = 0, pulseSpeed = 1;
        float pulseLevel = 0.5f, pulseDensity = 0.5f, pulseSwing = 0.2f, pulseHuman = 0.3f, pulseTone = 0.45f, pulseSpace = 0.55f,
              pulseVary = 0.2f, pulseWeird = 0.15f;
        int pulseKit = 0, pulseLength = 0;
        float pulsePunch = 0.35f, pulseFill = 0.2f, pulseStutter = 0.0f, pulseChop = 0.0f;
        bool texOn = false; int texA = 0, texB = 0;
        float texLevel = 0.4f, texBlend = 0.4f, texDensity = 0.5f, texTone = 0.5f, texMotion = 0.5f, texKey = 0.5f, texSpace = 0.4f;
        bool memOn = false; int memSpeed = 1;
        float memLevel = 0.5f, memAgo = 30.0f, memLength = 5.0f, memBlur = 0.35f, memScatter = 0.4f, memDissolve = 0.35f;
        int vco1Oct = 0, vco2Oct = 0; float vco1Fine = 0, vco2Fine = 0, vco1Pw = 0.5f, vco2Pw = 0.5f;
        float vco1Pwm = 0.5f, vco2Pwm = 0.5f, vco1Fm = 0.0f, vco2Fm = 0.0f;
        int arpWave = 0, arpTable = 0; float arpShape = 0.0f, arpFold = 0.0f, arpMove = 0.0f;
        bool wordsOn = false, wordsBeat = true; int wordsSrc = 0;
        float wordsLevel = 0.5f, wordsOften = 0.4f, wordsPhrases = 0.5f, wordsSpeed = 0.9f, wordsWobble = 0.4f, wordsRadio = 0.3f,
              wordsReverse = 0.1f, wordsRepeat = 0.1f, wordsSpace = 0.5f;
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
        int dreamChord = 0; float dreamRate = 1, dreamTone = 0.3f, dreamWarp = 0.4f, dreamLevel = 0.5f;
        int dreamSound = 0;
        float gPos = 0.3f, gSpray = 0.3f, gSize = 0.2f, gDensity = 12, gPitch = 0, gRev = 0.2f, gScan = 0.1f, gLevel = 0.4f;
        float tWow = 0.3f, tAge = 0.3f, tHiss = 0.2f, tCrush = 0.0f, shimmer = 0.3f;
    } p;

    double sr = 44100.0;
    int midiTranspose = 0;
    float dcL = 0, dcR = 0, dcInL = 0, dcInR = 0;
    int currentProgram = 0;
    int monitorCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftwerkProcessor)
};
