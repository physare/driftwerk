#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class DriftwerkEditor;

//==============================================================================
class RackLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
};

//==============================================================================
// Rotary knob that opens the modulation menu on right-click.
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
    JackComponent (DriftwerkEditor& e, bool output, int index) : editor (e), isOutput (output), idx (index) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    DriftwerkEditor& editor;
    const bool isOutput;
    const int idx;
    float led = 0.0f;
    bool hover = false;
};

//==============================================================================
class CableLayer : public juce::Component
{
public:
    explicit CableLayer (DriftwerkEditor& e) : editor (e) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    DriftwerkEditor& editor;
};

//==============================================================================
class ScopeComponent : public juce::Component
{
public:
    explicit ScopeComponent (DriftwerkProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    DriftwerkProcessor& proc;
};

class CameraView : public juce::Component
{
public:
    explicit CameraView (DriftwerkProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    DriftwerkProcessor& proc;
};

//==============================================================================
class DriftwerkEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DriftwerkEditor (DriftwerkProcessor&);
    ~DriftwerkEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Patching
    void beginDrag (JackComponent& from);
    void dragTo (juce::Point<float> editorPos);
    void endDrag (juce::Point<float> editorPos);
    void clearJack (JackComponent& j);
    juce::Point<float> jackCentre (bool output, int idx) const;
    juce::Colour cableColour (int out, int in) const;

    DriftwerkProcessor& proc;
    int dragOut = -1, dragIn = -1;
    juce::Point<float> dragPos;
    bool cablesDimmed = false;

private:
    void timerCallback() override;
    void buildModule (int specIndex);
    void refreshPatch();

    struct ControlDef { const char* param; const char* label; int kind; int rx = -1, ry = -1; }; // 0 knob, 1 combo, 2 toggle; rx/ry = fixed cell offset
    struct ModuleSpec
    {
        int module; const char* title; const char* subtitle;
        int row, x, w, cols; bool dark; juce::uint32 accent;
        std::vector<ControlDef> controls;
    };
    std::vector<ModuleSpec> specs;
    std::vector<juce::Rectangle<int>> moduleRects;
    struct TextLabel { juce::Rectangle<int> r; juce::String text; bool dark; };
    std::vector<TextLabel> labels;
    std::vector<juce::Rectangle<int>> outputPlates;
    std::vector<bool> outputPlatesDark;

    RackLookAndFeel lnf;
    std::vector<std::unique_ptr<ModSlider>> sliders;
    void showModMenu (ModSlider& s);
public:
    void refreshNow() { timerCallback(); } // used by the offline render tool
private:
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAtt;

    std::vector<std::unique_ptr<JackComponent>> outJacks, inJacks;
    std::unique_ptr<CableLayer> cables;
    std::unique_ptr<ScopeComponent> scope;
    std::unique_ptr<CameraView> cameraView;
    juce::Rectangle<int> arpLedArea;
    std::array<juce::Rectangle<int>, 4> moduleLedAreas; // LFO1, LFO2, ENV1, ENV2 activity LEDs
    juce::Rectangle<int> scopeRect, cameraRect;

    juce::ComboBox presetBox;
    juce::TextButton randomButton { "RANDOM PATCH" }, clearButton { "PULL ALL" }, dimButton { "DIM CABLES" };

    int lastArpStep = -1;
    juce::String lastPatch;

    static constexpr int headerH = 46, railH = 12, rowH = 360;
    static constexpr int editorW = 1360;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftwerkEditor)
};
