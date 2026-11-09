#include "PluginEditor.h"

namespace dwnative { bool coverMonitor (void* nativeHandle); }   // NativeWindow.cpp

using namespace dw;
using namespace dwui;

juce::Font dwui::font (float size, bool bold)
{
    // The site uses Helvetica Neue Light; fall back to the system's clean sans.
   #if JUCE_WINDOWS
    const juce::String face = bold ? "Segoe UI Semibold" : "Segoe UI Light";
   #elif JUCE_MAC
    const juce::String face = "Helvetica Neue";
   #else
    const juce::String face = juce::Font::getDefaultSansSerifFontName();
   #endif
    return juce::Font (juce::FontOptions (face, size, juce::Font::plain));
}

juce::Font dwui::mono (float size)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
}

namespace
{
const juce::uint32 veinPalette[] { 0xfff0f870, 0xffc6f05c, 0xfffffbd0, 0xff9be870, 0xffe8d25a, 0xffd8ff9a };

void drawGlowDot (juce::Graphics& g, juce::Point<float> c, float level, float radius = 3.0f)
{
    level = juce::jlimit (0.0f, 1.0f, level);
    if (level > 0.02f)
    {
        g.setColour (acid.withAlpha (0.22f * level));
        g.fillEllipse (c.x - radius * 2.6f, c.y - radius * 2.6f, radius * 5.2f, radius * 5.2f);
    }
    g.setColour (rule);
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (acid.withAlpha (level));
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
}
} // namespace

//==============================================================================
SlimeLookAndFeel::SlimeLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, acid);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff050505));
    setColour (juce::PopupMenu::textColourId, fg);
    setColour (juce::PopupMenu::headerTextColourId, fg3);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, acid);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextButton::textColourOffId, fg);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colours::black);
    setColour (juce::BubbleComponent::outlineColourId, acid);
}

void SlimeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float tr = radius * 0.9f;

    juce::Path track;
    track.addCentredArc (c.x, c.y, tr, tr, 0.0f, start, end, true);
    g.setColour (rule);
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float a = start + pos * (end - start);
    juce::Path value;
    value.addCentredArc (c.x, c.y, tr, tr, 0.0f, start, a, true);
    g.setColour (acid.withAlpha (0.18f));
    g.strokePath (value, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (acid);
    g.strokePath (value, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float ir = radius * 0.6f;
    g.setColour (juce::Colour (0xff0c0c0c));
    g.fillEllipse (c.x - ir, c.y - ir, ir * 2.0f, ir * 2.0f);
    g.setColour (s.isMouseOverOrDragging() ? fg3 : rule);
    g.drawEllipse (c.x - ir, c.y - ir, ir * 2.0f, ir * 2.0f, 1.0f);
    g.setColour (fg);
    g.drawLine (c.x + ir * 0.25f * std::sin (a), c.y - ir * 0.25f * std::cos (a),
                c.x + ir * 0.95f * std::sin (a), c.y - ir * 0.95f * std::cos (a), 1.6f);

    if ((int) s.getProperties().getWithDefault ("modSrc", -1) >= 0)
    {
        const float depth = s.getProperties()["modDepth"];
        const float now = s.getProperties()["modNow"];
        const float from = juce::jlimit (0.0f, 1.0f, depth >= 0 ? pos : pos + depth);
        const float to = juce::jlimit (0.0f, 1.0f, depth >= 0 ? pos + depth : pos);
        // Stays inside the knob's bounds (radius + 3), so the ring and its moving dot are never clipped.
        const float rr = radius + 0.4f;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rr, rr, 0.0f, start + from * (end - start), start + to * (end - start), true);
        g.setColour (fg.withAlpha (0.75f));
        g.strokePath (arc, juce::PathStrokeType (1.4f));
        if (now >= 0.0f)
        {
            const float na = start + now * (end - start);
            g.setColour (acid.withAlpha (0.3f));
            g.fillEllipse (c.x + rr * std::sin (na) - 2.6f, c.y - rr * std::cos (na) - 2.6f, 5.2f, 5.2f);
            g.setColour (fg);
            g.fillEllipse (c.x + rr * std::sin (na) - 1.9f, c.y - rr * std::cos (na) - 1.9f, 3.8f, 3.8f);
        }
    }
}

void SlimeLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colours::black);
    g.fillRect (r);
    g.setColour (box.isMouseOver (true) ? acidDim : rule);
    g.drawRect (r, 1.0f);
    juce::Path tri;
    const float tx = (float) w - 9.0f, ty = (float) h * 0.5f;
    tri.addTriangle (tx - 3.0f, ty - 1.5f, tx + 3.0f, ty - 1.5f, tx, ty + 2.0f);
    g.setColour (fg3);
    g.fillPath (tri);
}

juce::Font SlimeLookAndFeel::getComboBoxFont (juce::ComboBox&) { return dwui::font (12.5f); }

void SlimeLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (2, 1, box.getWidth() - 14, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

void SlimeLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                                         juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
        return;
    }
    const bool dim = s.getProperties()["dim"];
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (rule);
    g.fillRect ((float) x, cy - 1.0f, (float) w, 2.0f);
    g.setColour (dim ? fg3 : acidDim);
    g.fillRect ((float) x, cy - 1.0f, pos - (float) x, 2.0f);
    g.setColour (dim ? fg2 : (s.isMouseOverOrDragging() ? fg : acid));
    g.fillRect (pos - 1.5f, cy - 6.0f, 3.0f, 12.0f);
}

void SlimeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const auto pill = juce::Rectangle<float> (0, 0, 36, 18).withCentre (r.getCentre());
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (acid.withAlpha (0.25f));
        g.fillRoundedRectangle (pill.expanded (3), 12.0f);
        g.setColour (acid);
        g.fillRoundedRectangle (pill, 9.0f);
    }
    else
    {
        g.setColour (over ? fg3 : rule);
        g.drawRoundedRectangle (pill, 9.0f, 1.2f);
    }
    const float kx = on ? pill.getRight() - 9.0f : pill.getX() + 9.0f;
    g.setColour (on ? juce::Colours::black : fg2);
    g.fillEllipse (kx - 6.0f, pill.getCentreY() - 6.0f, 12.0f, 12.0f);
}

void SlimeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool lit = over || down || b.getToggleState();
    const bool accent = b.getProperties()["accent"];
    g.setColour (lit ? acid : juce::Colours::black);
    g.fillRect (r);
    g.setColour (lit || accent ? acid : juce::Colour (0x40ffffff));
    g.drawRect (r, 1.0f);
}

void SlimeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down)
{
    const bool lit = over || down || b.getToggleState();
    const bool accent = b.getProperties()["accent"];
    g.setColour (lit ? juce::Colours::black : accent ? acid : fg);
    const bool smallText = b.getProperties()["small"];
    g.setFont (smallText ? dwui::font (10.5f).withExtraKerningFactor (0.03f) : dwui::font (12.0f).withExtraKerningFactor (0.08f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void SlimeLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colours::black);
    g.fillRect (body);
    g.setColour (acid);
    g.drawRect (body, 1.0f);
}

juce::Font SlimeLookAndFeel::getSliderPopupFont (juce::Slider&) { return dwui::mono (12.5f); }
void SlimeLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (juce::Colour (0xff050505));
    g.setColour (rule);
    g.drawRect (0, 0, w, h);
}
juce::Font SlimeLookAndFeel::getPopupMenuFont() { return dwui::font (14.0f); }

//==============================================================================
void JackComponent::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    // The bounds leave room for the hover halo (radius 12) below and the LED glow above,
    // so neither circle is clipped at the component's edge.
    const auto c = juce::Point<float> (r.getCentreX(), r.getBottom() - 14.0f);
    if (isOutput) drawGlowDot (g, { c.x, c.y - 15.0f }, led, 2.2f);

    const auto ring = isOutput ? acid : fg2;
    if (hover)
    {
        g.setColour (acid.withAlpha (0.25f));
        g.fillEllipse (c.x - 12.0f, c.y - 12.0f, 24.0f, 24.0f);
    }
    g.setColour (ring.withAlpha (hover ? 1.0f : 0.8f));
    g.drawEllipse (c.x - 8.5f, c.y - 8.5f, 17.0f, 17.0f, 1.3f);
    g.setColour (juce::Colours::black);
    g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
    g.setColour (rule);
    g.drawEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f, 1.0f);
}

void JackComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { view.clearJack (*this); return; }
    view.beginDrag (*this);
    view.dragTo (e.getEventRelativeTo (&view).position);
}

void JackComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) view.dragTo (e.getEventRelativeTo (&view).position);
}

void JackComponent::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) view.endDrag (e.getEventRelativeTo (&view).position);
}

//==============================================================================
// Cables are drawn as glowing slime-mold veins.
static void drawVein (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour col, float alpha, float floorY)
{
    const float dist = a.getDistanceFrom (b);
    const float room = juce::jmax (0.0f, (floorY - juce::jmax (a.y, b.y)) / 0.75f);
    const float sag = juce::jmin (14.0f + dist * 0.18f, room);
    juce::Path p;
    p.startNewSubPath (a);
    p.cubicTo (a.x + (b.x - a.x) * 0.25f, a.y + sag, b.x - (b.x - a.x) * 0.25f, b.y + sag, b.x, b.y);

    g.setColour (col.withAlpha (0.10f * alpha));
    g.strokePath (p, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col.withAlpha (0.22f * alpha));
    g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col.withAlpha (0.95f * alpha));
    g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.45f * alpha));
    g.strokePath (p, juce::PathStrokeType (0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (auto pt : { a, b })
    {
        g.setColour (col.withAlpha (0.25f * alpha));
        g.fillEllipse (pt.x - 9.0f, pt.y - 9.0f, 18.0f, 18.0f);
        g.setColour (col.withAlpha (alpha));
        g.fillEllipse (pt.x - 5.0f, pt.y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colours::black.withAlpha (alpha));
        g.fillEllipse (pt.x - 2.0f, pt.y - 2.0f, 4.0f, 4.0f);
    }
}

void CableLayer::paint (juce::Graphics& g)
{
    const float alpha = view.cablesDimmed ? 0.25f : 0.9f;
    const float floorY = (float) getHeight() - 8.0f;
    for (int i = 0; i < NumIns; ++i)
    {
        const int o = view.proc.patch[(size_t) i].load();
        if (o < 0) continue;
        drawVein (g, view.jackCentre (true, o), view.jackCentre (false, i), view.cableColour (o, i), alpha, floorY);
    }
    if (view.dragOut >= 0)
        drawVein (g, view.jackCentre (true, view.dragOut), view.dragPos, acid, 1.0f, floorY);
    else if (view.dragIn >= 0)
        drawVein (g, view.dragPos, view.jackCentre (false, view.dragIn), acid, 1.0f, floorY);
}

//==============================================================================
void ScopeComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (rule);
    g.drawRect (r, 1.0f);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 4, r.getRight() - 4);

    const int N = DriftwerkProcessor::scopeSize;
    const int w = proc.scopeWrite.load();
    int start = (w + 1) % N;
    for (int k = 0; k < N / 2; ++k)
    {
        const int i0 = (w + N / 4 + k) % N, i1 = (i0 + 1) % N;
        if (proc.scope[i0] <= 0.0f && proc.scope[i1] > 0.0f) { start = i1; break; }
    }
    juce::Path trace;
    const int points = 300;
    for (int k = 0; k < points; ++k)
    {
        const float v = juce::jlimit (-1.2f, 1.2f, proc.scope[(start + k) % N] * 1.3f);
        const float x = r.getX() + 4 + (r.getWidth() - 8) * (float) k / (float) (points - 1);
        const float y = r.getCentreY() - v * r.getHeight() * 0.42f;
        if (k == 0) trace.startNewSubPath (x, y); else trace.lineTo (x, y);
    }
    g.setColour (acid.withAlpha (0.18f));
    g.strokePath (trace, juce::PathStrokeType (4.0f));
    g.setColour (acid);
    g.strokePath (trace, juce::PathStrokeType (1.2f));
}

//==============================================================================
// The 21-joint hand skeleton, mirrored to match the picture: glowing acid bones, white fingertips.
static void drawSkeleton (juce::Graphics& g, const HandTracker::Pose& pose, juce::Rectangle<float> r, float scale)
{
    if (! pose.present) return;
    static const int bones[][2] { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 0, 5 }, { 5, 6 }, { 6, 7 }, { 7, 8 },
                                  { 5, 9 }, { 9, 10 }, { 10, 11 }, { 11, 12 }, { 9, 13 }, { 13, 14 }, { 14, 15 }, { 15, 16 },
                                  { 13, 17 }, { 0, 17 }, { 17, 18 }, { 18, 19 }, { 19, 20 } };
    auto at = [&] (int j) { const auto p = pose.pts[(size_t) j];
                            return juce::Point<float> (r.getRight() - p.x * r.getWidth(), r.getY() + p.y * r.getHeight()); };
    juce::Path path;
    for (auto& b : bones) { path.startNewSubPath (at (b[0])); path.lineTo (at (b[1])); }
    g.setColour (dwui::acid.withAlpha (0.22f));
    g.strokePath (path, juce::PathStrokeType (6.0f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (dwui::acid);
    g.strokePath (path, juce::PathStrokeType (1.6f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    for (int j = 0; j < HandTracker::NumJoints; ++j)
    {
        const auto c = at (j);
        const bool tip = j == 4 || j == 8 || j == 12 || j == 16 || j == 20;
        const float rad = (tip ? 4.5f : 2.6f) * scale;
        g.setColour (tip ? dwui::fg : dwui::acid);
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
    }
}

void CameraView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black);
    g.fillRect (r);
    g.setColour (rule);
    g.drawRect (r, 1.0f);
    auto screen = r.reduced (3.0f);

    auto img = proc.camera.getPreview();
    if (img.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.setOpacity (0.85f);
        g.drawImage (img, screen);
        g.setOpacity (1.0f);
        drawSkeleton (g, proc.camera.hand.getPose(), screen, 0.6f);
        const float hx = proc.monitor[O_CAM_X].load(), hy = proc.monitor[O_CAM_Y].load();
        const float px = screen.getCentreX() + hx * screen.getWidth() * 0.5f;
        const float py = screen.getCentreY() - hy * screen.getHeight() * 0.5f;
        const float m = proc.monitor[O_CAM_MOTION].load();
        g.setColour (acid.withAlpha (0.5f + 0.5f * m));
        g.drawEllipse (px - 8, py - 8, 16, 16, 1.4f);
        g.drawLine (px - 13, py, px - 5, py, 1.0f); g.drawLine (px + 5, py, px + 13, py, 1.0f);
        g.drawLine (px, py - 13, px, py - 5, 1.0f); g.drawLine (px, py + 5, px, py + 13, 1.0f);
    }
    else
    {
        g.setColour (fg3);
        g.setFont (dwui::font (11.0f));
        g.drawFittedText (proc.camera.status.toLowerCase() + "\nclick here for full screen", screen.reduced (6.0f).toNearestInt(), juce::Justification::centred, 3);
    }
}

//==============================================================================
const juce::Image& SlimeView::latest (float level)
{
    if (! simImage.isValid()) simImage = juce::Image (juce::Image::RGB, Organism::W, Organism::H, true, juce::SoftwareImageType());
    proc.organism.withSim ([&] (SlimeSim& s)
    {
        juce::Graphics g (simImage);
        g.drawImageAt (s.render (level), 0, 0);
    });
    return simImage;
}

void SlimeView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.fillAll (juce::Colours::black);
    const float level = juce::jlimit (0.0f, 1.0f, proc.outputLevel.load() * 1.6f);
    const auto& img = latest (level);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (img, r);
    // Soft bloom: the same frame, blurred by scaling, drawn faintly on top.
    if (! frame.isValid()) frame = juce::Image (juce::Image::RGB, 75, 32, true, juce::SoftwareImageType());
    {
        juce::Graphics fg_ (frame);
        fg_.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        fg_.drawImage (img, frame.getBounds().toFloat());
    }
    g.setOpacity (0.35f + 0.25f * level);
    g.drawImage (frame, r);
    g.setOpacity (1.0f);

    // Readouts, in the site's manner.
    g.setColour (fg2);
    g.setFont (dwui::font (12.0f));
    const bool cam = proc.camera.isOpen();
    g.setColour (fg3);
    g.setFont (dwui::mono (10.5f));
    const juce::String readout = juce::String::formatted ("hand x %+.2f   y %+.2f   motion %.2f   level %.2f",
        proc.monitor[O_CAM_X].load(), proc.monitor[O_CAM_Y].load(), proc.monitor[O_CAM_MOTION].load(), level);
    g.drawText (readout, 14, getHeight() - 24, getWidth() - 28, 14, juce::Justification::left);
    if (! cam)
        g.drawText ("turn on HAND > CAM to play with movement", 14, getHeight() - 24, getWidth() - 28, 14, juce::Justification::right);

    if (cam)
    {
        const float px = r.getCentreX() + proc.monitor[O_CAM_X].load() * r.getWidth() * 0.5f;
        const float py = r.getCentreY() - proc.monitor[O_CAM_Y].load() * r.getHeight() * 0.5f;
        g.setColour (fg.withAlpha (0.7f));
        g.drawEllipse (px - 10, py - 10, 20, 20, 1.0f);
    }
    g.setColour (rule);
    g.drawRect (r, 1.0f);
}

//==============================================================================
void GrainView::rebuildPeaks()
{
    shownSample = proc.getSampleForDisplay();
    peaks.assign ((size_t) juce::jmax (1, getWidth()), 0.0f);
    if (shownSample == nullptr || shownSample->length() == 0) return;
    const int n = shownSample->length();
    for (size_t x = 0; x < peaks.size(); ++x)
    {
        const int a = (int) ((double) x / (double) peaks.size() * n), b = juce::jmin (n, (int) ((double) (x + 1) / (double) peaks.size() * n) + 1);
        float pk = 0.0f;
        for (int i = a; i < b; i += juce::jmax (1, (b - a) / 64))
            pk = juce::jmax (pk, std::abs (shownSample->left[(size_t) i]), std::abs (shownSample->right[(size_t) i]));
        peaks[x] = pk;
    }
}

void GrainView::paint (juce::Graphics& g)
{
    if (proc.getSampleForDisplay() != shownSample || (int) peaks.size() != getWidth()) rebuildPeaks();
    const auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black);
    g.fillRect (r);
    const float mid = r.getCentreY();
    for (size_t x = 0; x < peaks.size(); ++x)
    {
        const float h = peaks[x] * r.getHeight() * 0.46f;
        g.setColour (fg.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, mid - h, mid + h);
    }
    // Where the grains are coming from
    auto* ap = proc.apvts.getRawParameterValue ("grain.pos");
    auto* sp = proc.apvts.getRawParameterValue ("grain.spray");
    const float pos = ap->load(), spray = sp->load();
    const float cx = r.getX() + pos * r.getWidth(), sw = spray * 0.5f * r.getWidth();
    g.setColour (acid.withAlpha (0.12f));
    g.fillRect (juce::Rectangle<float> (cx - sw, r.getY(), sw * 2.0f, r.getHeight()));
    const float play = proc.grainPlayPos.load();
    g.setColour (acid);
    g.drawVerticalLine ((int) (r.getX() + play * r.getWidth()), r.getY(), r.getBottom());

    g.setColour (fg3);
    g.setFont (dwui::font (11.0f));
    g.drawText (proc.getSampleName(), r.reduced (8, 4), juce::Justification::topLeft);
    g.drawText ("drop a sound file here", r.reduced (8, 4), juce::Justification::bottomRight);
    // How IN KEY is treating this sound.
    juce::String keyInfo = "free pitch";
    if (proc.apvts.getRawParameterValue ("grain.key")->load() > 0.5f)
    {
        const auto* sd = proc.getSampleForDisplay();
        keyInfo = sd != nullptr && sd->rootMidi > 0.0f
                    ? "in key: its " + juce::MidiMessage::getMidiNoteName (juce::roundToInt (sd->rootMidi), true, false, 3) + " follows the chords"
                    : "in key: no clear pitch, so PITCH moves in octaves";
    }
    g.drawText (keyInfo, r.reduced (8, 4), juce::Justification::bottomLeft);
    g.setColour (dragHover ? acid : rule);
    g.drawRect (r, dragHover ? 2.0f : 1.0f);
}

void GrainView::mouseDown (const juce::MouseEvent&) { if (onClickLoad) onClickLoad(); }

//==============================================================================
FullCameraView::FullCameraView (DriftwerkProcessor& p, SlimeView& s) : proc (p), slime (s)
{
    setWantsKeyboardFocus (true);
    struct K { const char* id; const char* label; bool toggle; };
    const K ks[] { { "cam.on", "CAMERA", true }, { "play.on", "HAND PLAY", true }, { "play.depth", "DEPTH", false },
                   { "cam.sens", "SENSITIVITY", false }, { "cam.slew", "SMOOTH", false }, { "hall.mix", "SPACE", false },
                   { "tape.wow", "WARP", false }, { "out.vol", "VOLUME", false } };
    for (auto& k : ks)
    {
        if (k.toggle)
        {
            auto t = std::make_unique<juce::ToggleButton>();
            addAndMakeVisible (*t);
            toggleAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, k.id, *t));
            toggles.push_back (std::move (t));
        }
        else
        {
            auto sl = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            sl->setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
            sl->setPopupDisplayEnabled (true, true, this, 900);
            addAndMakeVisible (*sl);
            knobAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, k.id, *sl));
            knobs.push_back (std::move (sl));
        }
        labs.push_back ({ {}, k.label });
    }
    closeButton.onClick = [this] { if (onClose) onClose(); };
    sceneButton.onClick = [this] { proc.rollScene(); };
    addAndMakeVisible (closeButton);
    addAndMakeVisible (sceneButton);
}

void FullCameraView::resized()
{
    const int panelX = getWidth() - 300;
    stage = juce::Rectangle<float> (24.0f, 70.0f, (float) panelX - 48.0f, (float) getHeight() - 150.0f);
    closeButton.setBounds (getWidth() - 200, 18, 180, 30);
    // Two toggles, then knobs in a 2-column grid, then the scene button.
    int y = 90;
    for (size_t i = 0; i < toggles.size(); ++i)
    {
        toggles[i]->setBounds (panelX + 30 + (int) i * 130, y, 60, 40);
        labs[i].r = { panelX + 10 + (int) i * 130, y + 42, 100, 16 };
    }
    y += 100;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int col = (int) i % 2, row = (int) i / 2;
        knobs[i]->setBounds (panelX + 34 + col * 130, y + row * 110, 64, 64);
        labs[toggles.size() + i].r = { panelX + 16 + col * 130, y + row * 110 + 68, 100, 16 };
    }
    y += ((int) knobs.size() + 1) / 2 * 110 + 10;
    sceneButton.setBounds (panelX + 20, y, 250, 40);
}

void FullCameraView::pushTrail()
{
    trailX[trailPos] = proc.monitor[O_CAM_X].load();
    trailY[trailPos] = proc.monitor[O_CAM_Y].load();
    trailPos = (trailPos + 1) % 24;
}

void FullCameraView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (fg);
    g.setFont (dwui::font (26.0f));
    g.drawText ("Timewind", 24, 14, 200, 40, juce::Justification::centredLeft);
    g.setColour (fg3);
    g.setFont (dwui::font (13.0f));
    g.drawText ("Camera stage. Move to play. Esc to go back.", 170, 14, 400, 40, juce::Justification::centredLeft);

    // Fit a 4:3 picture into the stage.
    auto pic = stage;
    const float aspect = 4.0f / 3.0f;
    if (pic.getWidth() / pic.getHeight() > aspect) pic = pic.withSizeKeepingCentre (pic.getHeight() * aspect, pic.getHeight());
    else pic = pic.withSizeKeepingCentre (pic.getWidth(), pic.getWidth() / aspect);

    const float level = juce::jlimit (0.0f, 1.0f, proc.outputLevel.load() * 1.6f);
    auto frame = proc.camera.getColourFrame();
    if (frame.isValid())
    {
        // Mirror, so it behaves like a mirror.
        g.saveState();
        g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f, pic.getCentreX(), pic.getCentreY()));
        g.setOpacity (0.72f);
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImage (frame, pic);
        g.restoreState();
        // What's moving right now (fingers included), glowing in acid.
        auto mask = proc.camera.getPreview();
        if (mask.isValid())
        {
            g.setOpacity (0.35f);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (mask, pic);
            g.setOpacity (1.0f);
        }
    }
    else
    {
        g.setColour (fg3);
        g.setFont (dwui::font (18.0f));
        g.drawText (proc.camera.isOpen() ? "waiting for the camera..." : "turn on CAMERA (right) to start",
                    pic.toNearestInt(), juce::Justification::centred);
    }

    // Slime mold grows over the picture.
    g.setOpacity (frame.isValid() ? 0.55f : 0.9f);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (slime.latest (level), pic);
    g.setOpacity (1.0f);
    g.setColour (rule);
    g.drawRect (pic, 1.0f);

    // Hand trail and crosshair
    if (proc.camera.isOpen())
    {
        juce::Path trail;
        for (int k = 0; k < 24; ++k)
        {
            const int i = (trailPos + k) % 24;
            const float x = pic.getCentreX() + trailX[i] * pic.getWidth() * 0.5f;
            const float y = pic.getCentreY() - trailY[i] * pic.getHeight() * 0.5f;
            if (k == 0) trail.startNewSubPath (x, y); else trail.lineTo (x, y);
        }
        g.setColour (acid.withAlpha (0.5f));
        g.strokePath (trail, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float hx = pic.getCentreX() + proc.monitor[O_CAM_X].load() * pic.getWidth() * 0.5f;
        const float hy = pic.getCentreY() - proc.monitor[O_CAM_Y].load() * pic.getHeight() * 0.5f;
        const float m = proc.monitor[O_CAM_MOTION].load();
        const float rad = 16.0f + m * 30.0f;
        g.setColour (acid.withAlpha (0.25f));
        g.fillEllipse (hx - rad, hy - rad, rad * 2, rad * 2);
        g.setColour (acid);
        g.drawEllipse (hx - 14, hy - 14, 28, 28, 2.0f);
    }
    if (frame.isValid()) drawSkeleton (g, proc.camera.hand.getPose(), pic, 1.6f);

    // Readouts
    g.setColour (fg2);
    g.setFont (dwui::mono (14.0f));
    g.drawText (juce::String::formatted ("hand x %+.2f    hand y %+.2f    motion %.2f",
                 proc.monitor[O_CAM_X].load(), proc.monitor[O_CAM_Y].load(), proc.monitor[O_CAM_MOTION].load()),
                (int) pic.getX(), (int) pic.getBottom() + 8, (int) pic.getWidth(), 20, juce::Justification::left);
    {
        // Finger gestures from the hand skeleton
        const auto& ht = proc.camera.hand;
        const float vals[] { proc.monitor[O_CAM_PINCH].load(), proc.monitor[O_CAM_SPREAD].load(),
                             proc.monitor[O_CAM_FIST].load(), proc.monitor[O_CAM_TILT].load() * 0.5f + 0.5f };
        const char* names[] { "PINCH", "SPREAD", "FIST", "TILT" };
        const float bw = 110.0f, y = pic.getBottom() + 34.0f;
        for (int k = 0; k < 4; ++k)
        {
            const float x = pic.getX() + (float) k * (bw + 24.0f);
            g.setColour (fg3); g.setFont (dwui::mono (11.0f));
            g.drawText (names[k], (int) x, (int) y, (int) bw, 14, juce::Justification::left);
            g.setColour (rule); g.fillRect (x, y + 17.0f, bw, 3.0f);
            g.setColour (acid); g.fillRect (x, y + 17.0f, bw * juce::jlimit (0.0f, 1.0f, vals[k]), 3.0f);
        }
        g.setColour (fg3); g.setFont (dwui::mono (11.0f));
        const juce::String hs = ! ht.enabled.load() ? juce::String ("hand skeleton: off (BONES switch)")
                              : ! ht.isReady() ? ht.getStatus().toLowerCase()
                              : ht.present.load() ? juce::String::formatted ("hand skeleton: tracking  %.0f ms", ht.inferenceMs.load())
                                                  : juce::String ("hand skeleton: show your hand");
        g.drawText (hs, (int) (pic.getX() + 4 * (bw + 24.0f)), (int) y, 300, 22, juce::Justification::left);
    }

    // Side panel
    const int panelX = getWidth() - 300;
    g.setColour (rule);
    g.drawVerticalLine (panelX, 60.0f, (float) getHeight() - 20.0f);
    g.setColour (fg);
    g.setFont (dwui::font (18.0f));
    g.drawText ("Hand Play", panelX + 20, 56, 200, 24, juce::Justification::left);
    for (auto& l : labs)
    {
        g.setColour (fg2);
        g.setFont (dwui::font (12.0f).withExtraKerningFactor (0.06f));
        g.drawText (l.t, l.r, juce::Justification::centred);
    }
    // World bar
    const auto bar = juce::Rectangle<float> ((float) panelX + 40, (float) sceneButton.getBottom() + 50, 210, 1);
    g.setColour (rule);
    g.fillRect (bar);
    const char* names[] { "airport", "tape", "cosmic" };
    for (int k = 0; k < 3; ++k)
    {
        const float x = bar.getX() + bar.getWidth() * (float) k / 2.0f;
        g.setColour (fg3);
        g.setFont (dwui::font (12.0f));
        g.drawText (names[k], (int) x - 35, (int) bar.getY() + 8, 70, 16, juce::Justification::centred);
    }
    const float px = bar.getX() + bar.getWidth() * (proc.monitor[O_CAM_X].load() * 0.5f + 0.5f);
    drawGlowDot (g, { px, bar.getY() }, 1.0f, 5.0f);
    g.setColour (fg3);
    g.setFont (dwui::font (12.0f));
    g.drawFittedText ("Left / right: world\nUp / down: bright + space\nMove more: denser, wilder\nQuick wave: new scene",
                      juce::Rectangle<int> (panelX + 20, (int) bar.getY() + 40, 260, 80), juce::Justification::topLeft, 5);
}

//==============================================================================
MainView::MainView (DriftwerkProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);

    specs = {
        // top band
        { M_GRAIN, "Grain", "your sound, in pieces", 0, 0, 560, 8,
          { { "grain.pos", "POSITION", 0, 14, 142 }, { "grain.spray", "SPRAY", 0, 82, 142 }, { "grain.size", "SIZE", 0, 150, 142 },
            { "grain.density", "DENSITY", 0, 218, 142 }, { "grain.pitch", "PITCH", 0, 286, 142 }, { "grain.reverse", "REVERSE", 0, 354, 142 },
            { "grain.scan", "SCAN", 0, 422, 142 }, { "grain.level", "LEVEL", 0, 490, 142 } }, true },
        { -3, "Hand Play", "the camera morphs everything", 0, 1276, 224, 1,
          { { "play.on", "PLAY", 2, 28, 44 }, { "play.depth", "DEPTH", 0, 140, 44 } } },
        // rack row 1
        { M_CLOCK, "Clock", "time + tune", 1, 0, 110, 1,
          { { "clk.div", "RATE", 1 }, { "clk.bpm", "BPM", 0 }, { "clk.run", "RUN", 2 }, { "out.tune", "TUNE", 0 } } },
        { M_ARP, "Random Arp", "shift register", 1, 110, 250, 4,
          { { "arp.on", "ON", 2 }, { "arp.follow", "HARMONY", 1 }, { "arp.scale", "SCALE", 1 }, { "arp.root", "ROOT", 0 },
            { "arp.range", "RANGE", 0 }, { "arp.len", "LENGTH", 0 }, { "arp.density", "DENSITY", 0 }, { "arp.mutate", "MUTATE", 0 },
            { "arp.walk", "WALK", 0 }, { "arp.gate", "GATE", 0 } } },
        { M_VCO1, "VCO 1", "oscillator", 1, 360, 140, 2,
          { { "vco1.oct", "OCTAVE", 0 }, { "vco1.fine", "FINE", 0 }, { "vco1.pw", "WIDTH", 0 }, { "vco1.pwm", "PWM", 0 }, { "vco1.fm", "FM", 0 } } },
        { M_VCO2, "VCO 2", "oscillator", 1, 500, 140, 2,
          { { "vco2.oct", "OCTAVE", 0 }, { "vco2.fine", "FINE", 0 }, { "vco2.pw", "WIDTH", 0 }, { "vco2.pwm", "PWM", 0 }, { "vco2.fm", "FM", 0 } } },
        { M_DRONE, "Drone", "six drifting voices", 1, 640, 160, 2,
          { { "drn.chord", "CHORD", 1 }, { "drn.oct", "OCTAVE", 0 }, { "drn.detune", "DETUNE", 0 }, { "drn.drift", "DRIFT", 0 }, { "drn.level", "LEVEL", 0 } } },
        { M_NOISE, "Noise", "sample + hold", 1, 800, 100, 1, { { "nz.slew", "SLEW", 0 } } },
        { M_LFO1, "LFO 1", "slow", 1, 900, 100, 1, { { "lfo1.rate", "RATE", 0 }, { "lfo1.shape", "SHAPE", 1 } } },
        { M_LFO2, "LFO 2", "slow", 1, 1000, 100, 1, { { "lfo2.rate", "RATE", 0 }, { "lfo2.shape", "SHAPE", 1 } } },
        { M_DREAM, "Dream", "loops of unequal length", 1, 1100, 170, 2,
          { { "dream.chord", "CHORD", 1 }, { "dream.rate", "SPEED", 0 }, { "dream.tone", "TONE", 0 }, { "dream.warp", "WARP", 0 }, { "dream.level", "LEVEL", 0 } } },
        { M_CAMERA, "Hand", "webcam tracking", 1, 1270, 230, 1,
          { { "cam.on", "CAM", 2, 6, 160 }, { "cam.hands", "BONES", 2, 62, 160 }, { "cam.sens", "SENS", 0, 118, 160 }, { "cam.slew", "SMOOTH", 0, 172, 160 } } },
        // rack row 2
        { M_ENV1, "Env 1", "adsr", 2, 0, 130, 2, { { "env1.a", "ATTACK", 0 }, { "env1.d", "DECAY", 0 }, { "env1.s", "SUSTAIN", 0 }, { "env1.r", "RELEASE", 0 } } },
        { M_ENV2, "Env 2", "adsr", 2, 130, 130, 2, { { "env2.a", "ATTACK", 0 }, { "env2.d", "DECAY", 0 }, { "env2.s", "SUSTAIN", 0 }, { "env2.r", "RELEASE", 0 } } },
        { M_MIX, "Mixer", "4 into 1", 2, 260, 120, 2, { { "mix.l1", "IN 1", 0 }, { "mix.l2", "IN 2", 0 }, { "mix.l3", "IN 3", 0 }, { "mix.l4", "IN 4", 0 } } },
        { M_VCF1, "VCF 1", "ladder", 2, 380, 150, 2,
          { { "vcf1.cut", "CUTOFF", 0 }, { "vcf1.res", "RESO", 0 }, { "vcf1.drive", "DRIVE", 0 }, { "vcf1.cv1", "CV 1", 0 }, { "vcf1.cv2", "CV 2", 0 } } },
        { M_VCF2, "VCF 2", "ladder", 2, 530, 150, 2,
          { { "vcf2.cut", "CUTOFF", 0 }, { "vcf2.res", "RESO", 0 }, { "vcf2.drive", "DRIVE", 0 }, { "vcf2.cv1", "CV 1", 0 }, { "vcf2.cv2", "CV 2", 0 } } },
        { M_VCA1, "VCA 1", "linear", 2, 680, 100, 1, { { "vca1.gain", "BASE", 0 }, { "vca1.cv", "CV AMT", 0 } } },
        { M_VCA2, "VCA 2", "linear", 2, 780, 100, 1, { { "vca2.gain", "BASE", 0 }, { "vca2.cv", "CV AMT", 0 } } },
        { M_MASTER, "Output", "ensemble + master", 2, 880, 140, 2,
          { { "out.lvl1", "LEVEL 1", 0 }, { "out.lvl2", "LEVEL 2", 0 }, { "out.pan1", "PAN 1", 0 }, { "out.pan2", "PAN 2", 0 }, { "out.ens", "ENSEMBLE", 0 }, { "out.vol", "VOLUME", 0 } } },
        { -4, "Tape", "worn cassette", 2, 1020, 140, 2, { { "tape.wow", "WOW", 0 }, { "tape.age", "AGE", 0 }, { "tape.hiss", "HISS", 0 }, { "tape.crush", "CRUSH", 0 } } },
        { -1, "Echo", "ping-pong tape", 2, 1160, 170, 2,
          { { "echo.div", "TIME", 1 }, { "echo.ms", "FREE MS", 0 }, { "echo.fb", "REPEATS", 0 }, { "echo.mix", "MIX", 0 }, { "echo.wow", "WOW", 0 } } },
        { -2, "Hall", "shimmer cloud", 2, 1330, 170, 2,
          { { "hall.size", "SIZE", 0 }, { "hall.damp", "DAMP", 0 }, { "hall.mix", "MIX", 0 }, { "hall.shimmer", "SHIMMER", 0 } } },
        // right-hand column: the bass line (rows 0 and 1) and the mixer of every layer (row 2)
        { M_BASS, "Bass", "a line on the arp's clock and chords", 3, 1500, 270, 4,
          { { "bass.on", "ON", 2, 6, 44 }, { "bass.mode", "MODE", 1, 64, 44, 104 }, { "bass.rate", "RATE", 1, 172, 44, 92 },
            { "bass.range", "RANGE", 1, 6, 104, 64 }, { "bass.wave", "WAVE", 1, 72, 104, 92 }, { "bass.table", "TABLE", 1, 166, 104, 98 },
            { "bass.level", "LEVEL", 0, 6, 384, 64 }, { "bass.density", "DENSITY", 0, 70, 384, 64 },
            { "bass.length", "LENGTH", 0, 134, 384, 64 }, { "bass.glide", "GLIDE", 0, 198, 384, 64 },
            { "bass.shape", "SHAPE", 0, 6, 454, 64 }, { "bass.sub", "SUB", 0, 70, 454, 64 },
            { "bass.cutoff", "CUTOFF", 0, 134, 454, 64 }, { "bass.reso", "RESO", 0, 198, 454, 64 },
            { "bass.env", "ENV", 0, 38, 524, 64 }, { "bass.decay", "DECAY", 0, 102, 524, 64 }, { "bass.drive", "DRIVE", 0, 166, 524, 64 } } },
        { -6, "Layers", "level, mute and meter for every sound", 2, 1500, 270, 1, {} },
    };

    for (int i = 0; i < NumOuts; ++i) { outJacks.push_back (std::make_unique<JackComponent> (*this, true, i)); addAndMakeVisible (*outJacks.back()); }
    for (int i = 0; i < NumIns; ++i)  { inJacks.push_back (std::make_unique<JackComponent> (*this, false, i)); addAndMakeVisible (*inJacks.back()); }
    for (int s = 0; s < (int) specs.size(); ++s) buildModule (s);

    // Top band extras
    const int t0 = rowTop (0);
    grainView = std::make_unique<GrainView> (proc);
    grainView->setBounds (10, t0 + 40, 540, 92);
    grainView->onClickLoad = [this] { chooseSample(); };
    addAndMakeVisible (*grainView);
    loadButton.setBounds (452, t0 + 8, 98, 22);
    loadButton.onClick = [this] { chooseSample(); };
    addAndMakeVisible (loadButton);
    // Built-in sounds (or your own file) and the IN KEY lock.
    soundBox.addItemList (DriftwerkProcessor::grainSourceNames(), 1);
    soundBox.setBounds (238, t0 + 8, 140, 22);
    soundBox.setTooltip ("Sound for GRAIN: one of the built-in sounds, or MY FILE (the last file you loaded)");
    addAndMakeVisible (soundBox);
    soundAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "grain.src", soundBox);
    keyButton.setBounds (384, t0 + 8, 62, 22);
    keyButton.setClickingTogglesState (true);
    keyButton.setTooltip ("IN KEY: moves the sound into Timewind's key and snaps PITCH to octaves and fifths");
    addAndMakeVisible (keyButton);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "grain.key", keyButton);

    // DREAM: the SOUND menu and LOAD sit under its knobs; drop a file on the module to load it.
    {
        const int t1 = rowTop (1);
        dreamRect = { 1100, t1, 170, rowHeight (1) };
        dreamSoundBox.addItemList (DriftwerkProcessor::dreamSoundNames(), 1);
        dreamSoundBox.setBounds (1108, t1 + 250, 104, 22);
        dreamSoundBox.setTooltip ("What the DREAM loops play: a built-in sound, or MY SOUND (a file you load, or drop on DREAM)");
        addAndMakeVisible (dreamSoundBox);
        dreamSoundAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "dream.sound", dreamSoundBox);
        dreamSoundBox.onChange = [this] { repaint (dreamNameArea); };
        dreamLoadButton.setBounds (1216, t1 + 250, 46, 22);
        dreamLoadButton.setTooltip ("Load your own sound for DREAM: every loop plays it at the chord's notes");
        dreamLoadButton.onClick = [this] { chooseDreamSound(); };
        addAndMakeVisible (dreamLoadButton);
        dreamNameArea = { 1104, t1 + 276, 162, 14 };
    }

    slimeRect = { 568, t0, 700, rowHeight (0) };
    slime = std::make_unique<SlimeView> (proc);
    slime->setBounds (slimeRect);
    addAndMakeVisible (*slime);
    perform = std::make_unique<PerformPanel> (proc, *this);
    perform->setBounds (slimeRect);
    addAndMakeVisible (*perform);

    playRect = { 1276, t0, 224, rowHeight (0) };
    sceneButton.setBounds (playRect.getX() + 16, t0 + 124, 192, 34);
    sceneButton.onClick = [this] { proc.rollScene(); refreshPatch(); };
    addAndMakeVisible (sceneButton);
    scope = std::make_unique<ScopeComponent> (proc);
    scope->setBounds (playRect.getX() + 12, t0 + 170, 200, 70);
    addAndMakeVisible (*scope);

    bassView = std::make_unique<BassView> (proc);
    bassView->setBounds (1510, rowTop (3) + 172, 250, 200);
    addAndMakeVisible (*bassView);
    layers = std::make_unique<LayersPanel> (proc);
    layers->setBounds (1508, rowTop (2) + 44, 256, DriftwerkProcessor::numLayers * LayersPanel::rowH);
    addAndMakeVisible (*layers);

    cameraView = std::make_unique<CameraView> (proc);
    cameraView->setBounds (1270 + 10, rowTop (1) + 40, 210, 112);
    cameraView->onClick = [this] { openFullCamera (true); };   // click the little picture to go full screen
    cameraView->setMouseCursor (juce::MouseCursor::PointingHandCursor);
    cameraView->setTooltip ("Click for the full-screen camera");
    addAndMakeVisible (*cameraView);

    // Header
    rebuildPresetMenu();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 1000)
        {
            if (const int k = id - 1001; k < userPresetFiles.size()) { proc.loadUserPreset (userPresetFiles[k]); refreshPatch(); }
        }
        else if (id > 0) { proc.loadPreset (id - 1); refreshPatch(); }
    };
    addAndMakeVisible (presetBox);
    presetSaveButton.setTooltip ("Save this sound as one of your presets (they're listed under MY PRESETS)");
    presetSaveButton.onClick = [this] { showPresetSaveMenu(); };
    addAndMakeVisible (presetSaveButton);
    randomButton.onClick = [this] { proc.randomisePatch(); refreshPatch(); };
    clearButton.onClick = [this] { for (int i = 0; i < NumIns; ++i) proc.disconnect (i); refreshPatch(); };
    dimButton.setClickingTogglesState (true);
    dimButton.onClick = [this] { cablesDimmed = dimButton.getToggleState(); cables->repaint(); };
    addAndMakeVisible (randomButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (dimButton);

    cables = std::make_unique<CableLayer> (*this);
    addAndMakeVisible (*cables);

    recordButton.getProperties().set ("accent", true);
    recordButton.onClick = [this]
    {
        if (proc.isRecording()) { proc.stopRecording(); recordButton.setToggleState (false, juce::dontSendNotification); recordButton.setButtonText ("REC"); }
        else if (proc.startRecording()) recordButton.setToggleState (true, juce::dontSendNotification);
    };
    filesButton.onClick = [this]
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Timewind");
        dir.createDirectory();
        if (proc.lastRecording.existsAsFile()) proc.lastRecording.revealToUser(); else dir.startAsProcess();
    };
    addAndMakeVisible (recordButton);
    addAndMakeVisible (filesButton);
    screenButton.onClick = [this]
    {
        if (onFullScreen) onFullScreen();
    };
    addAndMakeVisible (screenButton);
    fullButton.onClick = [this] { openFullCamera (true); };
    fullButton.getProperties().set ("accent", true);
    addAndMakeVisible (fullButton);
    fullCam = std::make_unique<FullCameraView> (proc, *slime);
    fullCam->onClose = [this] { openFullCamera (false); };
    addChildComponent (*fullCam);

    setSize (designW, designH);
    lastPatch = proc.getPatchString();
    startTimerHz (30);
}

MainView::~MainView() { setLookAndFeel (nullptr); }

void MainView::buildModule (int specIndex)
{
    const auto& sp = specs[(size_t) specIndex];
    const int top = rowTop (sp.row), h = rowHeight (sp.row);
    moduleRects.push_back ({ sp.x, top, sp.w, h });

    int y0 = top + 44;
    if (sp.module == M_ARP) { arpLedArea = { sp.x + 12, top + 40, sp.w - 24, 14 }; y0 += 20; }

    const int cellW = (sp.w - 12) / sp.cols;
    const int cellH = sp.module == M_ARP || sp.module == M_CLOCK ? 60 : 66;

    for (int c = 0; c < (int) sp.controls.size(); ++c)
    {
        const auto& cd = sp.controls[(size_t) c];
        const int col = c % sp.cols, row = c / sp.cols;
        const juce::Rectangle<int> cell = cd.rx >= 0 ? juce::Rectangle<int> (sp.x + cd.rx, top + cd.ry, cd.rw, cellH)
                                                     : juce::Rectangle<int> (sp.x + 6 + col * cellW, y0 + row * cellH, cellW, cellH);
        if (cd.kind == 0)
        {
            auto s = std::make_unique<ModSlider>();
            s->paramIndex = proc.paramIndex (cd.param);
            auto* raw = s.get();
            s->onRightClick = [this, raw] { showModMenu (*raw); };
            s->setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
            s->setPopupDisplayEnabled (true, true, this, 900);
            s->setBounds (cell.withSizeKeepingCentre (46, 46).withY (cell.getY() + 1));
            s->setMouseDragSensitivity (220);
            addAndMakeVisible (*s);
            sliderAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, cd.param, *s));
            sliders.push_back (std::move (s));
            labels.push_back ({ { cell.getX() - 6, cell.getY() + 48, cell.getWidth() + 12, 14 }, cd.label, false });
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
            labels.push_back ({ { cell.getX(), cell.getY() + 42, cell.getWidth(), 14 }, cd.label, false });
        }
        else
        {
            auto t = std::make_unique<juce::ToggleButton>();
            t->setBounds (cell.withSizeKeepingCentre (48, 40).withY (cell.getY() + 4));
            addAndMakeVisible (*t);
            toggleAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, cd.param, *t));
            toggles.push_back (std::move (t));
            labels.push_back ({ { cell.getX() - 6, cell.getY() + 48, cell.getWidth() + 12, 14 }, cd.label, false });
        }
    }

    if (sp.module < 0) return;

    const int ledIndex = sp.module == M_LFO1 ? 0 : sp.module == M_LFO2 ? 1 : sp.module == M_ENV1 ? 2 : sp.module == M_ENV2 ? 3 : -1;
    if (ledIndex >= 0) moduleLedAreas[(size_t) ledIndex] = { sp.x + sp.w - 20, top + 12, 10, 10 };

    std::vector<int> ins, outsList;
    for (int i = 0; i < NumIns; ++i)  if (inDefs[i].module == sp.module)  ins.push_back (i);
    for (int i = 0; i < NumOuts; ++i) if (outDefs[i].module == sp.module) outsList.push_back (i);

    const int jackW = 44, jackH = 46;
    auto place = [&] (int jackIdx, bool isOut, int x, int yy)
    {
        auto& jack = isOut ? *outJacks[(size_t) jackIdx] : *inJacks[(size_t) jackIdx];
        jack.setBounds (x + jackW / 2 - 17, yy + 11, 34, 36);
        labels.push_back ({ { x - 4, yy + 2, jackW + 8, 12 }, isOut ? outDefs[jackIdx].label : inDefs[jackIdx].label, true });
    };

    if (sp.compactJacks)
    {
        const int total = (int) (ins.size() + outsList.size());
        const int y = top + h - 10 - jackH;
        const int x0 = sp.x + sp.w - 12 - total * jackW;
        for (size_t k = 0; k < ins.size(); ++k) place (ins[k], false, x0 + (int) k * jackW, y);
        const int xo = x0 + (int) ins.size() * jackW;
        if (! outsList.empty()) outputPlates.push_back ({ xo - 2, y - 4, (int) outsList.size() * jackW + 4, jackH + 6 });
        for (size_t k = 0; k < outsList.size(); ++k) place (outsList[k], true, xo + (int) k * jackW, y);
        return;
    }

    const int perRow = juce::jmax (1, (sp.w - 8) / jackW);
    auto rowsFor = [perRow] (size_t n) { return n == 0 ? 0 : (int) ((n + (size_t) perRow - 1) / (size_t) perRow); };
    const int outRows = rowsFor (outsList.size()), inRows = rowsFor (ins.size());
    const int y = top + h - 10 - outRows * jackH;
    if (outRows > 0) outputPlates.push_back ({ sp.x + 5, y - 4, sp.w - 10, outRows * jackH + 6 });

    auto placeRow = [&] (const std::vector<int>& list, bool isOut, int yStart)
    {
        for (size_t k = 0; k < list.size(); ++k)
        {
            const int row = (int) k / perRow, col = (int) k % perRow;
            const int inThisRow = juce::jmin (perRow, (int) list.size() - row * perRow);
            const int x = sp.x + (sp.w - inThisRow * jackW) / 2 + col * jackW;
            place (list[k], isOut, x, yStart + row * jackH);
        }
    };
    placeRow (outsList, true, y);
    placeRow (ins, false, y - 6 - inRows * jackH);
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // Header, in the site's voice
    g.setColour (fg);
    g.setFont (dwui::font (28.0f).withExtraKerningFactor (0.02f));
    g.drawText ("Timewind", 16, 8, 200, 40, juce::Justification::centredLeft);
    g.setColour (fg3);
    g.setFont (dwui::font (13.0f));
    g.drawText ("Grow sound with your hands.", 150, 8, 260, 40, juce::Justification::centredLeft);
    g.setColour (fg3);
    g.setFont (dwui::font (11.0f));
    g.drawFittedText ("drag jacks to patch, right-click to unplug\nright-click a knob to play it by hand",
                      330, 8, 205, 40, juce::Justification::centredLeft, 2);
    g.setColour (rule);
    g.drawHorizontalLine (52, 0.0f, (float) getWidth());

    // Modules
    for (size_t i = 0; i < specs.size(); ++i)
    {
        const auto& sp = specs[i];
        const auto r = moduleRects[i].toFloat().reduced (2.0f, 0.0f);
        g.setColour (panel);
        g.fillRect (r);
        g.setColour (rule);
        g.drawRect (r, 1.0f);
        g.setColour (fg);
        g.setFont (dwui::font (15.0f));
        g.drawText (sp.title, (int) r.getX() + 10, (int) r.getY() + 6, (int) r.getWidth() - 20, 18, juce::Justification::left);
        g.setColour (fg3);
        g.setFont (dwui::font (10.5f));
        g.drawText (sp.subtitle, (int) r.getX() + 10, (int) r.getY() + 22, (int) r.getWidth() - 20, 13, juce::Justification::left);
        g.setColour (acid);
        g.fillRect (r.getX() + 10.0f, r.getY() + 37.0f, 14.0f, 1.0f);
    }

    for (auto& pl : outputPlates)
    {
        g.setColour (acid.withAlpha (0.05f));
        g.fillRect (pl.toFloat());
        g.setColour (acid.withAlpha (0.18f));
        g.drawRect (pl.toFloat(), 1.0f);
    }

    for (auto& l : labels)
    {
        g.setColour (l.jack ? fg3 : fg2);
        g.setFont (dwui::font (l.jack ? 9.5f : 10.5f).withExtraKerningFactor (0.06f));
        g.drawText (l.text, l.r, juce::Justification::centred);
    }
    if (! dreamNameArea.isEmpty())
    {
        g.setColour (fg2);
        g.setFont (dwui::font (10.5f).withExtraKerningFactor (0.06f));
        g.drawFittedText (dreamLabelText(), dreamNameArea, juce::Justification::centred, 1, 0.8f);
    }

    // Hand Play: where your hand sits between the three worlds
    {
        const int t0 = rowTop (0);
        const auto bar = juce::Rectangle<float> ((float) playRect.getX() + 40, (float) t0 + 262, 144, 1);
        g.setColour (rule);
        g.fillRect (bar);
        g.setFont (dwui::font (10.5f));
        const char* names[] { "airport", "tape", "cosmic" };
        for (int k = 0; k < 3; ++k)
        {
            const float x = bar.getX() + bar.getWidth() * (float) k / 2.0f;
            g.setColour (fg3);
            g.drawText (names[k], (int) x - 30, t0 + 268, 60, 14, juce::Justification::centred);
            g.fillEllipse (x - 2, bar.getY() - 2, 4, 4);
        }
        const float hx = proc.monitor[O_CAM_X].load();
        const float px = bar.getX() + bar.getWidth() * (hx * 0.5f + 0.5f);
        drawGlowDot (g, { px, bar.getY() }, 1.0f, 4.0f);
    }

    // Arp steps
    if (! arpLedArea.isEmpty())
    {
        const int step = proc.arpStep.load(), len = proc.arpLength.load();
        const float gate = proc.monitor[O_ARP_GATE].load();
        const float dx = (float) arpLedArea.getWidth() / 16.0f;
        for (int i = 0; i < 16; ++i)
        {
            const juce::Point<float> c (arpLedArea.getX() + dx * (i + 0.5f), (float) arpLedArea.getCentreY());
            const float level = i == step ? (gate > 0.5f ? 1.0f : 0.5f) : (i < len ? 0.12f : 0.0f);
            drawGlowDot (g, c, level, 2.6f);
        }
    }

    const int ledOuts[] { O_LFO1, O_LFO2, O_ENV1, O_ENV2 };
    for (int i = 0; i < 4; ++i)
        if (! moduleLedAreas[(size_t) i].isEmpty())
        {
            const float v = proc.monitor[(size_t) ledOuts[i]].load();
            drawGlowDot (g, moduleLedAreas[(size_t) i].getCentre().toFloat(), i < 2 ? v * 0.5f + 0.5f : v, 3.0f);
        }
}

void MainView::showPerformTab (int t)
{
    if (t >= 100) { perform->setTab (t / 100); perform->setPulsePage (t % 100); }   // 601: the PULSE tab's SOUND page
    else perform->setTab (t);
}
void MainView::setFullScreenLabel (bool isFull) { screenButton.setButtonText (isFull ? "EXIT FULL" : "FULL SCREEN"); }

void MainView::openFullCamera (bool open)
{
    fullCam->setVisible (open);
    if (open) { fullCam->toFront (true); fullCam->grabKeyboardFocus(); }
}

void MainView::resized()
{
    cables->setBounds (getLocalBounds());
    fullCam->setBounds (getLocalBounds());
    recordButton.setBounds (getWidth() - 958, 14, 76, 26);
    filesButton.setBounds (getWidth() - 878, 14, 88, 26);
    screenButton.setBounds (getWidth() - 786, 14, 98, 26);
    fullButton.setBounds (getWidth() - 684, 14, 76, 26);
    presetBox.setBounds (getWidth() - 600, 14, 152, 26);
    presetSaveButton.setBounds (getWidth() - 444, 14, 50, 26);
    randomButton.setBounds (getWidth() - 390, 14, 130, 26);
    clearButton.setBounds (getWidth() - 252, 14, 100, 26);
    dimButton.setBounds (getWidth() - 144, 14, 128, 26);
}

//==============================================================================
// Presets: the built-in ones by group, then MY PRESETS (files in Timewind's presets folder).
void MainView::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    juce::String group;
    for (int i = 0; i < DriftwerkProcessor::getNumPresets(); ++i)
    {
        if (DriftwerkProcessor::getPresetGroup (i) != group)
        {
            group = DriftwerkProcessor::getPresetGroup (i);
            presetBox.addSectionHeading (group);
        }
        presetBox.addItem (DriftwerkProcessor::getPresetName (i), i + 1);
    }
    userPresetFiles = DriftwerkProcessor::userPresets();
    presetBox.addSectionHeading ("MY PRESETS");
    if (userPresetFiles.isEmpty()) presetBox.addItem ("(none yet: SAVE makes one)", 999), presetBox.setItemEnabled (999, false);
    for (int k = 0; k < userPresetFiles.size(); ++k) presetBox.addItem (userPresetFiles[k].getFileNameWithoutExtension(), 1001 + k);
    presetBox.setSelectedId (proc.getCurrentPreset() + 1, juce::dontSendNotification);
}

void MainView::showPresetSaveMenu()
{
    const auto mine = proc.userPresetName;
    juce::PopupMenu m;
    m.addItem (1, "Save as a new preset...");
    if (mine.isNotEmpty())
    {
        m.addItem (2, "Save over \"" + mine + "\"");
        m.addItem (3, "Delete \"" + mine + "\"");
    }
    m.addSeparator();
    m.addItem (4, "Show the presets folder");
    juce::Component::SafePointer<MainView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetSaveButton), [safe, mine] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        if (r == 1)
            self.askPresetName (mine.isNotEmpty() ? mine + " 2" : DriftwerkProcessor::getPresetName (self.proc.getCurrentPreset()) + " (mine)");
        else if (r == 2) { self.proc.saveUserPreset (mine); self.rebuildPresetMenu(); }
        else if (r == 3)
        {
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete preset",
                                                "Delete your preset \"" + mine + "\"? It goes to the recycle bin.", "DELETE", "KEEP", nullptr,
                juce::ModalCallbackFunction::create ([safe, mine] (int ok)
                {
                    if (safe == nullptr || ok == 0) return;
                    DriftwerkProcessor::userPresetFolder().getChildFile (mine + ".timewind").moveToTrash();
                    safe->proc.userPresetName.clear();
                    safe->rebuildPresetMenu();
                }));
        }
        else if (r == 4)
        {
            const auto folder = DriftwerkProcessor::userPresetFolder();
            folder.createDirectory();
            folder.startAsProcess();
        }
    });
}

void MainView::askPresetName (const juce::String& suggestion)
{
    auto* aw = new juce::AlertWindow ("Save preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", suggestion);
    aw->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MainView> safe (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int r)
    {
        if (safe == nullptr || r == 0) return;
        const auto name = aw->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;
        if (safe->proc.saveUserPreset (name)) safe->rebuildPresetMenu();
    }), true);
}

//==============================================================================
void MainView::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a sound for GRAIN", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile() && proc.loadSample (f)) useMyFile();
            grainView->repaint();
        });
}

juce::String MainView::dreamLabelText() const
{
    if (dreamSoundBox.getSelectedId() - 1 != DriftwerkProcessor::dreamMySound) return "SOUND";
    if (! proc.hasDreamSound()) return "NO FILE YET: LOAD ONE";
    return juce::File (proc.dreamPath).getFileNameWithoutExtension().toUpperCase();
}

void MainView::chooseDreamSound()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a sound for DREAM", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile() && proc.loadDreamSound (f)) useDreamSound();
            repaint (dreamNameArea);
        });
}

void MainView::useDreamSound()
{
    if (auto* ds = proc.apvts.getParameter ("dream.sound"))
    {
        ds->beginChangeGesture();
        ds->setValueNotifyingHost (ds->convertTo0to1 ((float) DriftwerkProcessor::dreamMySound));
        ds->endChangeGesture();
    }
    repaint (dreamNameArea);
}

void MainView::filesDropped (const juce::StringArray& files, int x, int y)
{
    grainView->dragHover = false;
    if (perform != nullptr && perform->isVisible() && perform->getBounds().contains (x, y) && ! files.isEmpty())
        if (perform->fileDropped (files[0], juce::Point<int> (x, y) - perform->getPosition()))
            return;
    if (dreamRect.contains (x, y))
    {   // Dropped on DREAM: it becomes MY SOUND there.
        for (auto& path : files)
            if (proc.loadDreamSound (juce::File (path))) { useDreamSound(); break; }
        grainView->repaint();
        return;
    }
    for (auto& path : files)
        if (proc.loadSample (juce::File (path))) { useMyFile(); break; }
    grainView->repaint();
}

void MainView::useMyFile()
{
    if (auto* src = proc.apvts.getParameter ("grain.src"))
    {
        src->beginChangeGesture();
        src->setValueNotifyingHost (src->convertTo0to1 ((float) DriftwerkProcessor::myFileSource));
        src->endChangeGesture();
    }
    proc.syncGrainSource();
}

void MainView::showModMenu (ModSlider& s)
{
    if (s.paramIndex < 0) return;
    auto& slot = *proc.mods[(size_t) s.paramIndex];
    const int current = slot.source.load();
    const float depth = slot.depth.load();

    struct Src { int out; const char* name; };
    const Src handSources[] { { O_CAM_X, "Hand X  (left / right)" }, { O_CAM_Y, "Hand Y  (up / down)" },
                              { O_CAM_MOTION, "Hand motion  (how much you move)" },
                              { O_CAM_PINCH, "Pinch  (thumb to index finger)" }, { O_CAM_SPREAD, "Spread  (fingers fanned open)" },
                              { O_CAM_FIST, "Fist  (fingers curled)" }, { O_CAM_TILT, "Tilt  (hand leaning)" } };
    const Src otherSources[] { { O_LFO1, "LFO 1" }, { O_LFO2, "LFO 2" }, { O_SNH, "S&H random" },
                               { O_ENV2, "ENV 2" }, { O_ARP_RND, "Arp random" } };

    juce::PopupMenu m;
    m.addSectionHeader ("Control this knob with...");
    for (auto& h : handSources) m.addItem (1000 + h.out, h.name, true, current == h.out);
    juce::PopupMenu others;
    for (auto& o : otherSources) others.addItem (1000 + o.out, o.name, true, current == o.out);
    m.addSubMenu ("Other modulators", others);
    m.addItem (1, "Nothing (manual only)", current >= 0, current < 0);
    m.addSeparator();
    m.addSectionHeader ("Depth");
    const float depths[] { 0.15f, 0.3f, 0.5f, 1.0f, -0.3f, -0.5f, -1.0f };
    for (int k = 0; k < 7; ++k)
        m.addItem (100 + k, (depths[k] > 0 ? "+" : "") + juce::String (juce::roundToInt (depths[k] * 100)) + "%",
                   true, std::abs (depth - depths[k]) < 0.01f);

    juce::Component::SafePointer<ModSlider> safe (&s);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&s),
        [this, safe, depths] (int r)
        {
            if (r == 0 || safe == nullptr) return;
            auto& sl = *proc.mods[(size_t) safe->paramIndex];
            if (r == 1) sl.source.store (-1);
            else if (r >= 1000) sl.source.store (r - 1000);
            else if (r >= 100 && r < 107)
            {
                sl.depth.store (depths[r - 100]);
                if (sl.source.load() < 0) sl.source.store (O_CAM_Y);
            }
            proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
        });
}

juce::Point<float> MainView::jackCentre (bool output, int idx) const
{
    const auto& j = output ? *outJacks[(size_t) idx] : *inJacks[(size_t) idx];
    const auto b = j.getBounds().toFloat();
    return { b.getCentreX(), b.getBottom() - 14.0f };
}

juce::Colour MainView::cableColour (int out, int in) const
{
    return juce::Colour (veinPalette[(out * 7 + in * 3) % 6]);
}

void MainView::beginDrag (JackComponent& from)
{
    dragOut = dragIn = -1;
    if (from.isOutput) dragOut = from.idx;
    else
    {
        const int src = proc.patch[(size_t) from.idx].load();
        if (src >= 0) { proc.disconnect (from.idx); dragOut = src; }
        else dragIn = from.idx;
    }
}

void MainView::dragTo (juce::Point<float> pos) { dragPos = pos; cables->repaint(); }

void MainView::endDrag (juce::Point<float> pos)
{
    JackComponent* target = nullptr;
    for (auto* list : { &outJacks, &inJacks })
        for (auto& j : *list)
            if (j->isVisible() && j->getBounds().expanded (4).contains (pos.toInt())) target = j.get();
    if (target != nullptr)
    {
        if (dragOut >= 0 && ! target->isOutput) proc.connect (dragOut, target->idx);
        else if (dragIn >= 0 && target->isOutput) proc.connect (target->idx, dragIn);
    }
    dragOut = dragIn = -1;
    refreshPatch();
}

void MainView::clearJack (JackComponent& j)
{
    if (j.isOutput) { for (int i = 0; i < NumIns; ++i) if (proc.patch[(size_t) i].load() == j.idx) proc.disconnect (i); }
    else proc.disconnect (j.idx);
    refreshPatch();
}

void MainView::refreshPatch()
{
    lastPatch = proc.getPatchString();
    cables->repaint();
    proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
}

void MainView::timerCallback()
{
    perform->refresh();
    if (const auto dn = dreamLabelText(); dn != shownDreamName) { shownDreamName = dn; repaint (dreamNameArea); }
    if (proc.isRecording())
    {
        const int secs = (int) proc.recordingSeconds();
        recordButton.setButtonText (juce::String::formatted ("REC %d:%02d", secs / 60, secs % 60));
    }
    if (dragOut < 0 && dragIn < 0)
    {
        const auto ps = proc.getPatchString();
        if (ps != lastPatch) { lastPatch = ps; cables->repaint(); }
    }
    {
        int want = proc.getCurrentPreset() + 1;
        if (proc.userPresetName.isNotEmpty())
        {
            want = 0;
            for (int k = 0; k < userPresetFiles.size(); ++k)
                if (userPresetFiles[k].getFileNameWithoutExtension() == proc.userPresetName) want = 1001 + k;
        }
        if (want == 0 && presetBox.getText() != proc.userPresetName) presetBox.setText (proc.userPresetName, juce::dontSendNotification);
        else if (want != 0 && presetBox.getSelectedId() != want) presetBox.setSelectedId (want, juce::dontSendNotification);
    }

    for (auto& sl : sliders)
    {
        if (sl->paramIndex < 0) continue;
        auto& slot = *proc.mods[(size_t) sl->paramIndex];
        const int src = slot.source.load();
        const float now = src >= 0 ? slot.now.load() : -1.0f;
        auto& props = sl->getProperties();
        if ((int) props["modSrc"] != src || std::abs ((float) props["modNow"] - now) > 0.004f
            || std::abs ((float) props["modDepth"] - slot.depth.load()) > 0.001f)
        {
            props.set ("modSrc", src); props.set ("modNow", now); props.set ("modDepth", slot.depth.load());
            sl->repaint();
        }
    }

    for (auto& j : outJacks)
    {
        const float led = juce::jlimit (0.0f, 1.0f, std::abs (proc.monitor[(size_t) j->idx].load()) * 1.4f);
        if (std::abs (led - j->led) > 0.04f) { j->led = led; j->repaint(); }
    }

    if (fullCam->isVisible())
    {
        fullCam->pushTrail();
        fullCam->repaint();
        return;
    }
    slime->repaint();
    scope->repaint();
    grainView->repaint();
    bassView->refresh();
    layers->refresh();
    if ((++frameCounter & 1) == 0) cameraView->repaint();
    repaint (arpLedArea.expanded (8));
    repaint (playRect.withTrimmedTop (250));
    for (auto& r : moduleLedAreas) if (! r.isEmpty()) repaint (r.expanded (10));
}

//==============================================================================
BassView::BassView (DriftwerkProcessor& p) : proc (p)
{
    newButton.getProperties().set ("small", true);
    newButton.setTooltip ("NEW: a new ACID line, and the WALK starts again from the root");
    newButton.onClick = [this] { proc.bassNewReq.fetch_add (1); };
    addAndMakeVisible (newButton);
}

void BassView::refresh()
{
    const int head = proc.bassHistHead.load(), notes = proc.bassNotes.load();
    const bool on = proc.apvts.getRawParameterValue ("bass.on")->load() > 0.5f;
    if (head != seenHead || notes != seenNotes || on != seenOn) { seenHead = head; seenNotes = notes; seenOn = on; repaint(); }
}

void BassView::paint (juce::Graphics& g)
{
    using namespace dwui;
    const auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black);
    g.fillRect (b);
    g.setColour (rule);
    g.drawRect (b, 1.0f);
    g.setFont (dwui::font (10.0f).withExtraKerningFactor (0.08f));
    g.setColour (fg3);
    g.drawText ("THE LINE", 8, 5, 80, 16, juce::Justification::centredLeft);

    // The last 32 steps, oldest on the left. Each note sits at its pitch; accents are bright,
    // slides are joined to the note they glide into.
    const int size = DriftwerkProcessor::bassHistSize, head = proc.bassHistHead.load();
    int hist[DriftwerkProcessor::bassHistSize];
    int lowest = 999, highest = -999;
    for (int i = 0; i < size; ++i)
    {
        hist[i] = proc.bassHist[(size_t) ((head + i) % size)].load();
        if (hist[i] >= 0) { lowest = std::min (lowest, hist[i] & 255); highest = std::max (highest, hist[i] & 255); }
    }
    const auto roll = b.reduced (8.0f).withTrimmedTop (20.0f);
    const bool on = proc.apvts.getRawParameterValue ("bass.on")->load() > 0.5f;
    if (lowest > highest)
    {
        g.setColour (fg3);
        g.drawFittedText (on ? "waiting for the clock" : "turn ON for a bass line under the arp,\nin time and on the chords",
                          roll.toNearestInt(), juce::Justification::centred, 3);
        return;
    }
    // A two-octave window around what it played, with the key's root marked.
    const int mid = (lowest + highest) / 2;
    const int lo = highest - lowest > 22 ? lowest - 2 : mid - 13, hi = highest - lowest > 22 ? highest + 2 : mid + 13;
    const auto yOf = [&] (int note) { return roll.getBottom() - (float) (note - lo) / (float) (hi - lo) * roll.getHeight(); };
    const int keyPc = proc.keyPcNow.load();
    for (int n = lo; n <= hi; ++n)
        if (((n % 12) + 12) % 12 == keyPc)
        {
            g.setColour (rule);
            g.fillRect (roll.getX(), yOf (n), roll.getWidth(), 1.0f);
            g.setColour (fg3);
            g.setFont (dwui::font (9.0f));
            g.drawText (harm::pcName (keyPc, true) + juce::String (n / 12 - 1), (int) roll.getRight() - 26, (int) yOf (n) - 12, 26, 11,
                        juce::Justification::right);
        }
    const float colW = roll.getWidth() / (float) size;
    int newest = -1;
    for (int i = size - 1; i >= 0 && newest < 0; --i) if (hist[i] >= 0) newest = i;
    for (int i = 0; i < size; ++i)
    {
        if (hist[i] < 0) continue;
        const int note = hist[i] & 255;
        const bool acc = (hist[i] & 256) != 0, slide = (hist[i] & 512) != 0;
        const float x = roll.getX() + colW * (float) i, y = yOf (note);
        const float age = (float) i / (float) (size - 1);
        g.setColour (acid.withAlpha (0.25f + 0.55f * age + (acc ? 0.2f : 0.0f)));
        g.fillRect (x + 0.5f, y - (acc ? 3.0f : 2.0f), colW * (slide ? 1.0f : 0.8f), acc ? 6.0f : 4.0f);
        if (slide && i + 1 < size && hist[i + 1] >= 0)
        {
            g.setColour (acid.withAlpha (0.35f + 0.4f * age));
            g.drawLine (x + colW, y, x + colW, yOf (hist[i + 1] & 255), 1.0f);
        }
    }
    if (newest >= 0)
    {
        const int note = hist[newest] & 255;
        drawGlowDot (g, { roll.getX() + colW * ((float) newest + 0.4f), yOf (note) }, 1.0f, 2.6f);
        g.setColour (fg);
        g.setFont (dwui::font (11.0f));
        g.drawText (harm::pcName (note % 12, true) + juce::String (note / 12 - 1), getWidth() - 100, 5, 48, 16, juce::Justification::right);
    }
}

//==============================================================================
LayersPanel::LayersPanel (DriftwerkProcessor& p) : proc (p)
{
    for (int k = 0; k < DriftwerkProcessor::numLayers; ++k)
    {
        const auto& ld = DriftwerkProcessor::layerDefs[k];
        auto& s = levels[k];
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setPopupDisplayEnabled (true, true, this, 900);
        s.setMouseDragSensitivity (180);
        addAndMakeVisible (s);
        sAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ld.level, s));
        auto& m = mutes[k];
        m.setButtonText ("M");
        m.getProperties().set ("small", true);
        m.setClickingTogglesState (true);
        m.setTooltip (juce::String ("Mute ") + juce::String (ld.name).toLowerCase() + " (it keeps playing, you just don't hear it)");
        addAndMakeVisible (m);
        bAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ld.mute, m));
    }
}

void LayersPanel::resized()
{
    for (int k = 0; k < DriftwerkProcessor::numLayers; ++k)
    {
        const int y = k * rowH;
        levels[k].setBounds (meterX + 10, y + 2, getWidth() - meterX - 10 - 36, rowH - 4);
        mutes[k].setBounds (getWidth() - 30, y + 3, 28, rowH - 6);
    }
}

void LayersPanel::refresh()
{
    for (int k = 0; k < DriftwerkProcessor::numLayers; ++k)
    {
        const bool muted = mutes[k].getToggleState();
        if (muted != shownMuted[k])
        {
            shownMuted[k] = muted;
            levels[k].getProperties().set ("dim", muted);
            levels[k].repaint();
            repaint (0, k * rowH, meterX + 10, rowH);
        }
        const float m = juce::jlimit (0.0f, 1.0f, std::sqrt (proc.layerMeter[(size_t) k].load()) * 1.6f);
        const float g = std::max (m, glow[k] * 0.82f);
        if (std::abs (g - glow[k]) > 0.02f || (g < 0.02f && glow[k] > 0.0f))
        {
            glow[k] = g < 0.02f ? 0.0f : g;
            repaint (meterX - 10, k * rowH, 20, rowH);
        }
    }
}

void LayersPanel::paint (juce::Graphics& g)
{
    using namespace dwui;
    g.setFont (dwui::font (10.5f).withExtraKerningFactor (0.06f));
    for (int k = 0; k < DriftwerkProcessor::numLayers; ++k)
    {
        const int y = k * rowH;
        if (k > 0) { g.setColour (rule.withMultipliedAlpha (0.5f)); g.fillRect (0, y, getWidth(), 1); }
        g.setColour (shownMuted[k] ? fg3 : fg2);
        g.drawText (DriftwerkProcessor::layerDefs[k].name, 2, y, nameW, rowH, juce::Justification::centredLeft);
        drawGlowDot (g, { (float) meterX, (float) y + rowH * 0.5f }, shownMuted[k] ? 0.0f : glow[k], 2.4f);
    }
}

//==============================================================================
PerformPanel::PerformPanel (DriftwerkProcessor& p, MainView& v) : proc (p), view (v)
{
    const char* tabNames[] { "SEQUENCE", "ARP SOUND", "DRONE", "CHORDS", "HARMONY", "LOOPS", "PULSE", "TEXTURE", "MEMORY", "ORGANISM", "WORDS" };
    static_assert (sizeof (tabNames) / sizeof (tabNames[0]) == NumTabs);
    for (int t = 0; t < NumTabs; ++t)
    {
        tabButtons[t].setButtonText (tabNames[t]);
        tabButtons[t].getProperties().set ("small", true);
        tabButtons[t].setBounds (6 + t * 63, 6, 60, 22);
        tabButtons[t].onClick = [this, t] { setTab (t); };
        addAndMakeVisible (tabButtons[t]);
    }

    // ---- SEQUENCE tab
    int x = 14;
    auto place = [&] (juce::TextButton& b, int w) { b.setBounds (x, 40, w, 24); x += w + 6; addAndMakeVisible (b); tabbed.push_back ({ &b, Sequence }); };
    place (holdButton, 58);
    holdButton.setClickingTogglesState (true);
    bAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "arp.hold", holdButton));
    place (recButton, 84);
    recButton.getProperties().set ("accent", true);
    recButton.onClick = [this] { proc.setRecordNotes (! proc.recordNotes.load()); };
    place (newButton, 46);
    newButton.onClick = [this] { proc.newRandomSequence(); proc.setStep (0, proc.stepNote[0].load(), true); lastSlot = -1; };
    place (clearButton, 54);
    clearButton.onClick = [this] { for (int i = 0; i < DriftwerkProcessor::numSteps; ++i) proc.setStep (i, 0, i == 0); lastSlot = -1; };
    place (leftButton, 26);
    place (rightButton, 26);
    auto shift = [this] (int dir)
    {
        const int len = proc.arpLength.load();
        std::vector<std::pair<int, bool>> s ((size_t) len);
        for (int i = 0; i < len; ++i) s[(size_t) i] = { proc.stepNote[(size_t) i].load(), proc.stepOn[(size_t) i].load() };
        for (int i = 0; i < len; ++i) { const auto& src = s[(size_t) ((i - dir + len) % len)]; proc.setStep (i, src.first, src.second); }
    };
    leftButton.onClick = [shift] { shift (-1); };
    rightButton.onClick = [shift] { shift (1); };
    x += 14;
    for (int i = 0; i < DriftwerkProcessor::numSlots; ++i)
    {
        slotButtons[i].setButtonText (juce::String (i + 1));
        place (slotButtons[i], 28);
        slotButtons[i].onClick = [this, i]
        {
            if (saveButton.getToggleState()) { proc.storeSlot (i); saveButton.setToggleState (false, juce::dontSendNotification); lastSlot = i; }
            else if (proc.recallSlot (i)) lastSlot = i;
        };
    }
    place (saveButton, 52);
    saveButton.setClickingTogglesState (true);

    // ---- ARP SOUND tab: the usual arp voice (VCO > VCF 1 > VCA 1), all in one place
    {
        const std::pair<const char*, const char*> k[] { { "arp.oct", "OCTAVE" }, { "arp.glide", "GLIDE" }, { "env1.a", "SOFT" },
            { "env1.d", "PLUCK" }, { "vcf1.cut", "BRIGHT" }, { "vcf1.res", "RESO" }, { "vcf1.drive", "DRIVE" },
            { "vco2.fine", "DETUNE" }, { "echo.mix", "ECHO" }, { "out.lvl1", "LEVEL" } };
        for (int i = 0; i < 10; ++i)
        {
            const int kx = 346 + (i % 5) * 68, ky = 46 + (i / 5) * 104;
            knob (k[i].first, k[i].second, ArpSound, kx, ky);
            labs.back().r = { kx + 15, ky + 54, 64, 14 };
        }
        // WAVE: the arp's waveform, drawn in the box, with SHAPE, FOLD and MOVE under it
        combo ("arp.wave", "WAVE", ArpSound, 16, 46, 110);
        combo ("arp.table", "WAVETABLE", ArpSound, 16, 104, 110);
        const std::pair<const char*, const char*> w[] { { "arp.shape", "SHAPE" }, { "arp.fold", "FOLD" }, { "arp.move", "MOVE" } };
        for (int i = 0; i < 3; ++i)
        {
            const int kx = 124 + i * 70, ky = 166;
            knob (w[i].first, w[i].second, ArpSound, kx, ky);
            labs.back().r = { kx + 15, ky + 54, 64, 14 };
        }
    }
    // ---- DRONE tab
    {
        combo ("drn.type", "TYPE", Drone, 30, 48, 92);
        combo ("drn.chord", "CHORD", Drone, 30, 104, 92);
        const std::pair<const char*, const char*> k[] { { "drn.oct", "OCTAVE" }, { "drn.detune", "DETUNE" }, { "drn.drift", "DRIFT" },
            { "drn.level", "LEVEL" }, { "drn.tone", "TONE" }, { "drn.wave", "COLOUR" }, { "drn.breath", "BREATH" },
            { "drn.air", "AIR" }, { "drn.sub", "SUB" } };
        for (int i = 0; i < 9; ++i) { const int c = i + 1; knob (k[i].first, k[i].second, Drone, 30 + (c % 5) * 132, 62 + (c / 5) * 104); }
    }
    // ---- CHORDS tab: the sound along the top, the HITS pattern (style STEPS) below
    {
        chordsOnButton.setBounds (16, 46, 100, 24);
        chordsOnButton.setClickingTogglesState (true);
        chordsOnButton.getProperties().set ("accent", true);
        addAndMakeVisible (chordsOnButton);
        bAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "chd.on", chordsOnButton));
        tabbed.push_back ({ &chordsOnButton, Chords });
        combo ("chd.mode", "STYLE", Chords, 16, 92, 100);
        const std::pair<const char*, const char*> k[] { { "chd.level", "LEVEL" }, { "chd.voices", "VOICES" }, { "chd.oct", "OCTAVE" },
            { "chd.wave", "COLOUR" }, { "chd.tone", "TONE" }, { "chd.attack", "ATTACK" }, { "chd.release", "RELEASE" }, { "chd.width", "WIDTH" } };
        for (int i = 0; i < 8; ++i) knob (k[i].first, k[i].second, Chords, 124 + i * 70, 44);
        combo ("chd.rate", "STEP", Chords, 130, 206, 70);
        knob ("chd.steps", "STEPS", Chords, 206, 198);
        knob ("chd.density", "DENSITY", Chords, 272, 198);
        knob ("chd.gate", "GATE", Chords, 338, 198);
        knob ("chd.vary", "VARY", Chords, 404, 198);
        hitsNewButton.setBounds (506, 208, 56, 24);
        hitsNewButton.onClick = [this]
        {
            proc.newHits();
            proc.setParam ("chd.mode", 5.0f);
            proc.setParam ("chd.on", 1.0f);
            repaint();
        };
        addAndMakeVisible (hitsNewButton);
        tabbed.push_back ({ &hitsNewButton, Chords });
    }
    // ---- HARMONY tab: settings on the left, the chord wheel in the middle, the chords of the loop on the right
    combo ("prog.mode", "PROGRESSION", Harmony, 16, 44, 118);
    combo ("prog.colour", "CHORD COLOUR", Harmony, 16, 94, 118);
    combo ("prog.bars", "CHANGE EVERY", Harmony, 16, 144, 118);
    combo ("arp.scale", "SCALE", Harmony, 146, 44, 100);
    combo ("arp.follow", "ARP FOLLOWS", Harmony, 146, 94, 100);
    combo ("scene.key", "NEW SCENE KEY", Harmony, 146, 144, 100);
    knob ("arp.root", "KEY", Harmony, 28, 192);
    knob ("prog.glide", "CHORD GLIDE", Harmony, 149, 192);
    editChordsButton.setBounds (498, 168, 92, 24);
    newChordsButton.setBounds (596, 168, 92, 24);
    newChordsButton.getProperties().set ("accent", true);
    editChordsButton.onClick = [this] { proc.copyProgressionToMine(); repaint(); };
    newChordsButton.onClick = [this] { proc.randomiseMyChords(); repaint(); };
    for (auto* b : { &editChordsButton, &newChordsButton }) { addAndMakeVisible (*b); tabbed.push_back ({ b, Harmony }); }
    combo ("prog.len", "LENGTH", Harmony, 498, 210, 88);
    combo ("prog.rhythm", "RHYTHM", Harmony, 600, 210, 88);

    // ---- LOOPS tab: the orbits on the left, the loop settings on the right
    onOff (loopsOnButton, "loop.on", Loops, { 262, 44, 96, 24 });
    combo ("loop.notes", "NOTES", Loops, 366, 44, 90);
    shuffleButton.setBounds (464, 44, 90, 24);
    shuffleButton.onClick = [this] { proc.loopShuffleReq.fetch_add (1); proc.setParam ("loop.on", 1.0f); };
    addAndMakeVisible (shuffleButton);
    tabbed.push_back ({ &shuffleButton, Loops });
    {
        const std::pair<const char*, const char*> k[] { { "loop.level", "LEVEL" }, { "loop.length", "LENGTH" }, { "loop.voices", "LOOPS" },
            { "loop.oct", "OCTAVE" }, { "loop.swell", "SWELL" }, { "loop.decay", "DECAY" }, { "loop.colour", "COLOUR" },
            { "loop.tone", "TONE" }, { "loop.drift", "WOBBLE" }, { "loop.weird", "WEIRD" } };
        for (int i = 0; i < 10; ++i) knob (k[i].first, k[i].second, Loops, 256 + (i % 5) * 86, 96 + (i / 5) * 82);
    }
    // ---- PULSE tab: settings on the left; two pages: the PATTERN (five lanes and the groove
    // knobs) and the SOUND of each drum (level, pitch, decay) with the kit's punch and tone.
    onOff (pulseOnButton, "pulse.on", Pulse, { 16, 40, 100, 24 });
    combo ("pulse.style", "STYLE", Pulse, 16, 78, 100);
    combo ("pulse.kit", "KIT", Pulse, 16, 128, 100);
    combo ("pulse.speed", "STEP", Pulse, 16, 178, 100);
    combo ("pulse.length", "LENGTH", Pulse, 16, 228, 100);
    pulsePatternButton.setBounds (132, 40, 76, 22);
    pulseSoundButton.setBounds (212, 40, 76, 22);
    pulseBreakButton.setBounds (292, 40, 76, 22);
    for (auto* b : { &pulsePatternButton, &pulseSoundButton, &pulseBreakButton })
    {
        b->getProperties().set ("small", true);
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
        tabbed.push_back ({ b, Pulse });
    }
    pulsePatternButton.onClick = [this] { setPulsePage (0); };
    pulseSoundButton.onClick = [this] { setPulsePage (1); };
    pulseBreakButton.onClick = [this] { setPulsePage (2); };
    pulseNewButton.setBounds (612, 40, 76, 22);
    pulseNewButton.onClick = [this] { proc.pulseNewReq.fetch_add (1); proc.setParam ("pulse.on", 1.0f); };
    addAndMakeVisible (pulseNewButton);
    tabbed.push_back ({ &pulseNewButton, Pulse });
    {
        const std::pair<const char*, const char*> k[] { { "pulse.level", "LEVEL" }, { "pulse.density", "DENSITY" }, { "pulse.swing", "SWING" },
            { "pulse.human", "HUMAN" }, { "pulse.vary", "VARY" }, { "pulse.fill", "FILL" }, { "pulse.stutter", "STUTTER" },
            { "pulse.chop", "CHOP" }, { "pulse.space", "SPACE" }, { "pulse.weird", "WEIRD" } };
        for (int i = 0; i < 10; ++i)
        {
            knob (k[i].first, k[i].second, PulsePatternPage, 108 + i * 56, 180);
            labs.back().r = { 108 + i * 56 + 15, 234, 64, 14 };
        }
        // SOUND: a column for each drum (LEVEL, PITCH, DECAY), the whole kit on the right
        for (int lane = 0; lane < dw::Pulse::Lanes; ++lane)
            for (int row = 0; row < 3; ++row)
            {
                const juce::String id = juce::String ("pulse.") + "lpd"[row] + juce::String (lane + 1);
                knob (id.toRawUTF8(), "", PulseSoundPage, 0, 0);
                labs.pop_back();
                owned.back()->setBounds (soundCol (lane) - 22, soundRowY[row], 44, 44);
            }
        const std::pair<const char*, const char*> kk[] { { "pulse.level", "LEVEL" }, { "pulse.punch", "PUNCH" }, { "pulse.tone", "TONE" } };
        for (int row = 0; row < 3; ++row)
        {
            knob (kk[row].first, kk[row].second, PulseSoundPage, 0, 0);
            owned.back()->setBounds (632, soundRowY[row], 44, 44);
            labs.back().r = { 566, soundRowY[row] + 15, 60, 14 };
        }
    }
    // SOUND: LOAD under each drum puts your own sound in its place (or drop a file on the column)
    for (int lane = 0; lane < dw::Pulse::Lanes; ++lane)
    {
        auto& b = laneLoadButtons[lane];
        b.setButtonText ("LOAD");
        b.getProperties().set ("small", true);
        b.setTooltip ("Use your own sound for this drum (or drop a file on it)");
        b.setBounds (soundCol (lane) - 30, 236, 60, 18);
        b.onClick = [this, lane] { showLaneMenu (lane); };
        addAndMakeVisible (b);
        tabbed.push_back ({ &b, PulseSoundPage });
    }
    // BREAK: a drum loop cut into sixteenths and chopped on the beat
    onOff (breakOnButton, "brk.on", PulseBreakPage, { 132, 72, 96, 24 });
    combo ("brk.src", "SOURCE", PulseBreakPage, 238, 72, 118);
    breakLoadButton.setBounds (364, 72, 104, 24);
    breakLoadButton.setTooltip ("Load a drum loop (a break). Timewind finds its tempo and slices it on the hits. Or drop one here.");
    breakLoadButton.onClick = [this] { chooseBreak(); };
    addAndMakeVisible (breakLoadButton);
    tabbed.push_back ({ &breakLoadButton, PulseBreakPage });
    combo ("brk.bars", "BARS", PulseBreakPage, 478, 72, 92);
    onOff (breakFitButton, "brk.fit", PulseBreakPage, { 580, 72, 108, 24 });
    breakFitButton.setTooltip ("On: the loop speeds up or slows down to Timewind's tempo, pitch and all (like an old sampler). Off: slices keep their own speed.");
    {
        const std::pair<const char*, const char*> k[] { { "brk.level", "LEVEL" }, { "brk.chop", "CHOP" }, { "brk.stutter", "STUTTER" },
            { "brk.reverse", "REVERSE" }, { "brk.pitch", "PITCH" }, { "brk.dust", "DUST" } };
        for (int i = 0; i < 6; ++i)
        {
            knob (k[i].first, k[i].second, PulseBreakPage, 116 + i * 64, 180);
            labs.back().r = { 116 + i * 64 + 15, 234, 64, 14 };
        }
    }
    onOff (breakDrumsButton, "brk.drums", PulseBreakPage, { 532, 196, 110, 24 });
    breakDrumsButton.setTooltip ("Off: the break plays alone. On: the PULSE drums play along with it.");
    setPulsePage (0);
    // ---- WORDS tab: your recordings (or the synthetic voice), half heard
    onOff (wordsOnButton, "words.on", Words, { 16, 40, 110, 24 });
    combo ("words.src", "SOURCE", Words, 16, 80, 110);
    wordsLoadButton.setBounds (16, 134, 110, 24);
    wordsLoadButton.onClick = [this]
    {
        wordsChooser = std::make_unique<juce::FileChooser> ("Load spoken words (pick one or more recordings)", juce::File(),
                                                            "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        juce::Component::SafePointer<PerformPanel> safe (this);
        wordsChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                       | juce::FileBrowserComponent::canSelectMultipleItems,
                                   [safe] (const juce::FileChooser& fc)
                                   {
                                       if (safe == nullptr) return;
                                       juce::StringArray paths;
                                       for (const auto& f : fc.getResults()) paths.add (f.getFullPathName());
                                       if (paths.isEmpty()) return;
                                       safe->proc.loadWords (paths);
                                       safe->proc.setParam ("words.src", 0.0f);
                                       safe->proc.setParam ("words.on", 1.0f);
                                   });
    };
    addAndMakeVisible (wordsLoadButton);
    tabbed.push_back ({ &wordsLoadButton, Words });
    wordsBeatButton.setBounds (16, 168, 110, 24);
    wordsBeatButton.setClickingTogglesState (true);
    addAndMakeVisible (wordsBeatButton);
    bAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "words.beat", wordsBeatButton));
    tabbed.push_back ({ &wordsBeatButton, Words });
    {
        const std::pair<const char*, const char*> k[] { { "words.level", "LEVEL" }, { "words.often", "OFTEN" }, { "words.phrases", "PHRASES" },
            { "words.speed", "SPEED" }, { "words.wobble", "WOBBLE" }, { "words.radio", "RADIO" }, { "words.reverse", "REVERSE" },
            { "words.repeat", "REPEAT" }, { "words.space", "SPACE" } };
        for (int i = 0; i < 9; ++i)
        {
            const int kx = 124 + i * 61, ky = 160;
            knob (k[i].first, k[i].second, Words, kx, ky);
            labs.back().r = { kx + 15, ky + 54, 64, 14 };
        }
    }

    // ---- TEXTURE tab
    onOff (texOnButton, "tex.on", Texture, { 16, 46, 100, 24 });
    combo ("tex.a", "TEXTURE A", Texture, 16, 92, 100);
    combo ("tex.b", "TEXTURE B", Texture, 16, 142, 100);
    {
        const std::pair<const char*, const char*> k[] { { "tex.level", "LEVEL" }, { "tex.blend", "A / B" }, { "tex.density", "DENSITY" },
            { "tex.tone", "TONE" }, { "tex.motion", "MOTION" }, { "tex.key", "IN KEY" }, { "tex.space", "SPACE" } };
        for (int i = 0; i < 7; ++i) knob (k[i].first, k[i].second, Texture, 130 + i * 78, 176);
    }
    // ---- MEMORY tab: the last three minutes along the top, settings below
    onOff (memOnButton, "mem.on", Memory, { 16, 150, 100, 24 });
    combo ("mem.speed", "SPEED", Memory, 16, 196, 100);
    forgetButton.setBounds (16, 246, 100, 22);
    forgetButton.onClick = [this] { proc.memForgetReq.fetch_add (1); };
    addAndMakeVisible (forgetButton);
    tabbed.push_back ({ &forgetButton, Memory });
    {
        const std::pair<const char*, const char*> k[] { { "mem.level", "LEVEL" }, { "mem.ago", "HOW LONG AGO" }, { "mem.length", "FRAGMENT" },
            { "mem.scatter", "SCATTER" }, { "mem.blur", "BLUR" }, { "mem.dissolve", "DISSOLVE" } };
        for (int i = 0; i < 6; ++i) knob (k[i].first, k[i].second, Memory, 140 + i * 90, 150);
    }
    // ---- ORGANISM tab: the slime shows through; its ears and their settings on the right
    onOff (slimeOnButton, "slime.play", Organism, { 414, 44, 100, 24 });
    combo ("slime.voice", "VOICE", Organism, 522, 44, 84);
    combo ("slime.style", "PLAYS AS", Organism, 612, 44, 80);
    {
        const std::pair<const char*, const char*> k[] { { "slime.level", "LEVEL" }, { "slime.activity", "ACTIVITY" }, { "slime.weird", "WEIRD" },
            { "slime.ears", "EARS" }, { "slime.growth", "GROWTH" }, { "slime.range", "RANGE" }, { "slime.oct", "OCTAVE" }, { "slime.tone", "TONE" } };
        for (int i = 0; i < 8; ++i) knob (k[i].first, k[i].second, Organism, 408 + (i % 4) * 70, 94 + (i / 4) * 76);
    }

    setTab (Sequence);
}

void PerformPanel::onOff (juce::TextButton& b, const char* param, int tabId, juce::Rectangle<int> r)
{
    b.setBounds (r);
    b.setClickingTogglesState (true);
    b.getProperties().set ("accent", true);
    addAndMakeVisible (b);
    bAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, param, b));
    tabbed.push_back ({ &b, tabId });
}

void PerformPanel::knob (const char* param, const char* label, int tabId, int x, int y)
{
    auto s = std::make_unique<ModSlider>();
    s->paramIndex = proc.paramIndex (param);
    auto* raw = s.get();
    s->onRightClick = [this, raw] { view.showModMenu (*raw); };
    s->setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s->setPopupDisplayEnabled (true, true, &view, 900);
    s->setBounds (x + 22, y, 50, 50);
    s->setMouseDragSensitivity (220);
    addAndMakeVisible (*s);
    sAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, param, *s));
    tabbed.push_back ({ s.get(), tabId });
    labs.push_back ({ { x - 6, y + 54, 106, 14 }, label, tabId });
    owned.push_back (std::move (s));
}

void PerformPanel::combo (const char* param, const char* label, int tabId, int x, int y, int w)
{
    auto cb = std::make_unique<juce::ComboBox>();
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (param)))
        cb->addItemList (pc->choices, 1);
    cb->setBounds (x, y, w, 24);
    addAndMakeVisible (*cb);
    cAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, param, *cb));
    tabbed.push_back ({ cb.get(), tabId });
    labs.push_back ({ { x - 10, y + 30, w + 20, 14 }, label, tabId });
    owned.push_back (std::move (cb));
}

void PerformPanel::setTab (int t)
{
    tab = t;
    for (int i = 0; i < NumTabs; ++i) tabButtons[i].setToggleState (i == t, juce::dontSendNotification);
    for (auto& [c, ct] : tabbed) c->setVisible (shows (ct));
    repaint();
}

void PerformPanel::showLaneMenu (int lane)
{
    const int kit = dw::Pulse::clampKit ((int) std::lround (proc.apvts.getRawParameterValue ("pulse.kit")->load()));
    juce::PopupMenu m;
    m.addItem (1, "Load a sound for " + juce::String (dw::Pulse::laneName (kit, lane)) + "...");
    if (proc.pulseLaneHasSample (lane))
        m.addItem (2, "Back to the kit's " + juce::String (dw::Pulse::laneName (kit, lane)).toLowerCase());
    juce::Component::SafePointer<PerformPanel> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&laneLoadButtons[lane]),
                     [safe, lane] (int r)
                     {
                         if (safe == nullptr) return;
                         if (r == 1) safe->chooseLaneSample (lane);
                         if (r == 2) { safe->proc.clearPulseSample (lane); safe->repaint(); }
                     });
}

void PerformPanel::chooseLaneSample (int lane)
{
    sampleChooser = std::make_unique<juce::FileChooser> ("Your own drum sound", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<PerformPanel> safe (this);
    sampleChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, lane] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            const auto f = fc.getResult();
            if (f.existsAsFile()) safe->proc.loadPulseSample (lane, f);
            safe->repaint();
        });
}

void PerformPanel::chooseBreak()
{
    sampleChooser = std::make_unique<juce::FileChooser> ("A drum loop to slice", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<PerformPanel> safe (this);
    sampleChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            const auto f = fc.getResult();
            if (f.existsAsFile() && safe->proc.loadBreak (f)) safe->proc.setParam ("brk.on", 1.0f);
            safe->repaint();
        });
}

bool PerformPanel::fileDropped (const juce::String& path, juce::Point<int> where)
{
    if (tab != Pulse || ! getLocalBounds().contains (where)) return false;
    const juce::File f (path);
    if (pulsePage == 2)
    {
        if (proc.loadBreak (f)) proc.setParam ("brk.on", 1.0f);
        repaint();
        return true;
    }
    if (pulsePage == 1 && where.x >= soundCol (0) - 37 && where.x < soundCol (dw::Pulse::Lanes - 1) + 37)
    {
        const int lane = juce::jlimit (0, dw::Pulse::Lanes - 1, (where.x - (soundCol (0) - 37)) / 74);
        proc.loadPulseSample (lane, f);
        repaint();
        return true;
    }
    return false;
}

void PerformPanel::paintBreak (juce::Graphics& g)
{
    using namespace dwui;
    const auto box = breakBox().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (box);
    g.setColour (rule);
    g.drawRect (box, 1.0f);
    const auto* bl = proc.breakForDisplay();
    const bool on = proc.apvts.getRawParameterValue ("brk.on")->load() > 0.5f
                    && proc.apvts.getRawParameterValue ("pulse.on")->load() > 0.5f;
    const bool wantsMine = (int) std::lround (proc.apvts.getRawParameterValue ("brk.src")->load()) == 1;
    juce::String info;
    if (bl != nullptr && bl->steps > 0)
    {
        const auto inner = box.reduced (2.0f, 14.0f);
        const int np = (int) bl->peaks.size();
        const float cw = inner.getWidth() / (float) np, mid = inner.getCentreY();
        // the slice playing now
        const int now = proc.breakSliceNow.load();
        const int len = juce::jmax (1, bl->audio->length());
        auto xAt = [&] (int sample) { return inner.getX() + inner.getWidth() * (float) sample / (float) len; };
        if (on && now >= 0 && now < bl->steps)
        {
            g.setColour (acid.withAlpha (0.16f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xAt (bl->slice[(size_t) now]), box.getY() + 1.0f,
                                                                    xAt (bl->slice[(size_t) now + 1]), box.getBottom() - 1.0f));
        }
        g.setColour (fg2.withAlpha (0.75f));
        for (int i = 0; i < np; ++i)
        {
            const float h = juce::jmin (1.0f, bl->peaks[(size_t) i]) * inner.getHeight() * 0.5f;
            g.fillRect (inner.getX() + cw * (float) i, mid - h, juce::jmax (1.0f, cw - 0.4f), h * 2.0f);
        }
        // slice lines: brighter on the beats, brightest on the bars
        for (int k = 0; k < bl->steps; ++k)
        {
            const float x = xAt (bl->slice[(size_t) k]);
            g.setColour (k % 16 == 0 ? acid.withAlpha (0.8f) : (k % 4 == 0 ? fg.withAlpha (0.4f) : fg3.withAlpha (0.25f)));
            g.drawVerticalLine ((int) x, box.getY() + (k % 4 == 0 ? 1.0f : 10.0f), box.getBottom() - (k % 4 == 0 ? 1.0f : 10.0f));
        }
        if (on)
        {
            g.setColour (acid);
            g.drawVerticalLine ((int) (inner.getX() + inner.getWidth() * juce::jlimit (0.0f, 1.0f, proc.breakPosNow.load())),
                                box.getY() + 1.0f, box.getBottom() - 1.0f);
        }
        info << (bl->builtIn ? juce::String ("synth break") : juce::String (bl->name))
             << "  " << juce::String (bl->bpm, 1) << " BPM, " << bl->bars << (bl->bars == 1 ? " bar, " : " bars, ") << bl->steps << " slices";
        if (wantsMine && bl->builtIn) info = "no loop yet: LOAD BREAK or drop one here (playing the synth break)";
    }
    g.setColour (fg2);
    g.setFont (dwui::font (10.5f));
    g.drawFittedText (info, breakBox().reduced (6, 2), juce::Justification::topLeft, 1, 0.8f);
    if (! on)
    {
        g.setColour (fg3);
        g.drawText (proc.apvts.getRawParameterValue ("pulse.on")->load() > 0.5f ? "BREAK is off" : "PULSE is off",
                    breakBox().reduced (6, 2), juce::Justification::bottomRight);
    }
}

void PerformPanel::setPulsePage (int pg)
{
    pulsePage = juce::jlimit (0, 2, pg);
    pulsePatternButton.setToggleState (pulsePage == 0, juce::dontSendNotification);
    pulseSoundButton.setToggleState (pulsePage == 1, juce::dontSendNotification);
    pulseBreakButton.setToggleState (pulsePage == 2, juce::dontSendNotification);
    setTab (tab);
}

void PerformPanel::refresh()
{
    recButton.setToggleState (proc.recordNotes.load(), juce::dontSendNotification);
    for (int i = 0; i < DriftwerkProcessor::numSlots; ++i)
    {
        slotButtons[i].getProperties().set ("accent", proc.slotUsed (i));
        slotButtons[i].setToggleState (i == lastSlot, juce::dontSendNotification);
    }
    if (tab == Harmony) animateWheel();
    animate();
    if (tab != ArpSound && tab != Drone) repaint();
    else if (tab == ArpSound) repaint (waveBox().expanded (4));
}

int PerformPanel::colAt (int x) const
{
    const auto g = grid();
    return juce::jlimit (0, DriftwerkProcessor::numSteps - 1, (x - g.getX()) * DriftwerkProcessor::numSteps / g.getWidth());
}

int PerformPanel::degAt (int y) const
{
    const auto g = grid().withTrimmedTop (16).withTrimmedBottom (16);
    const int maxDeg = juce::jmax (1, proc.maxDegree());
    return juce::jlimit (0, maxDeg, juce::roundToInt ((float) (g.getBottom() - y) / (float) g.getHeight() * (float) maxDeg));
}

int PerformPanel::hitAt (int x) const
{
    const auto g = hitGrid();
    return juce::jlimit (0, DriftwerkProcessor::numHitSteps - 1, (x - g.getX()) * DriftwerkProcessor::numHitSteps / g.getWidth());
}

void PerformPanel::mouseDown (const juce::MouseEvent& e)
{
    dragCol = -1;
    hitPaintCol = -1;
    pulsePaintLane = -1;
    if (tab == Pulse && pulsePage == 0 && pulseGrid().contains (e.getPosition()))
    {
        // Click a step to force it on or off; drag along the lane to paint more.
        const auto gr = pulseGrid();
        const int lane = juce::jlimit (0, dw::Pulse::Lanes - 1, (e.y - gr.getY()) * dw::Pulse::Lanes / gr.getHeight());
        const int step = juce::jlimit (0, dw::Pulse::Steps - 1, (e.x - gr.getX()) * dw::Pulse::Steps / gr.getWidth());
        pulsePaintOn = ! proc.pulseOnNow (lane, step);
        pulsePaintLane = lane; pulsePaintStep = step;
        proc.setPulse (lane, step, pulsePaintOn);
        proc.setParam ("pulse.on", 1.0f);
        repaint();
        return;
    }
    if (tab == Chords && hitGrid().contains (e.getPosition()))
    {
        // Click a step to force it on or off; drag to paint more. This plays the pattern (STEPS).
        const int i = hitAt (e.x);
        const int steps = (int) std::lround (proc.apvts.getRawParameterValue ("chd.steps")->load());
        if (i >= steps) return;
        hitPaintOn = ! proc.hitOnNow (i);
        hitPaintCol = i;
        proc.setHit (i, hitPaintOn);
        proc.setParam ("chd.mode", 5.0f);
        proc.setParam ("chd.on", 1.0f);
        repaint();
        return;
    }
    if (tab == Harmony)
    {
        const auto v = proc.getProgressionView();
        for (int k = 0; k < harm::maxSlots; ++k)
            if (chordBox (k).contains (e.getPosition()))
            {
                if (k < v.count) showChordMenu (k);
                else if (v.mode == harm::progMine && k == v.count) { proc.setMyChord (k, harm::defaultDegrees[k], 0, 0); showChordMenu (k); }
                return;
            }
        return;
    }
    if (tab != Sequence || ! grid().contains (e.getPosition())) return;
    dragCol = colAt (e.x); dragStartY = e.y; dragMoved = false;
}

void PerformPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (pulsePaintLane >= 0)
    {
        const auto gr = pulseGrid();
        const int step = juce::jlimit (0, dw::Pulse::Steps - 1, (e.x - gr.getX()) * dw::Pulse::Steps / gr.getWidth());
        if (step != pulsePaintStep) { pulsePaintStep = step; proc.setPulse (pulsePaintLane, step, pulsePaintOn); repaint(); }
        return;
    }
    if (hitPaintCol >= 0)
    {
        const int i = hitAt (e.x);
        const int steps = (int) std::lround (proc.apvts.getRawParameterValue ("chd.steps")->load());
        if (i != hitPaintCol && i < steps) { hitPaintCol = i; proc.setHit (i, hitPaintOn); repaint(); }
        return;
    }
    if (dragCol < 0) return;
    if (! dragMoved && std::abs (e.y - dragStartY) < 4 && colAt (e.x) == dragCol) return;
    dragMoved = true;
    proc.setStep (colAt (e.x), degAt (e.y), true);   // drag to draw notes, across steps too
    lastSlot = -1;
    repaint();
}

void PerformPanel::mouseUp (const juce::MouseEvent& e)
{
    if (dragCol >= 0 && ! dragMoved && grid().contains (e.getPosition()))
    {
        const int c = dragCol;
        proc.setStep (c, proc.stepNote[(size_t) c].load(), ! proc.stepOn[(size_t) c].load());
        lastSlot = -1;
        repaint();
    }
    dragCol = -1;
}

void PerformPanel::paint (juce::Graphics& g)
{
    using namespace dwui;
    if (tab == Organism) { paintOrganism (g); return; }      // the slime shows through
    g.setColour (juce::Colours::black.withAlpha (0.74f));
    g.fillRect (getLocalBounds());
    g.setColour (juce::Colours::black.withAlpha (0.9f));           // keep the slime's own captions out of the way
    g.fillRect (getLocalBounds().removeFromTop (38));
    g.fillRect (getLocalBounds().removeFromBottom (28));
    g.setColour (rule);
    g.drawRect (getLocalBounds(), 1);

    const char* subs[] { "HOLD keeps the pattern. Click a step on/off, drag to set its note, REC NOTES to play it in.",
                         "WAVE picks the arp's waveform (PATCH plays the VCO jacks as patched). SHAPE bends it, FOLD folds it, MOVE lets it breathe.",
                         "TYPE: saws, organ, strings, glass or choir. COLOUR shapes each type (brightness, drawbars, width, shimmer, vowel).",
                         "plays the HARMONY chord, in tune with everything. STEPS plays the HITS pattern: DENSITY fills it, NEW reshuffles, VARY changes it each loop.",
                         "circle of fifths in your key: lit notes are the chord now, the arrow points to the next one. Click a chord on the right to change it.",
                         "loops of different lengths drift in and out of phase, each ringing a note of the chord. WEIRD reverses, bends and ghosts them.",
                         "drums: pick a STYLE and a KIT. DENSITY fills the pattern, NEW reshuffles, click steps to force them. SOUND shapes each drum.",
                         "weather and creatures, made from nothing. IN KEY tunes drops, bubbles, whistles and birds to the chord.",
                         "Timewind drifts back into what it played: slower, reversed, blurred. DISSOLVE lets memories fade into memories.",
                         "",
                         "LOAD WORDS: any speech recordings. Timewind cuts them into words and phrases and lets them drift in, like an old tape." };
    static_assert (sizeof (subs) / sizeof (subs[0]) == NumTabs);
    g.setColour (fg3);
    g.setFont (dwui::font (10.5f));
    const char* sub = subs[tab];
    if (tab == Pulse && pulsePage == 2)
        sub = "BREAK cuts a drum loop into sixteenths and plays them on the beat. CHOP reorders them, STUTTER rolls, DUST makes it older.";
    if (tab == Pulse && pulsePage == 1)
        sub = "LOAD under a drum (or drop a file on it) to play your own kick, snare, hat... PITCH, DECAY and the rolls still apply.";
    g.drawFittedText (sub, 14, getHeight() - 22, getWidth() - 28, 14, juce::Justification::left, 1, 0.8f);

    g.setFont (dwui::font (10.0f).withExtraKerningFactor (0.08f));
    for (auto& l : labs)
        if (shows (l.tab)) { g.setColour (fg2); g.drawText (l.t, l.r, juce::Justification::centred); }

    if (tab == ArpSound)
        paintWave (g);
    if (tab == Words)
        paintWords (g);
    if (tab == Sequence)
    {
        const auto gr = grid();
        const int n = DriftwerkProcessor::numSteps, len = proc.arpLength.load(), cur = proc.arpStep.load();
        const int maxDeg = juce::jmax (1, proc.maxDegree());
        const float cw = (float) gr.getWidth() / (float) n;
        const auto inner = gr.withTrimmedTop (16).withTrimmedBottom (16).toFloat();
        const bool held = proc.apvts.getRawParameterValue ("arp.hold")->load() > 0.5f || proc.recordNotes.load();
        for (int i = 0; i < n; ++i)
        {
            const auto col = juce::Rectangle<float> (gr.getX() + cw * (float) i, (float) gr.getY(), cw, (float) gr.getHeight()).reduced (3.0f, 0.0f);
            const bool inLen = i < len, on = proc.stepOn[(size_t) i].load();
            const int deg = proc.stepNote[(size_t) i].load();
            const float y = inner.getBottom() - inner.getHeight() * (float) juce::jmin (deg, maxDeg) / (float) maxDeg;
            g.setColour (juce::Colours::white.withAlpha (inLen ? 0.05f : 0.015f));
            g.fillRect (col);
            if (i == cur && inLen) { g.setColour (acid.withAlpha (0.16f)); g.fillRect (col); }
            const auto bar = juce::Rectangle<float> (col.getX() + 4, y, col.getWidth() - 8, inner.getBottom() - y + 2);
            if (on && inLen)
            {
                g.setColour (acid.withAlpha (i == cur ? 1.0f : (held ? 0.75f : 0.45f)));
                g.fillRect (bar.withHeight (juce::jmin (bar.getHeight(), 5.0f)));
                g.setColour (acid.withAlpha (i == cur ? 0.45f : 0.2f));
                g.fillRect (bar.withTrimmedTop (5.0f));
            }
            else
            {
                g.setColour (fg3.withAlpha (inLen ? 0.6f : 0.2f));
                g.drawRect (bar.withHeight (5.0f), 1.0f);
            }
            g.setColour (inLen ? (on ? fg2 : fg3) : fg3.withAlpha (0.3f));
            g.setFont (dwui::mono (9.0f));
            g.drawText (proc.degreeName (deg), col.withHeight (14.0f).toNearestInt(), juce::Justification::centred);
            g.drawText (juce::String (i + 1), col.withTrimmedTop (col.getHeight() - 13.0f).toNearestInt(), juce::Justification::centred);
        }
        g.setColour (fg3);
        g.setFont (dwui::font (10.0f));
        g.drawText (held ? (proc.recordNotes.load() ? "recording: play your keyboard" : "held") : "free: the arp keeps changing it",
                    gr.getRight() - 220, 8, 220, 20, juce::Justification::right);
    }
    else if (tab == Chords)
        paintChords (g);
    else if (tab == Harmony)
        paintHarmony (g);
    else if (tab == Loops)
        paintLoops (g);
    else if (tab == Pulse)
        paintPulse (g);
    else if (tab == Texture)
        paintTexture (g);
    else if (tab == Memory)
        paintMemory (g);
}


//==============================================================================
// LOOPS, PULSE, TEXTURE, MEMORY and the ORGANISM's ears
void PerformPanel::animate()
{
    for (int l = 0; l < dw::Pulse::Lanes; ++l)
    {
        const int h = proc.pulseLaneHits[(size_t) l].load();
        if (h != laneSeen[l]) { laneSeen[l] = h; laneFlash[l] = 1.0f; }
        else laneFlash[l] *= 0.82f;
    }
    for (int k = 0; k < Organism::maxEars; ++k)
    {
        const int h = proc.organism.earHits[(size_t) k].load();
        if (h != earSeen[k]) { earSeen[k] = h; earFlash[k] = 1.0f; }
        else earFlash[k] *= 0.93f;
    }
    // TEXTURE: a short history of how busy each slot is, and a mote for each new sound
    texHead = (texHead + 1) % texHistory;
    for (int i = 0; i < 2; ++i) texHist[i][texHead] = proc.texEnergy[(size_t) i].load();
    const int ev = proc.texEvents.load();
    int fresh = juce::jlimit (0, 6, ev - texEventsSeen);
    texEventsSeen = ev;
    while (fresh-- > 0)
    {
        auto& m = motes[motePos];
        motePos = (motePos + 1) % 48;
        m.x = uiRng.nextFloat(); m.y = uiRng.nextFloat(); m.life = 1.0f; m.slot = uiRng.nextInt (2);
    }
    for (auto& m : motes) m.life *= 0.9f;
}

void PerformPanel::paintLoops (juce::Graphics& g)
{
    using namespace dwui;
    const auto pv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const bool on = pv ("loop.on") > 0.5f;
    const int voices = juce::jlimit (1, 8, (int) std::lround (pv ("loop.voices")));
    const juce::Point<float> c (128.0f, 158.0f);
    const harm::Spelling sp (proc.keyPcNow.load(), (int) std::lround (pv ("arp.scale")));
    g.setColour (acid.withAlpha (0.25f));
    g.fillEllipse (c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
    for (int i = 0; i < dw::TapeLoops::V; ++i)
    {
        const float rad = 26.0f + (float) i * 12.5f;
        const bool live = on && i < voices;
        g.setColour ((live ? fg2 : fg3).withAlpha (live ? 0.3f : 0.1f));
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f, 1.0f);
        const float env = proc.loopEnv[(size_t) i].load();
        const float ph = proc.loopPhase[(size_t) i].load();
        const float a = ph * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::halfPi;
        const juce::Point<float> d (c.x + rad * std::cos (a), c.y + rad * std::sin (a));
        // the tape: a short bright arc behind the dot, longer when the note rings
        if (live || env > 0.01f)
        {
            juce::Path arc;
            const float len = 0.25f + 1.4f * env;
            arc.addCentredArc (c.x, c.y, rad, rad, 0.0f, a + juce::MathConstants<float>::halfPi - len, a + juce::MathConstants<float>::halfPi, true);
            g.setColour (acid.withAlpha (0.12f + 0.5f * env));
            g.strokePath (arc, juce::PathStrokeType (1.6f + 2.0f * env, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        drawGlowDot (g, d, live ? 0.25f + 0.75f * env : 0.08f, 2.4f + 3.0f * env);
        if (env > 0.06f)
        {
            g.setColour (fg.withAlpha (juce::jmin (1.0f, env * 1.6f)));
            g.setFont (dwui::mono (9.5f));
            const float tx = d.x + (d.x >= c.x ? 7.0f : -37.0f);
            g.drawText (harm::noteName (proc.loopNote[(size_t) i].load(), sp), juce::Rectangle<float> (tx, d.y - 6.0f, 30.0f, 12.0f),
                        d.x >= c.x ? juce::Justification::left : juce::Justification::right);
        }
    }
    g.setColour (on ? fg3 : fg3.withAlpha (0.6f));
    g.setFont (dwui::font (10.0f));
    g.drawText (on ? juce::String (voices) + " loops, " + juce::String (pv ("loop.length"), 1) + " s and longer" : "LOOPS is off",
                256, 250, 430, 14, juce::Justification::left);
}

void PerformPanel::paintWords (juce::Graphics& g)
{
    using namespace dwui;
    const bool on = proc.apvts.getRawParameterValue ("words.on")->load() > 0.5f;
    const auto box = wordsBox().toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRect (box);
    const auto* bank = proc.wordsNowBank.load();
    const int w = proc.wordsNowWord.load();
    if (w >= 0 && bank != nullptr) { wordsShown = w; wordsShownBank = bank; wordsGlow = 1.0f; }
    else wordsGlow *= 0.94f;
    // what it is speaking from
    g.setFont (dwui::font (10.0f));
    g.setColour (fg3);
    juce::String info;
    const auto* mine = proc.myWords();
    if (proc.wordsLoading.load()) info = "listening for words in your recordings...";
    else if (bank != nullptr)
        info = juce::String (bank->name) + ": " + juce::String ((int) bank->words.size()) + " words, "
             + juce::String ((int) bank->phrases.size()) + " phrases";
    else info = mine != nullptr ? juce::String (mine->name) : juce::String ("the synthetic voice");
    const auto err = proc.wordsErrorText();
    if (err.isNotEmpty() && ! proc.wordsLoading.load()) info << "  (" << err << ")";
    if (mine == nullptr && ! proc.wordsLoading.load()) info << ". LOAD WORDS to give it real voices";
    g.drawFittedText (info, box.reduced (8.0f, 4.0f).removeFromTop (14.0f).toNearestInt(), juce::Justification::left, 1, 0.8f);
    // the word it is saying: its text (the synthetic voice) or its shape (your recordings)
    const auto inner = box.reduced (10.0f, 22.0f);
    if (wordsShownBank != nullptr && wordsShown >= 0 && wordsShown < (int) wordsShownBank->words.size() && wordsGlow > 0.02f)
    {
        const auto& sg = wordsShownBank->words[(size_t) wordsShown];
        const auto col = acid.withAlpha (juce::jmin (1.0f, wordsGlow * (on ? 1.0f : 0.4f)));
        const std::string& tx = wordsShownBank->text[(size_t) wordsShown];
        if (! tx.empty())
        {
            g.setColour (col);
            g.setFont (dwui::font (30.0f).withExtraKerningFactor (0.12f));
            g.drawText (juce::String (tx), inner, juce::Justification::centred);
        }
        else
        {
            juce::Path pth;
            const int n = (int) inner.getWidth();
            const int len = juce::jmax (1, sg.end - sg.start);
            for (int i = 0; i < n; ++i)
            {
                float pk = 0.0f;
                const int a = sg.start + len * i / n, b = sg.start + len * (i + 1) / n;
                for (int k = a; k < juce::jmax (a + 1, b); ++k) pk = juce::jmax (pk, std::abs ((float) wordsShownBank->audio[(size_t) k] / 32768.0f));
                const float h = juce::jmin (1.0f, pk * 1.4f) * inner.getHeight() * 0.5f;
                pth.addRectangle (inner.getX() + (float) i, inner.getCentreY() - h, 1.0f, h * 2.0f + 0.5f);
            }
            g.setColour (col);
            g.fillPath (pth);
        }
    }
    else
    {
        g.setColour (fg3.withAlpha (0.5f));
        g.setFont (dwui::font (11.0f));
        g.drawText (on ? "listening..." : "WORDS is off", inner, juce::Justification::centred);
    }
}

void PerformPanel::paintWave (juce::Graphics& g)
{
    using namespace dwui;
    const auto pv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const int wave = (int) std::lround (pv ("arp.wave"));
    const int bank = (int) std::lround (pv ("arp.table"));
    const float fold = pv ("arp.fold");
    const float shape = wave != 0 && pv ("arp.move") > 0.001f ? proc.arpShapeNow.load() : pv ("arp.shape");
    const auto box = waveBox().toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRect (box);
    g.setColour (rule);
    g.drawHorizontalLine ((int) box.getCentreY(), box.getX() + 4.0f, box.getRight() - 4.0f);
    // two cycles of the wave
    juce::Path pth;
    const int n = (int) box.getWidth() - 12;
    for (int i = 0; i <= n; ++i)
    {
        float ph = 2.0f * (float) i / (float) n;
        ph -= std::floor (ph);
        const float y = wave == 0 ? dw::WaveOsc::shapeAt (dw::WaveOsc::Saw, ph, 0.0f, 0.0f, 0)
                                  : dw::WaveOsc::shapeAt (wave, ph, shape, fold, bank);
        const float px = box.getX() + 6.0f + (float) i, py = box.getCentreY() - juce::jlimit (-1.1f, 1.1f, y) * (box.getHeight() * 0.38f);
        if (i == 0) pth.startNewSubPath (px, py); else pth.lineTo (px, py);
    }
    g.setColour (wave == 0 ? fg3 : acid);
    g.strokePath (pth, juce::PathStrokeType (wave == 0 ? 1.0f : 1.6f, juce::PathStrokeType::curved));
    g.setFont (dwui::font (9.5f).withExtraKerningFactor (0.06f));
    g.setColour (fg3);
    const char* names[] { "PATCH: the VCO jacks as patched", "SINE", "TRIANGLE", "SAW", "SQUARE", "WAVETABLE" };
    juce::String label = names[juce::jlimit (0, 5, wave)];
    if (wave == 5) label << ": " << dw::Wavetables::bankName (bank);
    g.drawText (label, box.reduced (6.0f, 3.0f), juce::Justification::topLeft);
}

void PerformPanel::paintPulse (juce::Graphics& g)
{
    using namespace dwui;
    const auto pv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const bool on = pv ("pulse.on") > 0.5f;
    const int style = (int) std::lround (pv ("pulse.style"));
    const int kit = dw::Pulse::clampKit ((int) std::lround (pv ("pulse.kit")));
    const int lengthMode = (int) std::lround (pv ("pulse.length"));
    const int L = dw::Pulse::Lanes, N = dw::Pulse::Steps;
    if (pulsePage == 2) { paintBreak (g); return; }
    if (pulsePage == 1)
    {
        // SOUND: each drum's name flashes when it plays; rows are LEVEL, PITCH, DECAY
        const char* rows[] { "LEVEL", "PITCH", "DECAY" };
        const char* kits[] { "SOFT", "DEEP", "DUSTY", "GLASS", "HANDS", "GLITCH", "IDM" };
        g.setFont (dwui::font (10.0f).withExtraKerningFactor (0.08f));
        for (int row = 0; row < 3; ++row)
        {
            g.setColour (fg3);
            g.drawText (rows[row], 120, soundRowY[row] + 15, 42, 14, juce::Justification::centredRight);
        }
        for (int l = 0; l < L; ++l)
        {
            const int cx = soundCol (l);
            const bool mine = proc.pulseLaneHasSample (l);
            g.setColour ((on ? (mine ? acid : fg) : fg3).interpolatedWith (acid, laneFlash[l] * (on ? 0.9f : 0.0f)));
            g.setFont (dwui::font (10.5f).withExtraKerningFactor (0.08f));
            const juce::String name = mine ? juce::File (proc.pulseSamplePath[l]).getFileNameWithoutExtension().toUpperCase()
                                           : juce::String (dw::Pulse::laneName (kit, l));
            g.drawFittedText (name, cx - 36, 66, 72, 14, juce::Justification::centred, 1, 0.7f);
            g.setColour (acid.withAlpha (laneFlash[l] * (on ? 0.6f : 0.0f)));
            g.fillRect ((float) cx - 14.0f, 80.0f, 28.0f, 1.5f);
        }
        g.setColour (rule);
        g.drawVerticalLine (556, 70.0f, 230.0f);
        g.setColour (fg3);
        g.setFont (dwui::font (10.0f));
        g.drawText (juce::String ("the ") + kits[kit] + " kit. PUNCH pushes it forward, TONE opens it up",
                    120, 260, 568, 14, juce::Justification::right);
        return;
    }
    const auto gr = pulseGrid();
    const float cw = (float) gr.getWidth() / (float) N, lh = (float) gr.getHeight() / (float) L;
    for (int l = 0; l < L; ++l)
    {
        const float y = (float) gr.getY() + lh * (float) l;
        const int len = DriftwerkProcessor::pulseLaneLength (lengthMode, l);
        const int cur = proc.pulseLanePos[(size_t) l].load();
        g.setColour ((on ? fg2 : fg3).interpolatedWith (acid, laneFlash[l] * (on ? 0.9f : 0.0f)));
        g.setFont (dwui::font (9.5f).withExtraKerningFactor (0.06f));
        g.drawText (dw::Pulse::laneName (kit, l), juce::Rectangle<float> (120.0f, y, 56.0f, lh), juce::Justification::centredRight);
        for (int i = 0; i < N; ++i)
        {
            const auto cell = juce::Rectangle<float> ((float) gr.getX() + cw * (float) i, y, cw, lh).reduced (2.0f, 2.0f);
            if (i >= len)   // past this lane's length: it loops before it gets here
            {
                g.setColour (juce::Colours::white.withAlpha (0.015f));
                g.fillRect (cell);
                continue;
            }
            g.setColour (juce::Colours::white.withAlpha (i % 4 == 0 ? 0.07f : 0.04f));
            g.fillRect (cell);
            if (i == cur && on) { g.setColour (acid.withAlpha (0.14f)); g.fillRect (cell); }
            const bool hit = proc.pulseOnNow (l, i);
            const float w = proc.pulseW[(size_t) (l * N + i)].load();
            const int force = proc.pulseF[(size_t) (l * N + i)].load();
            if (hit)
            {
                const float vel = force > 0 ? 0.8f : 0.4f + 0.5f * juce::jmin (1.0f, w);
                const float h = cell.getHeight() * (0.35f + 0.65f * vel);
                g.setColour (acid.withAlpha (on ? (i == cur ? 1.0f : 0.75f) : 0.3f));
                g.fillRect (cell.withSizeKeepingCentre (juce::jmin (10.0f, cell.getWidth() - 6.0f), h));
            }
            else if (w > 0.0f)
            {
                g.setColour (fg3.withAlpha (0.25f + 0.4f * w));   // could play: DENSITY up lights it
                g.fillRect (cell.withSizeKeepingCentre (2.0f, 2.0f));
            }
            if (force != 0)
            {
                g.setColour (force > 0 ? acid : fg3);
                g.fillEllipse (cell.getRight() - 4.0f, cell.getY() + 1.0f, 3.0f, 3.0f);
            }
        }
        if (len < N)   // where the lane loops
        {
            g.setColour (acid.withAlpha (on ? 0.45f : 0.2f));
            g.fillRect ((float) gr.getX() + cw * (float) len - 1.0f, y + 2.0f, 1.5f, lh - 4.0f);
        }
    }
    g.setColour (fg3);
    g.setFont (dwui::font (10.0f));
    const char* how[] { "a slow lub-dub", "loose and sparse", "a lazy broken beat", "no beat: things falling", "a soft steady pulse",
                        "the slime's ears play the drums: high ears tick, low ears thump", "a backbeat with a lilt: turn SWING up",
                        "one drop on the third beat, hats on the offbeat", "hand drums, three against four", "a rocking low drum and a clave",
                        "a broken beat that never sits square", "a funky break with ghost snares", "a fast break, snares tumbling: try some CHOP",
                        "skittering, rolls everywhere: try the IDM kit and STUTTER" };
    g.drawFittedText (on ? how[juce::jlimit (0, (int) std::size (how) - 1, style)] : "PULSE is off", gr.getX(), gr.getBottom() + 1, gr.getWidth(), 14,
                      juce::Justification::right, 1, 0.8f);
}

void PerformPanel::paintTexture (juce::Graphics& g)
{
    using namespace dwui;
    const auto pv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const bool on = pv ("tex.on") > 0.5f;
    const int a = (int) std::lround (pv ("tex.a")), b = (int) std::lround (pv ("tex.b")) - 1;
    const char* names[] { "RAIN", "WIND", "STREAM", "CRACKLE", "CREATURES", "FIRE", "SHORE" };
    const juce::Rectangle<float> area (130.0f, 44.0f, 558.0f, 116.0f);
    for (int slot = 0; slot < 2; ++slot)
    {
        const auto lane = area.withHeight (area.getHeight() * 0.5f).translated (0.0f, area.getHeight() * 0.5f * (float) slot).reduced (0.0f, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.fillRect (lane);
        const int kind = slot == 0 ? a : b;
        const bool live = on && kind >= 0;
        if (live)
        {
            // a soft terrain: how busy it has been, newest on the right
            juce::Path p;
            p.startNewSubPath (lane.getX(), lane.getBottom());
            float peak = 1.0e-4f;
            for (int i = 0; i < texHistory; ++i) peak = juce::jmax (peak, texHist[slot][i]);
            for (int i = 0; i < texHistory; ++i)
            {
                const float v = texHist[slot][(texHead + 1 + i) % texHistory] / (peak * 1.15f);
                p.lineTo (lane.getX() + lane.getWidth() * (float) i / (float) (texHistory - 1), lane.getBottom() - lane.getHeight() * 0.8f * v);
            }
            p.lineTo (lane.getRight(), lane.getBottom());
            p.closeSubPath();
            g.setColour (acid.withAlpha (0.13f));
            g.fillPath (p);
            g.setColour (acid.withAlpha (0.5f));
            g.strokePath (p, juce::PathStrokeType (1.0f));
            for (auto& m : motes)
                if (m.slot == slot && m.life > 0.05f)
                    drawGlowDot (g, { lane.getX() + 10.0f + m.x * (lane.getWidth() - 20.0f), lane.getY() + 6.0f + m.y * (lane.getHeight() - 12.0f) }, m.life, 1.6f + 1.5f * m.life);
        }
        g.setColour (live ? fg : fg3);
        g.setFont (dwui::font (10.5f).withExtraKerningFactor (0.06f));
        g.drawText (kind >= 0 ? juce::String (slot == 0 ? "A  " : "B  ") + names[juce::jlimit (0, 6, kind)] : juce::String ("B  off"),
                    lane.reduced (6.0f, 3.0f).toNearestInt(), juce::Justification::topLeft);
    }
    if (! on)
    {
        g.setColour (fg3);
        g.setFont (dwui::font (10.0f));
        g.drawText ("TEXTURE is off", area.toNearestInt(), juce::Justification::centred);
    }
}

void PerformPanel::paintMemory (juce::Graphics& g)
{
    using namespace dwui;
    const auto pv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const bool on = pv ("mem.on") > 0.5f;
    const auto st = memStrip().toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.03f));
    g.fillRect (st);
    const float maxAgo = (float) dw::Memory::Seconds;
    auto xFor = [&] (float ago) { return st.getRight() - st.getWidth() * std::log1p (juce::jmax (0.0f, ago)) / std::log1p (maxAgo); };
    const float have = juce::jmin (maxAgo, proc.memSeconds.load());
    // not remembered yet
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (st.getX(), st.getY(), xFor (have), st.getBottom()));
    // what it remembers: louder moments taller, newest on the right (time runs on a log scale)
    const int head = proc.memPeakHead.load(), NP = dw::Memory::NumPeaks;
    const float mid = st.getCentreY();
    for (int x = (int) st.getX(); x < (int) st.getRight(); ++x)
    {
        // which seconds ago this pixel covers
        auto agoAt = [&] (float px) { return std::expm1 ((st.getRight() - px) / st.getWidth() * std::log1p (maxAgo)); };
        const float a0 = agoAt ((float) x + 1.0f), a1 = agoAt ((float) x);
        if (a1 > have) continue;
        float pk = 0.0f;
        for (int j = (int) std::floor (a0 * dw::Memory::PeakRate); j <= (int) std::ceil (a1 * dw::Memory::PeakRate); ++j)
            pk = juce::jmax (pk, proc.memPeaks[(size_t) (((head - 1 - j) % NP + NP) % NP)].load());
        const float h = juce::jmin (1.0f, pk * 1.6f) * st.getHeight() * 0.45f;
        g.setColour (fg.withAlpha (0.28f));
        g.drawVerticalLine (x, mid - h, mid + h);
    }
    // the heads, wandering back into it
    if (on)
    {
        const float ago = pv ("mem.ago");
        g.setColour (acid.withAlpha (0.07f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xFor (ago * 1.6f), st.getY(), xFor (ago * 0.55f), st.getBottom()));
        for (int k = 0; k < 2; ++k)
        {
            const float w = proc.memHeadWin[(size_t) k].load();
            const float x = xFor (proc.memHeadAgo[(size_t) k].load());
            const float speed = proc.memHeadSpeed[(size_t) k].load();
            g.setColour (acid.withAlpha (0.2f + 0.8f * w));
            g.drawLine (x, st.getY() + 2.0f, x, st.getBottom() - 2.0f, 1.0f + 1.5f * w);
            drawGlowDot (g, { x, st.getY() + 6.0f }, 0.3f + 0.7f * w, 2.5f + 2.0f * w);
            g.setFont (dwui::mono (9.0f));
            juce::String sp = std::abs (std::abs (speed) - 1.0f) < 0.01f ? "1x" : (std::abs (std::abs (speed) - 0.5f) < 0.01f ? "1/2" : (std::abs (std::abs (speed) - 0.25f) < 0.01f ? "1/4" : juce::String (std::abs (speed), 2)));
            if (speed < 0.0f) sp = "rev " + sp;
            g.setColour (fg.withAlpha (0.4f + 0.6f * w));
            g.drawText (sp, juce::Rectangle<float> (x - 30.0f, st.getBottom() - 15.0f - 12.0f * (float) k, 60.0f, 12.0f), juce::Justification::centred);
        }
    }
    // time marks
    g.setFont (dwui::mono (9.0f));
    const std::pair<float, const char*> marks[] { { 0.0f, "now" }, { 5.0f, "5s" }, { 15.0f, "15s" }, { 30.0f, "30s" }, { 60.0f, "1m" }, { 120.0f, "2m" }, { 200.0f, "3m20" } };
    for (auto& [s, t] : marks)
    {
        const float x = xFor (s);
        g.setColour (rule);
        g.drawVerticalLine ((int) x, st.getBottom(), st.getBottom() + 4.0f);
        g.setColour (fg3);
        g.drawText (t, juce::Rectangle<float> (x - 20.0f, st.getBottom() + 4.0f, 40.0f, 11.0f), juce::Justification::centred);
    }
    g.setColour (on ? fg2 : fg3);
    g.setFont (dwui::font (10.0f));
    const int secs = (int) have;
    g.drawText ((on ? "remembering " : "MEMORY is off. Remembered so far: ") + juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2),
                juce::Rectangle<float> (st.getX() + 6.0f, st.getY() + 3.0f, 300.0f, 12.0f), juce::Justification::left);
}

void PerformPanel::paintOrganism (juce::Graphics& g)
{
    using namespace dwui;
    const auto pv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const bool plays = pv ("slime.play") > 0.5f;
    const bool drums = pv ("pulse.on") > 0.5f && (int) std::lround (pv ("pulse.style")) == 5;
    const bool listening = plays || drums;
    const int ears = juce::jlimit (3, Organism::maxEars, (int) std::lround (pv ("slime.ears")));
    const harm::Spelling sp (proc.keyPcNow.load(), (int) std::lround (pv ("arp.scale")));
    const float W = (float) getWidth(), H = (float) getHeight();

    g.setColour (fg2);
    g.setFont (dwui::font (12.0f));
    g.drawText ("Organism 01", 14, 38, 200, 16, juce::Justification::left);
    g.setColour (acid);
    g.drawText (proc.camera.isOpen() ? "Following your hand" : (listening ? "Listening" : "Exploring"), 14, 54, 240, 16, juce::Justification::left);

    // the ears
    for (int k = 0; k < ears; ++k)
    {
        const juce::Point<float> c (proc.organism.earX[(size_t) k].load() * W, proc.organism.earY[(size_t) k].load() * H);
        const float lvl = proc.organism.earLevel[(size_t) k].load(), fl = earFlash[k];
        const float r = 6.0f + 4.0f * lvl + 10.0f * fl;
        g.setColour ((listening ? acid : fg3).withAlpha (listening ? 0.35f + 0.5f * fl : 0.25f));
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f + fl);
        g.setColour ((listening ? fg : fg3).withAlpha (0.5f));
        g.drawEllipse (c.x - 2.5f, c.y - 2.5f, 5.0f, 5.0f, 1.0f);
        if (fl > 0.05f && listening) drawGlowDot (g, c, fl, 3.0f + 3.0f * fl);
        const int note = proc.slimeEarNote[(size_t) k].load();
        if (plays && note >= 0 && fl > 0.08f)
        {
            g.setColour (fg.withAlpha (juce::jmin (1.0f, fl * 1.5f)));
            g.setFont (dwui::mono (10.0f));
            g.drawText (harm::noteName (note, sp), juce::Rectangle<float> (c.x - 20.0f, c.y - r - 14.0f, 40.0f, 12.0f), juce::Justification::centred);
        }
    }

    // the strip of settings
    const auto strip = juce::Rectangle<int> (organismStripX, 34, getWidth() - organismStripX, getHeight() - 62);
    g.setColour (juce::Colours::black.withAlpha (0.66f));
    g.fillRect (strip);
    g.setColour (rule);
    g.drawVerticalLine (organismStripX, (float) strip.getY(), (float) strip.getBottom());
    g.setFont (dwui::font (10.0f).withExtraKerningFactor (0.08f));
    for (auto& l : labs)
        if (shows (l.tab)) { g.setColour (fg2); g.drawText (l.t, l.r, juce::Justification::centred); }
    g.setColour (listening ? acid.withAlpha (0.8f) : fg3);
    g.setFont (dwui::font (10.0f));
    const int style = (int) std::lround (pv ("slime.style"));
    const juce::String what = ! plays ? " a drum." : (style == 1 ? " a chord." : (style == 2 ? " an arpeggio on the clock." : (style == 3 ? " a note, a chord or an arpeggio." : " its note.")));
    g.drawFittedText (listening ? juce::String (ears) + " ears listening. When a vein reaches one, it plays" + what
                                : juce::String ("SLIME PLAYS: veins reaching an ear play its note"),
                      strip.getX() + 10, strip.getBottom() - 30, strip.getWidth() - 20, 26, juce::Justification::centredLeft, 2);
}

void PerformPanel::paintChords (juce::Graphics& g)
{
    using namespace dwui;
    auto pv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const int style = juce::jlimit (0, 5, (int) std::lround (pv ("chd.mode")));
    const bool on = pv ("chd.on") > 0.5f;

    // The notes the chord layer is playing now.
    {
        const int n = proc.chordNoteCount.load();
        const auto area = juce::Rectangle<int> (8, 146, 116, 110);
        g.setColour (fg3);
        g.setFont (dwui::font (10.0f).withExtraKerningFactor (0.08f));
        g.drawText ("NOW PLAYING", area.withHeight (14), juce::Justification::centred);
        g.setFont (dwui::mono (12.0f));
        if (n == 0)
            g.drawText ("off", area.withTrimmedTop (20).withHeight (18), juce::Justification::centred);
        else
        {
            const harm::Spelling sp (proc.keyPcNow.load(), (int) std::lround (pv ("arp.scale")));
            juce::StringArray a, b;
            const int half = (n + 1) / 2;
            for (int i = 0; i < n; ++i) (i < half ? a : b).add (harm::noteName (proc.chordNotes[(size_t) i].load(), sp));
            g.setColour (acid);
            g.drawText (a.joinIntoString (" "), area.withTrimmedTop (20).withHeight (18), juce::Justification::centred);
            g.drawText (b.joinIntoString (" "), area.withTrimmedTop (38).withHeight (18), juce::Justification::centred);
        }
        g.setColour (fg3);
        g.setFont (dwui::font (10.0f));
        const char* how[] { "held, gliding between chords", "swells in every 2 bars", "on every beat", "with every arp note",
                            "strummed every bar", "plays the HITS pattern" };
        g.drawFittedText (how[style], area.withTrimmedTop (60).withHeight (28), juce::Justification::centred, 2);
    }

    // The HITS pattern.
    const auto gr = hitGrid();
    const int steps = juce::jlimit (2, 16, (int) std::lround (pv ("chd.steps")));
    const int cur = proc.hitStepNow.load();
    const float gate = pv ("chd.gate");
    const bool playing = on && style == 5;
    g.setColour (fg2);
    g.setFont (dwui::font (10.0f).withExtraKerningFactor (0.08f));
    g.drawText ("HITS", gr.getX(), 122, 100, 14, juce::Justification::left);
    g.setColour (playing ? acid.withAlpha (0.8f) : fg3);
    g.setFont (dwui::font (10.0f));
    g.drawText (playing ? "playing: click steps to force them on or off" : "set STYLE to STEPS (or click a step) to play this pattern",
                gr.getRight() - 360, 122, 360, 14, juce::Justification::right);

    const int n = DriftwerkProcessor::numHitSteps;
    const float cw = (float) gr.getWidth() / (float) n;
    bool hit[16] {};
    for (int i = 0; i < steps; ++i) hit[i] = proc.hitOnNow (i);
    for (int i = 0; i < n; ++i)
    {
        const auto col = juce::Rectangle<float> (gr.getX() + cw * (float) i, (float) gr.getY(), cw, (float) gr.getHeight()).reduced (2.0f, 0.0f);
        const bool inLen = i < steps;
        g.setColour (juce::Colours::white.withAlpha (inLen ? 0.05f : 0.015f));
        g.fillRect (col);
        if (i == cur && inLen && playing) { g.setColour (acid.withAlpha (0.16f)); g.fillRect (col); }
    }
    // How long each hit rings (GATE: a share of the time until the next hit).
    for (int i = 0; i < steps; ++i)
    {
        if (! hit[i]) continue;
        int gap = 1;
        while (gap < steps && ! hit[(i + gap) % steps]) ++gap;
        const float x0 = gr.getX() + cw * (float) i + 4.0f;
        const float x1 = juce::jmin ((float) gr.getX() + cw * (float) steps - 3.0f, x0 + cw * (float) gap * gate - 6.0f);
        g.setColour (acid.withAlpha (playing ? 0.28f : 0.12f));
        g.fillRect (juce::Rectangle<float> (x0, (float) gr.getCentreY() - 3.0f, juce::jmax (2.0f, x1 - x0), 6.0f));
    }
    for (int i = 0; i < n; ++i)
    {
        const auto col = juce::Rectangle<float> (gr.getX() + cw * (float) i, (float) gr.getY(), cw, (float) gr.getHeight()).reduced (2.0f, 0.0f);
        const bool inLen = i < steps;
        const auto block = col.reduced (4.0f, 9.0f).withWidth (juce::jmin (12.0f, col.getWidth() - 8.0f));
        if (inLen && hit[i])
        {
            g.setColour (acid.withAlpha (playing ? (i == cur ? 1.0f : 0.8f) : 0.35f));
            g.fillRect (block);
        }
        else
        {
            g.setColour (fg3.withAlpha (inLen ? 0.6f : 0.2f));
            g.drawRect (block, 1.0f);
        }
        const int force = proc.hitForce[(size_t) i].load();
        if (inLen && force != 0)
        {
            g.setColour (force > 0 ? acid : fg3);
            g.fillEllipse (col.getRight() - 7.0f, col.getY() + 3.0f, 4.0f, 4.0f);
        }
        if (i % 4 == 0)
        {
            g.setColour (fg3.withAlpha (inLen ? 0.8f : 0.3f));
            g.setFont (dwui::mono (9.0f));
            g.drawText (juce::String (i + 1), col.withTrimmedTop (col.getHeight() - 11.0f).toNearestInt(), juce::Justification::centredLeft);
        }
    }
}

void PerformPanel::animateWheel()
{
    // Chord tones glow in and out, arp notes flash, a ripple runs out when the chord changes.
    const int keyPc = proc.keyPcNow.load();
    const int mask = proc.chordIvMask.load();
    const int root = ((keyPc + proc.progOffset.load()) % 12 + 12) % 12;
    for (int pc = 0; pc < 12; ++pc)
    {
        const float t = ((mask >> ((pc - root + 12) % 12)) & 1) != 0 ? 1.0f : 0.0f;
        pcGlow[pc] = lastChordSig < 0 ? t : pcGlow[pc] + (t - pcGlow[pc]) * 0.22f;   // lit straight away the first time
    }
    const int sig = root * 4096 + mask;
    if (sig != lastChordSig) { if (lastChordSig >= 0) ripple = 0.0f; lastChordSig = sig; }
    ripple = juce::jmin (1.0f, ripple + 0.035f);
    const int ac = proc.arpNoteCount.load();
    if (ac != lastArpCount)
    {
        if (lastArpCount >= 0 && proc.apvts.getRawParameterValue ("arp.on")->load() > 0.5f)
        {
            flashes[flashPos] = { proc.arpNotePc.load(), 0.0f };
            flashPos = (flashPos + 1) % (int) std::size (flashes);
        }
        lastArpCount = ac;
    }
    for (auto& f : flashes) f.age = juce::jmin (1.0f, f.age + 0.07f);
}

void PerformPanel::paintHarmony (juce::Graphics& g)
{
    using namespace dwui;
    const auto v = proc.getProgressionView();
    const int keyPc = v.keyPc;
    const auto& sc = harm::scaleOf (v.scale);
    const auto& parent = harm::scaleOf (harm::parentScale (v.scale));
    bool inScale[12] {};
    for (int x : sc) inScale[(keyPc + x) % 12] = true;
    const int curMask = proc.chordIvMask.load();
    const int curRoot = ((keyPc + proc.progOffset.load()) % 12 + 12) % 12;
    const int nextRoot = ((keyPc + v.next.offset) % 12 + 12) % 12;
    const int nextMask = v.next.ivMask();
    auto isTone = [] (int mask, int root, int pc) { return ((mask >> ((pc - root + 12) % 12)) & 1) != 0; };

    const juce::Point<float> c (378.0f, 160.0f);
    const float rLabel = 101.0f, rDot = 84.0f, rArrow = 72.0f, rNum = 61.0f, rMid = 40.0f;
    auto angleOf = [keyPc] (int pc)
    {
        const int j = (((pc - keyPc) * 7) % 12 + 12) % 12;      // steps round the circle of fifths from the key
        return -juce::MathConstants<float>::halfPi + (float) j * juce::MathConstants<float>::twoPi / 12.0f;
    };
    auto at = [&c] (float a, float r) { return c + juce::Point<float> (std::cos (a), std::sin (a)) * r; };

    // Rings
    g.setColour (rule);
    g.drawEllipse (c.x - rDot, c.y - rDot, rDot * 2.0f, rDot * 2.0f, 1.0f);
    g.setColour (rule.withAlpha (0.5f));
    g.drawEllipse (c.x - rNum - 9.0f, c.y - rNum - 9.0f, (rNum + 9.0f) * 2.0f, (rNum + 9.0f) * 2.0f, 1.0f);

    // The key's notes: one arc round the wheel.
    for (int pc = 0; pc < 12; ++pc)
        if (inScale[pc] && inScale[(pc + 7) % 12])
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, rDot, rDot, 0.0f, angleOf (pc) + juce::MathConstants<float>::halfPi,
                               angleOf ((pc + 7) % 12) + juce::MathConstants<float>::halfPi, true);
            g.setColour (fg3.withAlpha (0.55f));
            g.strokePath (arc, juce::PathStrokeType (2.2f));
        }

    // Shapes: the next chord dashed, the chord playing now lit.
    auto shape = [&] (int mask, int root)
    {
        juce::Path p;
        bool first = true;
        for (int j = 0; j < 12; ++j)
        {
            const int pc = (keyPc + 7 * j) % 12;
            if (! isTone (mask, root, pc)) continue;
            const auto pt = at (angleOf (pc), rDot);
            if (first) p.startNewSubPath (pt); else p.lineTo (pt);
            first = false;
        }
        p.closeSubPath();
        return p;
    };
    if (v.mode != 0 && (nextMask != curMask || nextRoot != curRoot))
    {
        juce::Path dashed;
        const float dashes[] { 3.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, shape (nextMask, nextRoot), dashes, 2);
        g.setColour (fg2.withAlpha (0.4f));
        g.fillPath (dashed);
    }
    {
        const auto p = shape (curMask, curRoot);
        const float in = juce::jmin (1.0f, ripple * 3.0f);
        g.setColour (acid.withAlpha (0.10f * in));
        g.fillPath (p);
        g.setColour (acid.withAlpha (0.65f * in));
        g.strokePath (p, juce::PathStrokeType (1.3f));
    }

    // Where the chord goes next: an arrow round the inside, a dot travelling along it as the chord plays.
    if (v.mode != 0 && nextRoot != curRoot)
    {
        const float a0 = angleOf (curRoot);
        float a1 = angleOf (nextRoot);
        while (a1 - a0 > juce::MathConstants<float>::pi) a1 -= juce::MathConstants<float>::twoPi;
        while (a0 - a1 > juce::MathConstants<float>::pi) a1 += juce::MathConstants<float>::twoPi;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rArrow, rArrow, 0.0f, a0 + juce::MathConstants<float>::halfPi, a1 + juce::MathConstants<float>::halfPi, true);
        g.setColour (fg2.withAlpha (0.5f));
        g.strokePath (arc, juce::PathStrokeType (1.2f));
        const float dir = a1 > a0 ? 1.0f : -1.0f;
        const auto tip = at (a1, rArrow);
        const auto back = at (a1 - dir * 0.16f, rArrow);
        const auto side = juce::Point<float> (tip.y - back.y, back.x - tip.x) * 0.45f;
        juce::Path head;
        head.addTriangle (tip, back + side, back - side);
        g.fillPath (head);
        const auto dot = at (a0 + (a1 - a0) * v.phase, rArrow);
        drawGlowDot (g, dot, 0.9f, 2.4f);
    }

    // Notes round the outside, the chord's notes glowing.
    for (int pc = 0; pc < 12; ++pc)
    {
        const float a = angleOf (pc);
        const float glow = pcGlow[pc];
        const auto d = at (a, rDot);
        if (glow > 0.03f) drawGlowDot (g, d, glow, 3.0f + 2.0f * glow);
        else
        {
            g.setColour (inScale[pc] ? fg2 : fg3.withAlpha (0.35f));
            const float r = inScale[pc] ? 2.4f : 1.5f;
            g.fillEllipse (d.x - r, d.y - r, r * 2.0f, r * 2.0f);
        }
        const auto lp = at (a, rLabel);
        g.setFont (dwui::font (glow > 0.5f ? 12.5f : 11.0f, glow > 0.5f));
        g.setColour (glow > 0.5f ? acid : (inScale[pc] ? fg2 : fg3.withAlpha (0.4f)));
        g.drawText (v.spell (pc), juce::Rectangle<float> (lp.x - 16.0f, lp.y - 8.0f, 32.0f, 16.0f), juce::Justification::centred);
    }
    // Arp notes flash where they land.
    for (const auto& f : flashes)
        if (f.age < 1.0f)
        {
            const auto d = at (angleOf (f.pc), rDot);
            const float r = 4.0f + 11.0f * f.age;
            g.setColour (acid.withAlpha (0.75f * (1.0f - f.age)));
            g.drawEllipse (d.x - r, d.y - r, r * 2.0f, r * 2.0f, 1.4f);
        }

    // Roman numerals: the chord on each note of the key.
    for (int d = 0; d < (int) parent.size(); ++d)
    {
        const int pc = (keyPc + parent[(size_t) d]) % 12;
        const auto np = at (angleOf (pc), rNum);
        const bool now = pc == curRoot && v.cur.degree == d;
        g.setFont (dwui::font (now ? 11.5f : 9.5f, now));
        g.setColour (now ? acid : fg3);
        g.drawText (harm::numeral (harm::diatonic (v.scale, d, harm::TTriad, 1)),
                    juce::Rectangle<float> (np.x - 14.0f, np.y - 7.0f, 28.0f, 14.0f), juce::Justification::centred);
    }

    // A ripple when the chord changes.
    if (ripple < 1.0f)
    {
        const float r = rMid + (rDot - rMid) * ripple;
        g.setColour (acid.withAlpha (0.45f * (1.0f - ripple)));
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.5f);
    }

    // The middle: the chord, its numeral, the key, and how far through the chord we are.
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.fillEllipse (c.x - rMid, c.y - rMid, rMid * 2.0f, rMid * 2.0f);
    g.setColour (rule);
    g.drawEllipse (c.x - rMid, c.y - rMid, rMid * 2.0f, rMid * 2.0f, 1.0f);
    if (v.mode != 0)
    {
        juce::Path prog;
        prog.addCentredArc (c.x, c.y, rMid + 3.5f, rMid + 3.5f, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * juce::jlimit (0.0f, 1.0f, v.phase), true);
        g.setColour (acid.withAlpha (0.7f));
        g.strokePath (prog, juce::PathStrokeType (2.0f));
    }
    g.setColour (acid);
    g.setFont (dwui::font (17.0f));
    g.drawFittedText (v.curName, juce::Rectangle<int> ((int) c.x - 36, (int) c.y - 19, 72, 20), juce::Justification::centred, 1, 0.6f);
    g.setColour (fg2);
    g.setFont (dwui::font (11.0f));
    g.drawText (v.curNumeral, juce::Rectangle<int> ((int) c.x - 30, (int) c.y + 1, 60, 14), juce::Justification::centred);
    g.setColour (fg3);
    g.setFont (dwui::font (8.5f));
    g.drawFittedText (v.keyName, juce::Rectangle<int> ((int) c.x - 29, (int) c.y + 15, 58, 12), juce::Justification::centred, 1, 0.65f);

    // The chords of the loop.
    const char* caption = v.mode == 0 ? "PROGRESSION OFF: ONE CHORD"
                        : v.mode == harm::progWander ? "WANDER: THE CHORDS SO FAR"
                        : v.mode == harm::progMine ? "MY CHORDS: CLICK ONE TO CHANGE IT" : "THE LOOP: CLICK A CHORD TO EDIT";
    g.setColour (fg3);
    g.setFont (dwui::font (9.5f).withExtraKerningFactor (0.06f));
    g.drawText (caption, 498, 42, 192, 14, juce::Justification::left);
    double longest = 1.0;
    for (int k = 0; k < v.count; ++k) longest = juce::jmax (longest, v.beats[k]);
    for (int k = 0; k < harm::maxSlots; ++k)
    {
        const auto r = chordBox (k);
        if (k >= v.count)
        {
            if (v.mode == harm::progMine && k == v.count)
            {
                g.setColour (fg3.withAlpha (0.5f));
                g.drawRect (r, 1);
                g.setFont (dwui::font (16.0f));
                g.drawText ("+", r, juce::Justification::centred);
            }
            continue;
        }
        const bool now = k == v.current;
        g.setColour (now ? acid : rule);
        g.drawRect (r, 1);
        if (now) { g.setColour (acid.withAlpha (0.14f)); g.fillRect (r.reduced (1)); }
        g.setColour (now ? acid : fg);
        g.setFont (dwui::font (12.0f));
        g.drawFittedText (v.names[k], r.withHeight (22).withTrimmedTop (3).reduced (2, 0), juce::Justification::centred, 1, 0.55f);
        g.setColour (now ? acid.withAlpha (0.8f) : fg3);
        g.setFont (dwui::font (9.5f));
        g.drawText (v.numerals[k], r.withTrimmedTop (21).withHeight (12), juce::Justification::centred);
        // Length bar: how long this chord lasts, filling up while it plays.
        const float w = (float) (r.getWidth() - 8) * (float) (v.beats[k] / longest);
        const auto bar = juce::Rectangle<float> ((float) r.getX() + 4.0f, (float) r.getBottom() - 5.0f, w, 2.0f);
        g.setColour (fg3.withAlpha (0.6f));
        g.fillRect (bar);
        if (now) { g.setColour (acid); g.fillRect (bar.withWidth (w * v.phase)); }
    }
    g.setFont (dwui::font (10.5f));
    if (v.mode == 0)
    {
        g.setColour (fg3);
        g.drawFittedText ("pick a PROGRESSION, or press NEW CHORDS to make one of your own", juce::Rectangle<int> (498, 64, 190, 60),
                          juce::Justification::centredLeft, 3);
    }
    else
    {
        g.setColour (fg3);
        g.drawText ("next", 498, 150, 30, 14, juce::Justification::left);
        g.setColour (fg);
        g.drawText (v.nextName + "   " + v.nextNumeral, 530, 150, 158, 14, juce::Justification::left);
    }
}

void PerformPanel::showChordMenu (int k)
{
    // Editing a chord turns the progression into MY CHORDS first, so nothing else changes.
    if (proc.getProgressionView().mode != harm::progMine) proc.copyProgressionToMine();
    const auto v = proc.getProgressionView();
    if (k < 0 || k >= v.count) return;
    auto iv = [this] (const juce::String& id) { return (int) std::lround (proc.apvts.getRawParameterValue (id)->load()); };
    const juce::String n (k + 1);
    const int deg = iv ("prog.d" + n), type = iv ("prog.t" + n), len = iv ("prog.b" + n);
    const int colour = iv ("prog.colour"), drone = iv ("drn.chord");

    juce::PopupMenu m;
    m.addSectionHeader ("CHORD " + n);
    for (int d = 0; d < 7; ++d)
    {
        const auto h = harm::slotChord (v.scale, d, type, colour, drone);
        m.addItem (100 + d, harm::numeral (h) + "    " + harm::chordName (h, v.spell), true, d == deg);
    }
    m.addColumnBreak();
    m.addSectionHeader ("TYPE");
    for (int t = 0; t < harm::NumTypes; ++t)
        m.addItem (200 + t, t == 0 ? juce::String ("COLOUR (follows CHORD COLOUR)") : harm::slotTypeNames[t], true, t == type);
    m.addColumnBreak();
    m.addSectionHeader ("LENGTH");
    for (int b = 0; b < 7; ++b)
        m.addItem (300 + b, b == 0 ? juce::String ("EVEN (CHANGE EVERY)") : harm::slotLenNames[b], true, b == len);
    m.addSeparator();
    m.addItem (400, "remove this chord", v.count > 2);

    juce::Component::SafePointer<PerformPanel> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (chordBox (k))),
                     [safe, k, count = v.count] (int result)
    {
        if (safe == nullptr || result <= 0) return;
        auto& p = safe->proc;
        if (result >= 400)
        {
            for (int j = k; j + 1 < count; ++j)
            {
                const juce::String a (j + 1), b (j + 2);
                auto get = [&p] (const juce::String& id) { return (int) std::lround (p.apvts.getRawParameterValue (id)->load()); };
                p.setMyChord (j, get ("prog.d" + b), get ("prog.t" + b), get ("prog.b" + b));
            }
            p.setParam ("prog.len", (float) (count - 1 - 2));
        }
        else if (result >= 300) p.setMyChord (k, -1, -1, result - 300);
        else if (result >= 200) p.setMyChord (k, -1, result - 200, -1);
        else p.setMyChord (k, result - 100, -1, -1);
        safe->repaint();
    });
}

//==============================================================================
DriftwerkEditor::DriftwerkEditor (DriftwerkProcessor& p) : AudioProcessorEditor (&p), view (p)
{
    addAndMakeVisible (view);
    view.onFullScreen = [this] { toggleFullScreen(); };
    setResizable (true, true);
    setResizeLimits (MainView::designW / 3, MainView::designH / 3, MainView::designW * 2, MainView::designH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainView::designW / (double) MainView::designH);

    // Open at a size that fits the screen (minus taskbar and the host's window title).
    double fit = 0.8;
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = d->userArea;
        fit = juce::jmin ((area.getWidth() - 40) / (double) MainView::designW,
                          (area.getHeight() - 110) / (double) MainView::designH, 1.0);
    }
    fit = juce::jmax (fit, 0.4);
    setSize ((int) (MainView::designW * fit), (int) (MainView::designH * fit));
}

DriftwerkEditor::~DriftwerkEditor()
{
    if (fsWindow != nullptr) fsWindow->removeChildComponent (&view);
    fsWindow = nullptr;
    if (full && juce::Desktop::getInstance().getKioskModeComponent() != nullptr)
        juce::Desktop::getInstance().setKioskModeComponent (nullptr);
}

// FULL SCREEN. The whole rack moves into a borderless window of its own over the whole display
// (in Ableton and in the standalone app alike), fitted and centred, and comes back on Esc, F11
// or EXIT FULL.
void DriftwerkEditor::toggleFullScreen()
{
    // Switch on the next turn of the message loop: the click or key that asked for it may come
    // from inside the window that is about to go away.
    juce::Component::SafePointer<DriftwerkEditor> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->setFull (! safe->full); });
}

void DriftwerkEditor::setFull (bool on)
{
    if (on == full) return;
    if (on)
    {
        // The standalone app does the same as a host now: putting its own window into kiosk mode
        // let the app's window and the editor's size limits fight, and the rack zoomed in by itself.
        {
            const auto& displays = juce::Desktop::getInstance().getDisplays();
            const auto* d = displays.getDisplayForRect (getScreenBounds());
            const auto area = d != nullptr ? d->totalArea : displays.getTotalBounds (true);
            fsWindow = std::make_unique<FullScreenWindow> (*this);
            removeChildComponent (&view);
            fsWindow->addAndMakeVisible (view);
            fsWindow->setBounds (area);
            if (fullScreenOnDesktop)
            {
                fsWindow->addToDesktop (juce::ComponentPeer::windowAppearsOnTaskbar);
                fsWindow->setBounds (area);          // again, now that the window lives on that display
                fsWindow->setVisible (true);
                fsWindow->toFront (true);
                fsWindow->grabKeyboardFocus();
                fsWindow->keepCoveringScreen();
            }
        }
    }
    else
    {
        if (fsWindow != nullptr)
        {
            fsWindow->removeChildComponent (&view);
            fsWindow = nullptr;
            addAndMakeVisible (view);
            resized();
        }
        else if (juce::Desktop::getInstance().getKioskModeComponent() != nullptr)
            juce::Desktop::getInstance().setKioskModeComponent (nullptr);
    }
    full = on;
    view.setFullScreenLabel (full);
    repaint();
}

void DriftwerkEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    if (fsWindow != nullptr)
    {
        g.setColour (dwui::fg2);
        g.setFont (dwui::font (16.0f));
        g.drawFittedText ("Timewind is full screen.\nPress Esc there, or click here, to bring it back.",
                          getLocalBounds().reduced (20), juce::Justification::centred, 3);
    }
}

void DriftwerkEditor::mouseDown (const juce::MouseEvent&)
{
    if (fsWindow != nullptr) toggleFullScreen();
}

bool DriftwerkEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::F11Key || (full && k == juce::KeyPress::escapeKey)) { toggleFullScreen(); return true; }
    return false;
}

void DriftwerkEditor::fitInto (juce::Component& c, juce::Rectangle<int> area)
{
    // Fit the whole rack inside the area whatever its shape, centred.
    const float scale = juce::jmin ((float) area.getWidth() / (float) MainView::designW,
                                    (float) area.getHeight() / (float) MainView::designH);
    const float ox = (float) area.getX() + ((float) area.getWidth()  - MainView::designW * scale) * 0.5f;
    const float oy = (float) area.getY() + ((float) area.getHeight() - MainView::designH * scale) * 0.5f;
    c.setTransform (juce::AffineTransform::scale (scale).translated (ox, oy));
    c.setBounds (0, 0, MainView::designW, MainView::designH);
}

FullScreenWindow::FullScreenWindow (DriftwerkEditor& e) : editor (e)
{
    setName ("Timewind");
    setOpaque (true);
    setWantsKeyboardFocus (true);
}

void FullScreenWindow::keepCoveringScreen()
{
    timerCallback();
    startTimer (250);
}

void FullScreenWindow::timerCallback()
{
    // Windows decides the monitor's size here, in the window's own units; JUCE then reads the
    // window's real size back and resized() fits the rack into it. Capped, so it can never fight.
    if (auto* peer = getPeer())
        if (fixes < 40 && dwnative::coverMonitor (peer->getNativeHandle()))
            ++fixes;
}

void FullScreenWindow::resized()
{
    if (getNumChildComponents() > 0) DriftwerkEditor::fitInto (*getChildComponent (0), getLocalBounds());
}

bool FullScreenWindow::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::F11Key || k == juce::KeyPress::escapeKey) { editor.toggleFullScreen(); return true; }
    return false;
}

void FullScreenWindow::userTriedToCloseWindow() { editor.toggleFullScreen(); }

void DriftwerkEditor::resized()
{
    if (view.getParentComponent() == this) fitInto (view, getLocalBounds());
}
