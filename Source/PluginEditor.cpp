#include "PluginEditor.h"

using namespace dw;

namespace
{
juce::Font panelFont (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), size,
                                          bold ? juce::Font::bold : juce::Font::plain)
                           .withHorizontalScale (0.92f));
}

const juce::Colour aluminium (0xffc9cac4);
const juce::Colour aluminiumDark (0xffaeafa8);
const juce::Colour panelBlack (0xff1c1c1e);
const juce::Colour inkDark (0xff141414);
const juce::Colour inkLight (0xffe9e4d4);
const juce::Colour railColour (0xff3a3b3d);

const juce::uint32 cablePalette[] {
    0xffd8312b, 0xfff2c12e, 0xff2f6fd6, 0xff2fa84f, 0xffef7d22,
    0xff8e44ad, 0xffe8e8e8, 0xffe75a9c, 0xff19a7a7, 0xff222222,
};

void drawScrew (juce::Graphics& g, juce::Point<float> c)
{
    g.setColour (juce::Colour (0xff8d8e88));
    g.fillEllipse (c.x - 4.5f, c.y - 4.5f, 9.0f, 9.0f);
    g.setColour (juce::Colour (0xffdedfd8));
    g.fillEllipse (c.x - 3.5f, c.y - 3.8f, 7.0f, 7.0f);
    g.setColour (juce::Colour (0xff55564f));
    g.drawLine (c.x - 3.0f, c.y + 1.0f, c.x + 3.0f, c.y - 1.0f, 1.3f);
}

void drawLed (juce::Graphics& g, juce::Point<float> c, float level, juce::Colour col, float radius = 3.5f)
{
    level = juce::jlimit (0.0f, 1.0f, level);
    if (level > 0.02f)
    {
        g.setColour (col.withAlpha (0.28f * level));
        g.fillEllipse (c.x - radius * 2.6f, c.y - radius * 2.6f, radius * 5.2f, radius * 5.2f);
    }
    g.setColour (juce::Colour (0xff101010));
    g.fillEllipse (c.x - radius - 1.0f, c.y - radius - 1.0f, radius * 2.0f + 2.0f, radius * 2.0f + 2.0f);
    g.setColour (col.darker (2.2f).interpolatedWith (col.brighter (0.4f), level));
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.25f + 0.4f * level));
    g.fillEllipse (c.x - radius * 0.5f, c.y - radius * 0.7f, radius * 0.8f, radius * 0.6f);
}
} // namespace

//==============================================================================
RackLookAndFeel::RackLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffffb43a));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1a1a));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffffb43a));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffffb43a));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextButton::textColourOffId, inkLight);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffb43a));
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff111111));
    setColour (juce::BubbleComponent::outlineColourId, juce::Colour (0xffffb43a));
    setColour (juce::TooltipWindow::textColourId, juce::Colour (0xffffb43a));
}

void RackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const bool dark = s.getProperties()["dark"];
    const auto cap = juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["accent"]);

    // Scale ticks printed on the panel
    g.setColour (dark ? inkLight.withAlpha (0.7f) : inkDark.withAlpha (0.75f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f;
        const float r1 = radius * 0.93f, r2 = radius * (i % 5 == 0 ? 1.06f : 1.0f);
        g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a),
                    c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), i % 5 == 0 ? 1.4f : 0.9f);
    }

    const float kr = radius * 0.80f;
    // Shadow
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - kr + 1.5f, c.y - kr + 2.5f, kr * 2.0f, kr * 2.0f);
    // Knurled skirt
    juce::ColourGradient skirt (juce::Colour (0xff3c3c3c), c.x, c.y - kr, juce::Colour (0xff090909), c.x, c.y + kr, false);
    g.setGradientFill (skirt);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colour (0xff4a4a4a));
    for (int i = 0; i < 24; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 24.0f;
        g.drawLine (c.x + kr * 0.78f * std::sin (a), c.y - kr * 0.78f * std::cos (a),
                    c.x + kr * 0.98f * std::sin (a), c.y - kr * 0.98f * std::cos (a), 0.8f);
    }
    // Coloured cap
    const float cr = kr * 0.62f;
    juce::ColourGradient capG (cap.brighter (0.45f), c.x - cr * 0.4f, c.y - cr, cap.darker (0.6f), c.x + cr * 0.4f, c.y + cr, false);
    g.setGradientFill (capG);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.fillEllipse (c.x - cr * 0.6f, c.y - cr * 0.85f, cr * 1.0f, cr * 0.6f);

    // Pointer
    const float a = start + pos * (end - start);
    g.setColour (juce::Colour (0xfff4f1e6));
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.4f, -kr * 0.98f, 2.8f, kr * 0.62f, 1.0f);
    g.fillPath (ptr, juce::AffineTransform::rotation (a).translated (c.x, c.y));
}

void RackLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff0b0b0b));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colour (0xff3d2a0c));
    g.drawRoundedRectangle (r.reduced (1.5f), 2.0f, 1.0f);
    g.setColour (juce::Colour (0xff6b6b6b));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path tri;
    const float tx = (float) w - 9.0f, ty = (float) h * 0.5f;
    tri.addTriangle (tx - 3.5f, ty - 2.0f, tx + 3.5f, ty - 2.0f, tx, ty + 2.5f);
    g.setColour (juce::Colour (0xffffb43a).withAlpha (0.8f));
    g.fillPath (tri);
}

juce::Font RackLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold));
}

void RackLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (2, 1, box.getWidth() - 14, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

void RackLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    // Bat-handle toggle switch
    const auto r = b.getLocalBounds().toFloat();
    const auto c = r.getCentre();
    g.setColour (juce::Colour (0xff6d6e68));
    g.fillEllipse (c.x - 9, c.y - 9, 18, 18);
    g.setColour (juce::Colour (0xffd9dad3));
    g.fillEllipse (c.x - 7, c.y - 7, 14, 14);
    const bool on = b.getToggleState();
    const float dir = on ? -1.0f : 1.0f;
    g.setColour (juce::Colour (0xff9c9d97));
    g.drawLine (c.x, c.y, c.x, c.y + dir * 14.0f, 5.0f);
    g.setColour (juce::Colour (0xfff2f2ee));
    g.fillEllipse (c.x - 4, c.y + dir * 14.0f - 4, 8, 8);
    drawLed (g, { r.getRight() - 6.0f, r.getY() + 6.0f }, on ? 1.0f : 0.0f, juce::Colour (0xffff3b2f), 3.0f);
}

void RackLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? juce::Colour (0xff111111) : juce::Colour (over ? 0xff2e2e30 : 0xff232325));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (b.getToggleState() ? juce::Colour (0xffffb43a) : juce::Colour (0xff5a5a5a));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

juce::Font RackLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return panelFont (11.5f); }

void RackLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xf0101010));
    g.fillRoundedRectangle (body, 3.0f);
    g.setColour (juce::Colour (0xffffb43a));
    g.drawRoundedRectangle (body, 3.0f, 1.0f);
}

juce::Font RackLookAndFeel::getSliderPopupFont (juce::Slider&)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
}

//==============================================================================
void JackComponent::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto c = juce::Point<float> (r.getCentreX(), r.getBottom() - 11.0f);

    if (isOutput)
        drawLed (g, { c.x, r.getY() + 4.0f }, led, juce::Colour (0xffff3b2f), 2.6f);

    // Hex nut
    juce::Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + juce::MathConstants<float>::pi / 6.0f;
        const juce::Point<float> pt (c.x + 9.5f * std::cos (a), c.y + 9.5f * std::sin (a));
        if (i == 0) hex.startNewSubPath (pt); else hex.lineTo (pt);
    }
    hex.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (hex, juce::AffineTransform::translation (1.0f, 1.5f));
    juce::ColourGradient nut (juce::Colour (0xffeeeee8), c.x, c.y - 9, juce::Colour (0xff8a8b85), c.x, c.y + 9, false);
    g.setGradientFill (nut);
    g.fillPath (hex);
    g.setColour (juce::Colour (0xff5d5e58));
    g.strokePath (hex, juce::PathStrokeType (0.8f));
    // Socket
    g.setColour (hover ? juce::Colour (0xffffb43a) : juce::Colour (0xff2b2b2b));
    g.fillEllipse (c.x - 5.5f, c.y - 5.5f, 11.0f, 11.0f);
    g.setColour (juce::Colours::black);
    g.fillEllipse (c.x - 3.6f, c.y - 3.6f, 7.2f, 7.2f);
}

void JackComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { editor.clearJack (*this); return; }
    editor.beginDrag (*this);
    editor.dragTo (e.getEventRelativeTo (&editor).position);
}

void JackComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        editor.dragTo (e.getEventRelativeTo (&editor).position);
}

void JackComponent::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        editor.endDrag (e.getEventRelativeTo (&editor).position);
}

//==============================================================================
static void drawCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour col, float alpha, float floorY)
{
    const float dist = a.getDistanceFrom (b);
    // Cubic control points at +sag reach ~0.75*sag below the lower plug; keep the loop on screen.
    const float room = juce::jmax (0.0f, (floorY - juce::jmax (a.y, b.y)) / 0.75f);
    const float sag = juce::jmin (18.0f + dist * 0.22f, room);
    juce::Path p;
    p.startNewSubPath (a);
    p.cubicTo (a.x + (b.x - a.x) * 0.2f, a.y + sag, b.x - (b.x - a.x) * 0.2f, b.y + sag, b.x, b.y);

    g.setColour (juce::Colours::black.withAlpha (0.25f * alpha));
    g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (2.0f, 4.0f));
    g.setColour (col.darker (0.7f).withAlpha (alpha));
    g.strokePath (p, juce::PathStrokeType (6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col.withAlpha (alpha));
    g.strokePath (p, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.3f * alpha));
    g.strokePath (p, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (-0.8f, -1.2f));

    for (auto pt : { a, b })
    {
        g.setColour (juce::Colour (0xff151515).withAlpha (alpha));
        g.fillEllipse (pt.x - 6.5f, pt.y - 6.5f, 13.0f, 13.0f);
        g.setColour (col.withAlpha (alpha));
        g.fillEllipse (pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f * alpha));
        g.fillEllipse (pt.x - 2.5f, pt.y - 3.0f, 3.5f, 2.5f);
    }
}

void CableLayer::paint (juce::Graphics& g)
{
    const float alpha = editor.cablesDimmed ? 0.28f : 0.92f;
    const float floorY = (float) getHeight() - 14.0f;
    for (int i = 0; i < NumIns; ++i)
    {
        const int o = editor.proc.patch[(size_t) i].load();
        if (o < 0) continue;
        drawCable (g, editor.jackCentre (true, o), editor.jackCentre (false, i), editor.cableColour (o, i), alpha, floorY);
    }
    if (editor.dragOut >= 0)
        drawCable (g, editor.jackCentre (true, editor.dragOut), editor.dragPos, juce::Colour (cablePalette[editor.dragOut % 10]), 1.0f, floorY);
    else if (editor.dragIn >= 0)
        drawCable (g, editor.dragPos, editor.jackCentre (false, editor.dragIn), juce::Colour (cablePalette[editor.dragIn % 10]), 1.0f, floorY);
}

//==============================================================================
void ScopeComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // Bezel
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRoundedRectangle (r, 8.0f);
    auto screen = r.reduced (7.0f);
    juce::ColourGradient glass (juce::Colour (0xff0c2a16), screen.getCentreX(), screen.getCentreY(),
                                juce::Colour (0xff030a05), screen.getX(), screen.getY(), true);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (screen, 14.0f);

    // Graticule
    g.setColour (juce::Colour (0xff1f5a33).withAlpha (0.6f));
    for (int i = 1; i < 8; ++i)
    {
        const float x = screen.getX() + screen.getWidth() * (float) i / 8.0f;
        g.drawVerticalLine ((int) x, screen.getY() + 4, screen.getBottom() - 4);
    }
    for (int i = 1; i < 6; ++i)
    {
        const float y = screen.getY() + screen.getHeight() * (float) i / 6.0f;
        g.drawHorizontalLine ((int) y, screen.getX() + 4, screen.getRight() - 4);
    }

    // Trace: find a rising zero crossing for a stable display.
    const int N = DriftwerkProcessor::scopeSize;
    const int w = proc.scopeWrite.load();
    int start = (w + 1) % N;
    for (int k = 0; k < N / 2; ++k)
    {
        const int i0 = (w + N / 4 + k) % N, i1 = (i0 + 1) % N;
        if (proc.scope[i0] <= 0.0f && proc.scope[i1] > 0.0f) { start = i1; break; }
    }
    juce::Path trace;
    const int points = 400;
    for (int k = 0; k < points; ++k)
    {
        const float v = juce::jlimit (-1.2f, 1.2f, proc.scope[(start + k) % N] * 1.3f);
        const float x = screen.getX() + 6 + (screen.getWidth() - 12) * (float) k / (float) (points - 1);
        const float y = screen.getCentreY() - v * screen.getHeight() * 0.4f;
        if (k == 0) trace.startNewSubPath (x, y); else trace.lineTo (x, y);
    }
    g.setColour (juce::Colour (0xff46ff8a).withAlpha (0.18f));
    g.strokePath (trace, juce::PathStrokeType (5.0f));
    g.setColour (juce::Colour (0xff8dffb4));
    g.strokePath (trace, juce::PathStrokeType (1.4f));

    // Scanlines + glare
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (float y = screen.getY(); y < screen.getBottom(); y += 3.0f)
        g.drawHorizontalLine ((int) y, screen.getX(), screen.getRight());
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillEllipse (screen.getX() + 10, screen.getY() + 4, screen.getWidth() * 0.6f, screen.getHeight() * 0.3f);
}

//==============================================================================
void CameraView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRoundedRectangle (r, 6.0f);
    auto screen = r.reduced (5.0f);
    g.setColour (juce::Colour (0xff050806));
    g.fillRoundedRectangle (screen, 8.0f);

    auto img = proc.camera.getPreview();
    if (img.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImage (img, screen.reduced (2.0f));
        // Hand position crosshair
        const float hx = proc.monitor[dw::O_CAM_X].load(), hy = proc.monitor[dw::O_CAM_Y].load();
        const float px = screen.getCentreX() + hx * screen.getWidth() * 0.5f;
        const float py = screen.getCentreY() - hy * screen.getHeight() * 0.5f;
        const float m = proc.monitor[dw::O_CAM_MOTION].load();
        g.setColour (juce::Colour (0xff8dffb4).withAlpha (0.4f + 0.6f * m));
        g.drawEllipse (px - 7, py - 7, 14, 14, 1.5f);
        g.drawLine (px - 11, py, px + 11, py, 1.0f);
        g.drawLine (px, py - 11, px, py + 11, 1.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xffffb43a).withAlpha (0.8f));
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold)));
        g.drawFittedText (proc.camera.status, screen.reduced (6.0f).toNearestInt(), juce::Justification::centred, 3);
    }
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    for (float y = screen.getY(); y < screen.getBottom(); y += 3.0f)
        g.drawHorizontalLine ((int) y, screen.getX(), screen.getRight());
}

//==============================================================================
DriftwerkEditor::DriftwerkEditor (DriftwerkProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    const juce::uint32 orange = 0xffe2742b, blue = 0xff3a6fc4, red = 0xffc8352e, green = 0xff3f9a5a,
                       yellow = 0xffe2b12b, grey = 0xff9a9a9a, purple = 0xff7d52b0;
    // module, title, subtitle, row, x, w, cols, dark, accent, controls
    specs = {
        { M_CLOCK,  "CLOCK", "MASTER TIME", 0, 0, 110, 1, true, red,
          { { "clk.div", "RATE", 1 }, { "clk.bpm", "BPM", 0 }, { "clk.run", "RUN", 2 } } },
        { M_ARP,    "RANDOM ARP", "SHIFT REGISTER SEQ", 0, 110, 250, 3, false, red,
          { { "arp.scale", "SCALE", 1 }, { "arp.root", "ROOT", 0 }, { "arp.range", "RANGE", 0 },
            { "arp.len", "LENGTH", 0 }, { "arp.density", "DENSITY", 0 }, { "arp.mutate", "MUTATE", 0 },
            { "arp.walk", "WALK", 0 }, { "arp.gate", "GATE", 0 } } },
        { M_VCO1,   "VCO 1", "OSCILLATOR", 0, 360, 140, 2, false, orange,
          { { "vco1.oct", "OCTAVE", 0 }, { "vco1.fine", "FINE", 0 }, { "vco1.pw", "WIDTH", 0 },
            { "vco1.pwm", "PWM", 0 }, { "vco1.fm", "FM", 0 } } },
        { M_VCO2,   "VCO 2", "OSCILLATOR", 0, 500, 140, 2, false, orange,
          { { "vco2.oct", "OCTAVE", 0 }, { "vco2.fine", "FINE", 0 }, { "vco2.pw", "WIDTH", 0 },
            { "vco2.pwm", "PWM", 0 }, { "vco2.fm", "FM", 0 } } },
        { M_DRONE,  "DRONE BANK", "6 x SAW CHOIR", 0, 640, 160, 2, true, yellow,
          { { "drn.chord", "CHORD", 1 }, { "drn.oct", "OCTAVE", 0 }, { "drn.detune", "DETUNE", 0 },
            { "drn.drift", "DRIFT", 0 }, { "drn.level", "LEVEL", 0 } } },
        { M_NOISE,  "NOISE", "S&H", 0, 800, 100, 1, false, grey, { { "nz.slew", "SLEW", 0 } } },
        { M_LFO1,   "LFO 1", "SLOW", 0, 900, 100, 1, false, blue, { { "lfo1.rate", "RATE", 0 }, { "lfo1.shape", "SHAPE", 1 } } },
        { M_LFO2,   "LFO 2", "SLOW", 0, 1000, 100, 1, false, blue, { { "lfo2.rate", "RATE", 0 }, { "lfo2.shape", "SHAPE", 1 } } },
        { M_CAMERA, "MONITOR / CAMERA", "SCOPE + HAND TRACKING", 0, 1100, 260, 1, true, yellow,
          { { "cam.on", "CAM ON", 2, 150, 146 }, { "cam.sens", "SENS", 0, 202, 146 }, { "cam.slew", "SMOOTH", 0, 176, 210 } } },
        { M_ENV1,   "ENV 1", "ADSR", 1, 0, 130, 2, false, blue,
          { { "env1.a", "ATTACK", 0 }, { "env1.d", "DECAY", 0 }, { "env1.s", "SUSTAIN", 0 }, { "env1.r", "RELEASE", 0 } } },
        { M_ENV2,   "ENV 2", "ADSR", 1, 130, 130, 2, false, blue,
          { { "env2.a", "ATTACK", 0 }, { "env2.d", "DECAY", 0 }, { "env2.s", "SUSTAIN", 0 }, { "env2.r", "RELEASE", 0 } } },
        { M_MIX,    "MIXER", "4 > 1", 1, 260, 120, 2, true, grey,
          { { "mix.l1", "IN 1", 0 }, { "mix.l2", "IN 2", 0 }, { "mix.l3", "IN 3", 0 }, { "mix.l4", "IN 4", 0 } } },
        { M_VCF1,   "VCF 1", "24dB LADDER", 1, 380, 150, 2, false, green,
          { { "vcf1.cut", "CUTOFF", 0 }, { "vcf1.res", "RESO", 0 }, { "vcf1.drive", "DRIVE", 0 },
            { "vcf1.cv1", "CV 1", 0 }, { "vcf1.cv2", "CV 2", 0 } } },
        { M_VCF2,   "VCF 2", "24dB LADDER", 1, 530, 150, 2, false, green,
          { { "vcf2.cut", "CUTOFF", 0 }, { "vcf2.res", "RESO", 0 }, { "vcf2.drive", "DRIVE", 0 },
            { "vcf2.cv1", "CV 1", 0 }, { "vcf2.cv2", "CV 2", 0 } } },
        { M_VCA1,   "VCA 1", "LINEAR", 1, 680, 100, 1, false, grey, { { "vca1.gain", "BASE", 0 }, { "vca1.cv", "CV AMT", 0 } } },
        { M_VCA2,   "VCA 2", "LINEAR", 1, 780, 100, 1, false, grey, { { "vca2.gain", "BASE", 0 }, { "vca2.cv", "CV AMT", 0 } } },
        { M_MASTER, "OUTPUT", "ENSEMBLE / MASTER", 1, 880, 140, 2, true, red,
          { { "out.lvl1", "LEVEL 1", 0 }, { "out.lvl2", "LEVEL 2", 0 }, { "out.pan1", "PAN 1", 0 },
            { "out.pan2", "PAN 2", 0 }, { "out.ens", "ENSEMBLE", 0 }, { "out.vol", "VOLUME", 0 } } },
        { -1,       "TAPE ECHO", "PING-PONG", 1, 1020, 170, 2, true, purple,
          { { "echo.div", "TIME", 1 }, { "echo.ms", "FREE MS", 0 }, { "echo.fb", "REPEATS", 0 },
            { "echo.mix", "MIX", 0 }, { "echo.wow", "WOW", 0 } } },
        { -2,       "HALL", "SPACE TANK", 1, 1190, 170, 2, true, purple,
          { { "hall.size", "SIZE", 0 }, { "hall.damp", "DAMP", 0 }, { "hall.mix", "MIX", 0 } } },
    };

    for (int i = 0; i < NumOuts; ++i)
    {
        outJacks.push_back (std::make_unique<JackComponent> (*this, true, i));
        addAndMakeVisible (*outJacks.back());
    }
    for (int i = 0; i < NumIns; ++i)
    {
        inJacks.push_back (std::make_unique<JackComponent> (*this, false, i));
        addAndMakeVisible (*inJacks.back());
    }

    for (int s = 0; s < (int) specs.size(); ++s)
        buildModule (s);

    const int row0 = headerH + railH;
    scopeRect = { 1100 + 10, row0 + 40, 240, 100 };
    cameraRect = { 1100 + 10, row0 + 148, 136, 104 };
    scope = std::make_unique<ScopeComponent> (proc);
    scope->setBounds (scopeRect);
    addAndMakeVisible (*scope);
    cameraView = std::make_unique<CameraView> (proc);
    cameraView->setBounds (cameraRect);
    addAndMakeVisible (*cameraView);

    // Header controls
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        proc.setCurrentProgram (presetBox.getSelectedId() - 1);
        refreshPatch();
    };
    addAndMakeVisible (presetBox);

    randomButton.onClick = [this] { proc.randomisePatch(); refreshPatch(); };
    clearButton.onClick = [this] { for (int i = 0; i < NumIns; ++i) proc.disconnect (i); refreshPatch(); };
    dimButton.setClickingTogglesState (true);
    dimButton.onClick = [this] { cablesDimmed = dimButton.getToggleState(); cables->repaint(); };
    addAndMakeVisible (randomButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (dimButton);

    cables = std::make_unique<CableLayer> (*this);
    addAndMakeVisible (*cables);

    setSize (editorW, headerH + railH + rowH + railH * 2 + rowH + railH);
    lastPatch = proc.getPatchString();
    startTimerHz (30);
}

DriftwerkEditor::~DriftwerkEditor()
{
    setLookAndFeel (nullptr);
}

void DriftwerkEditor::buildModule (int specIndex)
{
    const auto& sp = specs[(size_t) specIndex];
    const int top = headerH + railH + sp.row * (rowH + railH * 2);
    const juce::Rectangle<int> rect (sp.x, top, sp.w, rowH);
    moduleRects.push_back (rect);

    int y0 = top + 44;
    if (sp.module == M_ARP)
    {
        arpLedArea = { sp.x + 12, top + 40, sp.w - 24, 16 };
        y0 += 20;
    }

    const int cellW = (sp.w - 12) / sp.cols;
    const int cellH = sp.module == M_ARP ? 60 : 66;
    const auto ink = sp.dark ? inkLight : inkDark;
    juce::ignoreUnused (ink);

    for (int c = 0; c < (int) sp.controls.size(); ++c)
    {
        const auto& cd = sp.controls[(size_t) c];
        const int col = c % sp.cols, row = c / sp.cols;
        const juce::Rectangle<int> cell = cd.rx >= 0 ? juce::Rectangle<int> (sp.x + cd.rx, top + cd.ry, 52, cellH)
                                                     : juce::Rectangle<int> (sp.x + 6 + col * cellW, y0 + row * cellH, cellW, cellH);

        if (cd.kind == 0)
        {
            auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            s->setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
            s->getProperties().set ("dark", sp.dark);
            s->getProperties().set ("accent", (juce::int64) sp.accent);
            s->setPopupDisplayEnabled (true, true, this, 900);
            s->setBounds (cell.withSizeKeepingCentre (48, 48).withY (cell.getY()));
            s->setMouseDragSensitivity (220);
            addAndMakeVisible (*s);
            sliderAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, cd.param, *s));
            sliders.push_back (std::move (s));
            labels.push_back ({ { cell.getX(), cell.getY() + 48, cell.getWidth(), 14 }, cd.label, sp.dark });
        }
        else if (cd.kind == 1)
        {
            auto cb = std::make_unique<juce::ComboBox>();
            if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (cd.param)))
                cb->addItemList (param->choices, 1);
            cb->setBounds (cell.getX() + 3, cell.getY() + 14, cell.getWidth() - 6, 22);
            addAndMakeVisible (*cb);
            comboAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, cd.param, *cb));
            combos.push_back (std::move (cb));
            labels.push_back ({ { cell.getX(), cell.getY() + 42, cell.getWidth(), 14 }, cd.label, sp.dark });
        }
        else
        {
            auto t = std::make_unique<juce::ToggleButton>();
            t->setBounds (cell.withSizeKeepingCentre (44, 44).withY (cell.getY() + 2));
            addAndMakeVisible (*t);
            toggleAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, cd.param, *t));
            toggles.push_back (std::move (t));
            labels.push_back ({ { cell.getX(), cell.getY() + 48, cell.getWidth(), 14 }, cd.label, sp.dark });
        }
    }

    if (sp.module < 0) return;

    // Activity LED areas for modulation modules.
    const int ledIndex = sp.module == M_LFO1 ? 0 : sp.module == M_LFO2 ? 1 : sp.module == M_ENV1 ? 2 : sp.module == M_ENV2 ? 3 : -1;
    if (ledIndex >= 0) moduleLedAreas[(size_t) ledIndex] = { sp.x + sp.w - 22, top + 12, 12, 12 };

    // Jacks: inputs above, outputs on a contrasting plate at the bottom.
    std::vector<int> ins, outsList;
    for (int i = 0; i < NumIns; ++i)  if (inDefs[i].module == sp.module)  ins.push_back (i);
    for (int i = 0; i < NumOuts; ++i) if (outDefs[i].module == sp.module) outsList.push_back (i);

    const int jackW = 44, jackH = 46;
    const int perRow = juce::jmax (1, (sp.w - 8) / jackW);
    auto rowsFor = [perRow] (size_t n) { return n == 0 ? 0 : (int) ((n + (size_t) perRow - 1) / (size_t) perRow); };
    const int outRows = rowsFor (outsList.size()), inRows = rowsFor (ins.size());

    int y = top + rowH - 10 - outRows * jackH;
    if (outRows > 0)
    {
        outputPlates.push_back ({ sp.x + 5, y - 4, sp.w - 10, outRows * jackH + 6 });
        outputPlatesDark.push_back (sp.dark);
    }
    auto placeRow = [&] (const std::vector<int>& list, bool isOut, int yStart)
    {
        for (size_t k = 0; k < list.size(); ++k)
        {
            const int row = (int) k / perRow, col = (int) k % perRow;
            const int inThisRow = juce::jmin (perRow, (int) list.size() - row * perRow);
            const int rowW = inThisRow * jackW;
            const int x = sp.x + (sp.w - rowW) / 2 + col * jackW;
            const int yy = yStart + row * jackH;
            auto& jack = isOut ? *outJacks[(size_t) list[k]] : *inJacks[(size_t) list[k]];
            jack.setBounds (x + (jackW - 30) / 2, yy + 14, 30, 30);
            labels.push_back ({ { x, yy + 2, jackW, 12 }, isOut ? outDefs[list[k]].label : inDefs[list[k]].label,
                                isOut ? ! sp.dark : sp.dark });
        }
    };
    placeRow (outsList, true, y);
    placeRow (ins, false, y - 6 - inRows * jackH);
}

void DriftwerkEditor::paint (juce::Graphics& g)
{
    // Case and header
    g.fillAll (juce::Colour (0xff17130f));
    juce::ColourGradient wood (juce::Colour (0xff3b2516), 0, 0, juce::Colour (0xff24170d), 0, (float) headerH, false);
    g.setGradientFill (wood);
    g.fillRect (0, 0, getWidth(), headerH);
    g.setColour (juce::Colour (0xffe9e4d4));
    g.setFont (panelFont (24.0f).withExtraKerningFactor (0.22f));
    g.drawText ("DRIFTWERK", 16, 4, 260, headerH - 8, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffffb43a));
    g.setFont (panelFont (11.0f));
    g.drawText ("MODULAR SYNTHESIZER SYSTEM  /  MODEL 94", 222, 4, 320, headerH - 8, juce::Justification::centredLeft);
    g.setColour (inkLight.withAlpha (0.55f));
    g.setFont (panelFont (10.0f, false));
    g.drawText ("drag jack to jack to patch  /  right-click a jack to unplug",
                545, 4, 340, headerH - 8, juce::Justification::centredLeft);

    // Rails
    auto drawRail = [&] (int y)
    {
        juce::ColourGradient rg (juce::Colour (0xff5a5b5e), 0, (float) y, juce::Colour (0xff26272a), 0, (float) (y + railH), false);
        g.setGradientFill (rg);
        g.fillRect (0, y, getWidth(), railH);
        g.setColour (juce::Colour (0xff111111));
        for (int x = 6; x < getWidth(); x += 10)
            g.fillRect ((float) x, (float) y + railH * 0.5f - 1.5f, 5.0f, 3.0f);
    };
    const int row0 = headerH + railH;
    const int row1 = row0 + rowH + railH * 2;
    drawRail (headerH);
    drawRail (row0 + rowH);
    drawRail (row1 - railH);
    drawRail (row1 + rowH);

    // Module panels
    for (size_t i = 0; i < specs.size(); ++i)
    {
        const auto& sp = specs[i];
        const auto r = moduleRects[i].toFloat().reduced (1.0f, 0.0f);
        if (sp.dark)
        {
            g.setColour (panelBlack);
            g.fillRect (r);
        }
        else
        {
            juce::ColourGradient al (aluminium.brighter (0.08f), r.getX(), r.getY(), aluminiumDark, r.getRight(), r.getBottom(), false);
            g.setGradientFill (al);
            g.fillRect (r);
            // Brushed texture
            juce::Random rnd ((juce::int64) i * 7919);
            for (int k = 0; k < 140; ++k)
            {
                const float yy = r.getY() + rnd.nextFloat() * r.getHeight();
                g.setColour (juce::Colours::white.withAlpha (0.05f + rnd.nextFloat() * 0.06f));
                g.drawHorizontalLine ((int) yy, r.getX(), r.getRight());
            }
        }
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRect (r, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (sp.dark ? 0.08f : 0.45f));
        g.drawLine (r.getX() + 1, r.getY() + 1, r.getX() + 1, r.getBottom() - 1, 1.0f);

        // Accent stripe + title
        const auto accent = juce::Colour (sp.accent);
        g.setColour (accent);
        g.fillRect (r.getX() + 6, r.getY() + 32, r.getWidth() - 12, 2.0f);
        g.setColour (sp.dark ? inkLight : inkDark);
        g.setFont (panelFont (14.0f).withExtraKerningFactor (0.08f));
        g.drawText (sp.title, (int) r.getX() + 6, (int) r.getY() + 6, (int) r.getWidth() - 12, 16, juce::Justification::centred);
        g.setFont (panelFont (8.5f, false).withExtraKerningFactor (0.1f));
        g.setColour ((sp.dark ? inkLight : inkDark).withAlpha (0.7f));
        g.drawText (sp.subtitle, (int) r.getX() + 6, (int) r.getY() + 20, (int) r.getWidth() - 12, 11, juce::Justification::centred);

        drawScrew (g, { r.getX() + 8.0f, r.getY() + 8.0f });
        drawScrew (g, { r.getRight() - 8.0f, r.getBottom() - 6.0f });
        if (r.getWidth() > 120)
        {
            drawScrew (g, { r.getRight() - 8.0f, r.getY() + 8.0f });
            drawScrew (g, { r.getX() + 8.0f, r.getBottom() - 6.0f });
        }
    }

    // Output plates (inverse colour, the classic "outputs live here" convention)
    for (size_t i = 0; i < outputPlates.size(); ++i)
    {
        g.setColour (outputPlatesDark[i] ? aluminium : juce::Colour (0xff232325));
        g.fillRoundedRectangle (outputPlates[i].toFloat(), 4.0f);
    }

    // Labels
    for (auto& l : labels)
    {
        g.setColour (l.dark ? inkLight : inkDark);
        g.setFont (panelFont (9.5f).withExtraKerningFactor (0.06f));
        g.drawText (l.text, l.r, juce::Justification::centred);
    }

    // Arp step LEDs
    if (! arpLedArea.isEmpty())
    {
        const int step = proc.arpStep.load();
        const int len = proc.arpLength.load();
        const float gate = proc.monitor[O_ARP_GATE].load();
        const float dx = (float) arpLedArea.getWidth() / 16.0f;
        for (int i = 0; i < 16; ++i)
        {
            const juce::Point<float> c (arpLedArea.getX() + dx * (i + 0.5f), (float) arpLedArea.getCentreY());
            const float level = i == step ? (gate > 0.5f ? 1.0f : 0.55f) : (i < len ? 0.08f : 0.0f);
            drawLed (g, c, level, juce::Colour (0xffff3b2f), 3.2f);
        }
    }

    // Module activity LEDs
    const int ledOuts[] { O_LFO1, O_LFO2, O_ENV1, O_ENV2 };
    for (int i = 0; i < 4; ++i)
        if (! moduleLedAreas[(size_t) i].isEmpty())
        {
            const float v = proc.monitor[(size_t) ledOuts[i]].load();
            const float level = i < 2 ? v * 0.5f + 0.5f : v;
            drawLed (g, moduleLedAreas[(size_t) i].getCentre().toFloat(), level,
                     i < 2 ? juce::Colour (0xff36a2ff) : juce::Colour (0xff4cff7a), 3.5f);
        }
}

void DriftwerkEditor::resized()
{
    cables->setBounds (getLocalBounds());
    presetBox.setBounds (getWidth() - 470, 11, 170, 24);
    randomButton.setBounds (getWidth() - 292, 11, 110, 24);
    clearButton.setBounds (getWidth() - 176, 11, 76, 24);
    dimButton.setBounds (getWidth() - 94, 11, 84, 24);
}

//==============================================================================
juce::Point<float> DriftwerkEditor::jackCentre (bool output, int idx) const
{
    const auto& j = output ? *outJacks[(size_t) idx] : *inJacks[(size_t) idx];
    const auto b = j.getBounds().toFloat();
    return { b.getCentreX(), b.getBottom() - 11.0f };
}

juce::Colour DriftwerkEditor::cableColour (int out, int in) const
{
    return juce::Colour (cablePalette[(out * 7 + in * 3) % 10]);
}

void DriftwerkEditor::beginDrag (JackComponent& from)
{
    dragOut = dragIn = -1;
    if (from.isOutput)
        dragOut = from.idx;
    else
    {
        const int src = proc.patch[(size_t) from.idx].load();
        if (src >= 0)
        {
            proc.disconnect (from.idx); // pick the cable up by its plug
            dragOut = src;
        }
        else
            dragIn = from.idx;
    }
}

void DriftwerkEditor::dragTo (juce::Point<float> pos)
{
    dragPos = pos;
    cables->repaint();
}

void DriftwerkEditor::endDrag (juce::Point<float> pos)
{
    JackComponent* target = nullptr;
    for (auto* list : { &outJacks, &inJacks })
        for (auto& j : *list)
            if (j->getBounds().expanded (4).contains (pos.toInt()))
                target = j.get();

    if (target != nullptr)
    {
        if (dragOut >= 0 && ! target->isOutput) proc.connect (dragOut, target->idx);
        else if (dragIn >= 0 && target->isOutput) proc.connect (target->idx, dragIn);
    }
    dragOut = dragIn = -1;
    refreshPatch();
}

void DriftwerkEditor::clearJack (JackComponent& j)
{
    if (j.isOutput)
    {
        for (int i = 0; i < NumIns; ++i)
            if (proc.patch[(size_t) i].load() == j.idx) proc.disconnect (i);
    }
    else
        proc.disconnect (j.idx);
    refreshPatch();
}

void DriftwerkEditor::refreshPatch()
{
    lastPatch = proc.getPatchString();
    cables->repaint();
    proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
}

void DriftwerkEditor::timerCallback()
{
    // Patch may change from presets/state loads made by the host.
    if (dragOut < 0 && dragIn < 0)
    {
        const auto ps = proc.getPatchString();
        if (ps != lastPatch) { lastPatch = ps; cables->repaint(); }
    }
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

    for (auto& j : outJacks)
    {
        const float v = std::abs (proc.monitor[(size_t) j->idx].load());
        const float led = juce::jlimit (0.0f, 1.0f, v * 1.4f);
        if (std::abs (led - j->led) > 0.04f) { j->led = led; j->repaint(); }
    }

    scope->repaint();
    cameraView->repaint();
    repaint (arpLedArea.expanded (8));
    for (auto& r : moduleLedAreas)
        if (! r.isEmpty()) repaint (r.expanded (10));
}
