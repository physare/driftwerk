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
    explicit SlimeView (DriftwerkProcessor& p) : proc (p), sim (300, 128, 22000) {}
    void paint (juce::Graphics&) override;
    void advance();
    DriftwerkProcessor& proc;
    SlimeSim sim;
    juce::Image frame;
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
    bool hitTest (int x, int y) override { return tab != Organism || y < 34; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();                                    // called by the UI timer

    enum Tab { Sequence, ArpSound, Drone, Chords, Harmony, Organism, NumTabs };
    void setTab (int t);

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
    juce::TextButton chordsOnButton { "CHORDS ON" };
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
};

//==============================================================================
// The whole instrument, laid out at a fixed design size; the editor scales it.
class MainView : public juce::Component, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    explicit MainView (DriftwerkProcessor&);
    ~MainView() override;

    static constexpr int designW = 1500, designH = 1104;

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
    void advanceSlime (int steps) { for (int i = 0; i < steps; ++i) slime->advance(); }

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

    struct ControlDef { const char* param; const char* label; int kind; int rx = -1, ry = -1; }; // 0 knob, 1 combo, 2 toggle
    struct ModuleSpec
    {
        int module; const char* title; const char* subtitle;
        int row, x, w, cols;
        std::vector<ControlDef> controls;
        bool compactJacks = false;
    };
    static int rowTop (int row) { return row == 0 ? 60 : (row == 1 ? 372 : 740); }
    static int rowHeight (int row) { return row == 0 ? 304 : 360; }

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
    friend class PerformPanel;
    juce::TextButton recordButton { "REC" }, filesButton { "RECORDINGS" }, screenButton { "FULL SCREEN" };
    juce::TextButton fullButton { "CAMERA" };
    void openFullCamera (bool open);
    juce::Rectangle<int> arpLedArea, slimeRect, playRect;
    std::array<juce::Rectangle<int>, 4> moduleLedAreas;

    juce::ComboBox presetBox;
    juce::TextButton sceneButton { "NEW SCENE" }, randomButton { "RANDOM PATCH" }, clearButton { "PULL ALL" },
                     dimButton { "DIM CABLES" }, loadButton { "LOAD SOUND" }, keyButton { "IN KEY" };
    juce::ComboBox soundBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> soundAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> keyAtt;
    void useMyFile();
    std::unique_ptr<juce::FileChooser> chooser;

    juce::String lastPatch;
    int frameCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

//==============================================================================
class DriftwerkEditor : public juce::AudioProcessorEditor
{
public:
    explicit DriftwerkEditor (DriftwerkProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    void refreshNow() { view.refreshNow(); }
    bool keyPressed (const juce::KeyPress& k) override;
    void toggleFullScreen();
    bool isFull() const { return full; }
    MainView view;

private:
    bool full = false;
    juce::Rectangle<int> normalBounds;
};
