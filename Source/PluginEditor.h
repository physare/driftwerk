#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "SlimeSim.h"

class MainView;

//==============================================================================
// Look: black field, hairline rules, acid-yellow (#f0f870) light, thin Helvetica-style type.
namespace dwui
{
const juce::Colour bg (0xff000000), panel (0xff070707), fg (0xffffffff),
                   fg2 (0x9effffff), fg3 (0x61ffffff), rule (0x29ffffff),
                   acid (0xfff0f870), acidDim (0x8cf0f870);
juce::Font font (float size, bool bold = false);
juce::Font mono (float size);
}

class SlimeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SlimeLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getPopupMenuFont() override;
};

//==============================================================================
class ModSlider : public juce::Slider
{
public:
    ModSlider() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox) {}
    std::function<void()> onRightClick;
    int paramIndex = -1;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) juce::Slider::mouseUp (e); }
};

//==============================================================================
class JackComponent : public juce::Component
{
public:
    JackComponent (MainView& v, bool output, int index) : view (v), isOutput (output), idx (index) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    MainView& view;
    const bool isOutput;
    const int idx;
    float led = 0.0f;
    bool hover = false;
};

class CableLayer : public juce::Component
{
public:
    explicit CableLayer (MainView& v) : view (v) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    MainView& view;
};

class ScopeComponent : public juce::Component
{
public:
    explicit ScopeComponent (DriftwerkProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    DriftwerkProcessor& proc;
};

class CameraView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit CameraView (DriftwerkProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    std::function<void()> onClick;
    DriftwerkProcessor& proc;
};

class SlimeView : public juce::Component
{
public:
    // The slime lives in the processor (it plays even with this window closed); this shows it.
    explicit SlimeView (DriftwerkProcessor& p) : proc (p) { proc.organism.viewers.fetch_add (1); proc.organism.ensureRunning(); }
    ~SlimeView() override { proc.organism.viewers.fetch_sub (1); }
    void paint (juce::Graphics&) override;
    const juce::Image& latest (float level);           // the slime now, as a small image
    DriftwerkProcessor& proc;
    juce::Image frame, simImage;
};

class GrainView : public juce::Component
{
public:
    explicit GrainView (DriftwerkProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void rebuildPeaks();
    DriftwerkProcessor& proc;
    const dw::SampleData* shownSample = nullptr;
    std::vector<float> peaks;
    bool dragHover = false;
    std::function<void()> onClickLoad;
};

//==============================================================================
// BASS: the line it played lately, as a little piano roll, and NEW for a fresh one.
class BassView : public juce::Component
{
public:
    explicit BassView (DriftwerkProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override { newButton.setBounds (getWidth() - 46, 4, 42, 18); }
    void refresh();                                    // called by the UI timer
    DriftwerkProcessor& proc;
    juce::TextButton newButton { "NEW" };
    int seenHead = -1, seenNotes = -1; bool seenOn = false;
};

// LAYERS: a level, a mute and a meter for every sound, in one place.
class LayersPanel : public juce::Component
{
public:
    explicit LayersPanel (DriftwerkProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                                    // called by the UI timer
    static constexpr int rowH = 22, nameW = 64, meterX = 70;
    DriftwerkProcessor& proc;
    juce::Slider levels[DriftwerkProcessor::numLayers];
    juce::TextButton mutes[DriftwerkProcessor::numLayers];
    float glow[DriftwerkProcessor::numLayers] {};
    bool shownMuted[DriftwerkProcessor::numLayers] {};
    // after the controls they watch, so they go first
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;
};

//==============================================================================
// Full-window camera stage: big mirrored live image, slime mold over it, motion
// highlights so you can see every finger move, and the hand-play controls.
class FullCameraView : public juce::Component
{
public:
    FullCameraView (DriftwerkProcessor& p, SlimeView& s);
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
        return false;
    }
    std::function<void()> onClose;
    DriftwerkProcessor& proc;
    SlimeView& slime;
    juce::Rectangle<float> stage;

    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAtt;
    juce::TextButton closeButton { "BACK TO RACK" }, sceneButton { "NEW SCENE" };
    struct Lab { juce::Rectangle<int> r; juce::String t; };
    std::vector<Lab> labs;
    float trailX[24] {}, trailY[24] {}; int trailPos = 0;
    void pushTrail();
};

//==============================================================================
// The performance panel over the slime mold: step sequence, arp sound, drone, harmony.
// The ORGANISM tab shows the slime on its own.
class PerformPanel : public juce::Component
{
public:
    PerformPanel (DriftwerkProcessor& p, MainView& v);
    // The attachments must go before the buttons they watch (some buttons are declared after them).
    ~PerformPanel() override { bAtt.clear(); cAtt.clear(); sAtt.clear(); }
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override { return tab != Organism || y < 34 || x >= organismStripX; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();                                    // called by the UI timer

    enum Tab { Sequence, ArpSound, Drone, Chords, Harmony, Loops, Pulse, Texture, Memory, Organism, Words, NumTabs };
    static constexpr int organismStripX = 404;
    void setTab (int t);
    void setPulsePage (int pg);
    bool fileDropped (const juce::String& path, juce::Point<int> where);   // true if this page took it

private:
    DriftwerkProcessor& proc;
    MainView& view;
    int tab = Sequence;

    struct Lab { juce::Rectangle<int> r; juce::String t; int tab; };
    std::vector<Lab> labs;
    std::vector<std::pair<juce::Component*, int>> tabbed;   // control, tab it belongs to
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> cAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;
    juce::TextButton tabButtons[NumTabs];
    juce::TextButton chordsOnButton { "CHORDS ON" }, hitsNewButton { "NEW" };
    juce::TextButton pulsePatternButton { "PATTERN" }, pulseSoundButton { "SOUND" }, pulseBreakButton { "BREAK" };
    int pulsePage = 0;                                  // PULSE: 0 the pattern, 1 the sound of each drum, 2 the break
    static constexpr int PulsePatternPage = 100, PulseSoundPage = 101, PulseBreakPage = 102;   // pseudo tabs for the PULSE pages
    // PULSE SOUND: your own sound for each drum. BREAK: a sliced drum loop.
    juce::TextButton laneLoadButtons[dw::Pulse::Lanes];
    juce::TextButton breakOnButton { "BREAK ON" }, breakLoadButton { "LOAD BREAK" }, breakFitButton { "FOLLOW TEMPO" },
                     breakDrumsButton { "DRUMS TOO" };
    std::unique_ptr<juce::FileChooser> sampleChooser;
    juce::Rectangle<int> breakBox() const { return { 132, 122, 556, 54 }; }
    void paintBreak (juce::Graphics&);
    void showLaneMenu (int lane);
    void chooseLaneSample (int lane);
    void chooseBreak();
    bool shows (int ct) const { return ct == tab || (tab == Pulse && ct == PulsePatternPage + pulsePage); }
    juce::TextButton loopsOnButton { "LOOPS ON" }, shuffleButton { "SHUFFLE" }, pulseOnButton { "PULSE ON" }, pulseNewButton { "NEW" },
                     texOnButton { "TEXTURE ON" }, memOnButton { "MEMORY ON" }, forgetButton { "FORGET" }, slimeOnButton { "SLIME PLAYS" },
                     wordsOnButton { "WORDS ON" }, wordsLoadButton { "LOAD WORDS" }, wordsBeatButton { "ON THE BEAT" };
    std::unique_ptr<juce::FileChooser> wordsChooser;
    juce::Rectangle<int> wordsBox() const { return { 140, 42, 548, 106 }; }
    void paintWords (juce::Graphics&);
    int wordsShown = -1; float wordsGlow = 0.0f; const dw::WordBank* wordsShownBank = nullptr;
    void onOff (juce::TextButton& b, const char* param, int tabId, juce::Rectangle<int> r);
    juce::TextButton editChordsButton { "EDIT THESE" }, newChordsButton { "NEW CHORDS" };
    juce::TextButton holdButton { "HOLD" }, recButton { "REC NOTES" }, newButton { "NEW" }, clearButton { "CLEAR" },
                     leftButton { "<" }, rightButton { ">" }, saveButton { "SAVE" };
    juce::TextButton slotButtons[DriftwerkProcessor::numSlots];
    int lastSlot = -1;

    void knob (const char* param, const char* label, int tabId, int x, int y);
    void combo (const char* param, const char* label, int tabId, int x, int y, int w);
    juce::Rectangle<int> grid() const { return { 14, 74, getWidth() - 28, 186 }; }
    int colAt (int x) const;
    int degAt (int y) const;
    int dragCol = -1, dragStartY = 0; bool dragMoved = false;

    // CHORDS tab: the HITS pattern
    juce::Rectangle<int> hitGrid() const { return { 130, 138, 558, 50 }; }
    int hitAt (int x) const;
    int hitPaintCol = -1; bool hitPaintOn = true;
    void paintChords (juce::Graphics&);

    // HARMONY tab: the chord wheel and the chords of the loop
    juce::Rectangle<int> chordBox (int k) const { return { 498 + (k % 4) * 48, 58 + (k / 4) * 46, 44, 40 }; }
    void paintHarmony (juce::Graphics&);
    void showChordMenu (int k);
    void animateWheel();
    float pcGlow[12] {}, nextGlow[12] {};
    struct Flash { int pc = 0; float age = 1.0f; };
    Flash flashes[10];
    int flashPos = 0, lastArpCount = -1, lastChordSig = -1;
    float ripple = 1.0f;

    // LOOPS, PULSE, TEXTURE, MEMORY, ORGANISM
    void paintLoops (juce::Graphics&);
    void paintPulse (juce::Graphics&);
    void paintWave (juce::Graphics&);
    juce::Rectangle<int> waveBox() const { return { 140, 46, 196, 108 }; }
    void paintTexture (juce::Graphics&);
    void paintMemory (juce::Graphics&);
    void paintOrganism (juce::Graphics&);
    juce::Rectangle<int> pulseGrid() const { return { 182, 68, 506, 92 }; }
    static int soundCol (int lane) { return 200 + lane * 74; }   // PULSE SOUND page: the centre of each drum's column
    static constexpr int soundRowY[3] { 82, 134, 186 };
    juce::Rectangle<int> memStrip() const { return { 14, 44, 672, 86 }; }
    int pulsePaintLane = -1, pulsePaintStep = -1; bool pulsePaintOn = true;
    float laneFlash[dw::Pulse::Lanes] {}; int laneSeen[dw::Pulse::Lanes] {};
    float earFlash[Organism::maxEars] {}; int earSeen[Organism::maxEars] {};
    static constexpr int texHistory = 140;
    float texHist[2][texHistory] {}; int texHead = 0;
    struct Mote { float x = 0, y = 0, life = 0; int slot = 0; };
    Mote motes[48]; int motePos = 0, texEventsSeen = 0;
    juce::Random uiRng;
    void animate();
};

//==============================================================================
// The whole instrument, laid out at a fixed design size; the editor scales it.
class MainView : public juce::Component, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    explicit MainView (DriftwerkProcessor&);
    ~MainView() override;

    static constexpr int designW = 1770, designH = 1104;   // 16:10, so full screen fills a 1920x1200 screen

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragEnter (const juce::StringArray&, int, int) override { grainView->dragHover = true; grainView->repaint(); }
    void fileDragExit (const juce::StringArray&) override { grainView->dragHover = false; grainView->repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

    // Patching
    void beginDrag (JackComponent& from);
    void dragTo (juce::Point<float> pos);
    void endDrag (juce::Point<float> pos);
    void clearJack (JackComponent& j);
    juce::Point<float> jackCentre (bool output, int idx) const;
    juce::Colour cableColour (int out, int in) const;

    void refreshNow() { timerCallback(); } // used by the offline render tool
    void showFullCamera() { openFullCamera (true); } // used by the offline render tool
    void showPerformTab (int t);
    void setFullScreenLabel (bool isFull);
    std::function<void()> onFullScreen;                    // set by the editor
    void advanceSlime (int steps) { for (int i = 0; i < steps / 2; ++i) proc.organism.tick(); }   // offline renders

    DriftwerkProcessor& proc;
    int dragOut = -1, dragIn = -1;
    juce::Point<float> dragPos;
    bool cablesDimmed = false;

private:
    void timerCallback() override;
    void buildModule (int specIndex);
    void refreshPatch();
    void showModMenu (ModSlider& s);
    void chooseSample();

    struct ControlDef { const char* param; const char* label; int kind; int rx = -1, ry = -1, rw = 54; }; // 0 knob, 1 combo, 2 toggle
    struct ModuleSpec
    {
        int module; const char* title; const char* subtitle;
        int row, x, w, cols;
        std::vector<ControlDef> controls;
        bool compactJacks = false;
    };
    // rows 0, 1, 2; 3 is a tall module standing in rows 0 and 1 (the right-hand column)
    static int rowTop (int row) { return row == 0 || row == 3 ? 60 : (row == 1 ? 372 : 740); }
    static int rowHeight (int row) { return row == 0 ? 304 : (row == 3 ? 672 : 360); }

    std::vector<ModuleSpec> specs;
    std::vector<juce::Rectangle<int>> moduleRects;
    struct TextLabel { juce::Rectangle<int> r; juce::String text; bool jack; };
    std::vector<TextLabel> labels;
    std::vector<juce::Rectangle<int>> outputPlates;

    SlimeLookAndFeel lnf;
    std::vector<std::unique_ptr<ModSlider>> sliders;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAtt;

    std::vector<std::unique_ptr<JackComponent>> outJacks, inJacks;
    std::unique_ptr<CableLayer> cables;
    std::unique_ptr<ScopeComponent> scope;
    std::unique_ptr<CameraView> cameraView;
    std::unique_ptr<SlimeView> slime;
    std::unique_ptr<GrainView> grainView;
    std::unique_ptr<FullCameraView> fullCam;
    std::unique_ptr<PerformPanel> perform;
    std::unique_ptr<BassView> bassView;
    std::unique_ptr<LayersPanel> layers;
    friend class PerformPanel;
    juce::TextButton recordButton { "REC" }, filesButton { "RECORDINGS" }, screenButton { "FULL SCREEN" };
    juce::TextButton fullButton { "CAMERA" };
    void openFullCamera (bool open);
    juce::Rectangle<int> arpLedArea, slimeRect, playRect;
    std::array<juce::Rectangle<int>, 4> moduleLedAreas;

    juce::ComboBox presetBox;
    juce::TextButton presetSaveButton { "SAVE" };
    juce::Array<juce::File> userPresetFiles;
    void rebuildPresetMenu();
    void showPresetSaveMenu();
    void askPresetName (const juce::String& suggestion);
    juce::TextButton sceneButton { "NEW SCENE" }, randomButton { "RANDOM PATCH" }, clearButton { "PULL ALL" },
                     dimButton { "DIM CABLES" }, loadButton { "LOAD SOUND" }, keyButton { "IN KEY" };
    juce::ComboBox soundBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> soundAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> keyAtt;
    void useMyFile();
    std::unique_ptr<juce::FileChooser> chooser;
    // DREAM: what the loops play, and your own file for MY SOUND.
    juce::ComboBox dreamSoundBox;
    juce::TextButton dreamLoadButton { "LOAD" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dreamSoundAtt;
    juce::Rectangle<int> dreamRect, dreamNameArea;
    juce::String shownDreamName;
    juce::String dreamLabelText() const;
    void chooseDreamSound();
    void useDreamSound();

    juce::String lastPatch;
    int frameCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

//==============================================================================
// FULL SCREEN inside a host: a borderless window of its own over the whole screen. The rack
// moves into it and comes back to the plugin window when you leave (Esc, F11 or EXIT FULL).
class DriftwerkEditor;
class FullScreenWindow : public juce::Component, private juce::Timer
{
public:
    explicit FullScreenWindow (DriftwerkEditor& e);
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void userTriedToCloseWindow() override;
    void keepCoveringScreen();       // starts the guard below
    DriftwerkEditor& editor;

private:
    // With Windows display scaling the window can be grown past the screen after it opens;
    // this checks a few times a second that it still covers exactly its monitor.
    void timerCallback() override;
    int fixes = 0;
};

class DriftwerkEditor : public juce::AudioProcessorEditor
{
public:
    explicit DriftwerkEditor (DriftwerkProcessor&);
    ~DriftwerkEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refreshNow() { view.refreshNow(); }
    bool keyPressed (const juce::KeyPress& k) override;
    void toggleFullScreen();                       // switches on the next message loop turn
    void setFull (bool on);                        // switches now
    bool isFull() const { return full; }
    static void fitInto (juce::Component& c, juce::Rectangle<int> area);
    static inline bool fullScreenOnDesktop = true;   // the offline test tool turns this off (no desktop there)
    MainView view;

private:
    bool full = false;
    std::unique_ptr<FullScreenWindow> fsWindow;    // after view: goes first
};
