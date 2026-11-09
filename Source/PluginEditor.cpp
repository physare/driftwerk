#include "PluginEditor.h"

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
        const float rr = radius * 1.1f;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rr, rr, 0.0f, start + from * (end - start), start + to * (end - start), true);
        g.setColour (fg.withAlpha (0.75f));
        g.strokePath (arc, juce::PathStrokeType (1.4f));
        if (now >= 0.0f)
        {
            const float na = start + now * (end - start);
            g.setColour (acid.withAlpha (0.3f));
            g.fillEllipse (c.x + rr * std::sin (na) - 6.0f, c.y - rr * std::cos (na) - 6.0f, 12.0f, 12.0f);
            g.setColour (fg);
            g.fillEllipse (c.x + rr * std::sin (na) - 2.6f, c.y - rr * std::cos (na) - 2.6f, 5.2f, 5.2f);
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
    g.setColour (lit ? acid : juce::Colours::black);
    g.fillRect (r);
    g.setColour (lit ? acid : juce::Colour (0x40ffffff));
    g.drawRect (r, 1.0f);
}

void SlimeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down)
{
    const bool lit = over || down || b.getToggleState();
    g.setColour (lit ? juce::Colours::black : fg);
    g.setFont (dwui::font (12.0f).withExtraKerningFactor (0.08f));
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
    const auto c = juce::Point<float> (r.getCentreX(), r.getBottom() - 11.0f);
    if (isOutput) drawGlowDot (g, { c.x, r.getY() + 4.0f }, led, 2.2f);

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
        g.drawFittedText (proc.camera.status.toLowerCase(), screen.reduced (6.0f).toNearestInt(), juce::Justification::centred, 3);
    }
}

//==============================================================================
void SlimeView::advance()
{
    const bool active = proc.camera.isOpen();
    sim.step (proc.monitor[O_CAM_X].load(), proc.monitor[O_CAM_Y].load(), proc.monitor[O_CAM_MOTION].load(),
              active, juce::jlimit (0.0f, 1.0f, proc.outputLevel.load() * 1.6f));
}

void SlimeView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.fillAll (juce::Colours::black);
    const float level = juce::jlimit (0.0f, 1.0f, proc.outputLevel.load() * 1.6f);
    const auto& img = sim.render (level);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (img, r);
    // Soft bloom: the same frame, blurred by scaling, drawn faintly on top.
    if (! frame.isValid()) frame = juce::Image (juce::Image::RGB, 75, 32, true);
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
    g.drawText ("Organism 01", 14, 10, 200, 16, juce::Justification::left);
    g.setColour (acid);
    g.drawText (cam ? "Following your hand" : "Exploring", 14, 26, 240, 16, juce::Justification::left);
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
    g.drawImage (slime.sim.render (level), pic);
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
        const juce::String hs = ! ht.isReady() ? ht.status.toLowerCase()
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
        { M_DRONE, "Drone", "six drifting saws", 1, 640, 160, 2,
          { { "drn.chord", "CHORD", 1 }, { "drn.oct", "OCTAVE", 0 }, { "drn.detune", "DETUNE", 0 }, { "drn.drift", "DRIFT", 0 }, { "drn.level", "LEVEL", 0 } } },
        { M_NOISE, "Noise", "sample + hold", 1, 800, 100, 1, { { "nz.slew", "SLEW", 0 } } },
        { M_LFO1, "LFO 1", "slow", 1, 900, 100, 1, { { "lfo1.rate", "RATE", 0 }, { "lfo1.shape", "SHAPE", 1 } } },
        { M_LFO2, "LFO 2", "slow", 1, 1000, 100, 1, { { "lfo2.rate", "RATE", 0 }, { "lfo2.shape", "SHAPE", 1 } } },
        { M_DREAM, "Dream", "loops of unequal length", 1, 1100, 170, 2,
          { { "dream.chord", "CHORD", 1 }, { "dream.rate", "SPEED", 0 }, { "dream.tone", "TONE", 0 }, { "dream.warp", "WARP", 0 }, { "dream.level", "LEVEL", 0 } } },
        { M_CAMERA, "Hand", "webcam tracking", 1, 1270, 230, 1,
          { { "cam.on", "CAM", 2, 16, 160 }, { "cam.sens", "SENS", 0, 89, 160 }, { "cam.slew", "SMOOTH", 0, 162, 160 } } },
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

    slimeRect = { 568, t0, 700, rowHeight (0) };
    slime = std::make_unique<SlimeView> (proc);
    slime->setBounds (slimeRect);
    addAndMakeVisible (*slime);

    playRect = { 1276, t0, 224, rowHeight (0) };
    sceneButton.setBounds (playRect.getX() + 16, t0 + 124, 192, 34);
    sceneButton.onClick = [this] { proc.rollScene(); refreshPatch(); };
    addAndMakeVisible (sceneButton);
    scope = std::make_unique<ScopeComponent> (proc);
    scope->setBounds (playRect.getX() + 12, t0 + 170, 200, 70);
    addAndMakeVisible (*scope);

    cameraView = std::make_unique<CameraView> (proc);
    cameraView->setBounds (1270 + 10, rowTop (1) + 40, 210, 112);
    addAndMakeVisible (*cameraView);

    // Header
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); refreshPatch(); };
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

    fullButton.onClick = [this] { openFullCamera (true); };
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
    const int cellH = sp.module == M_ARP ? 60 : 66;

    for (int c = 0; c < (int) sp.controls.size(); ++c)
    {
        const auto& cd = sp.controls[(size_t) c];
        const int col = c % sp.cols, row = c / sp.cols;
        const juce::Rectangle<int> cell = cd.rx >= 0 ? juce::Rectangle<int> (sp.x + cd.rx, top + cd.ry, 54, cellH)
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
        jack.setBounds (x + (jackW - 30) / 2, yy + 14, 30, 30);
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
    g.drawFittedText ("drag jack to jack to patch  /  right-click a jack to unplug\nright-click a knob to play it by hand",
                      350, 8, 360, 40, juce::Justification::centredLeft, 2);
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

void MainView::openFullCamera (bool open)
{
    fullCam->setVisible (open);
    if (open) { fullCam->toFront (true); fullCam->grabKeyboardFocus(); }
}

void MainView::resized()
{
    cables->setBounds (getLocalBounds());
    fullCam->setBounds (getLocalBounds());
    fullButton.setBounds (getWidth() - 780, 14, 170, 26);
    presetBox.setBounds (getWidth() - 600, 14, 200, 26);
    randomButton.setBounds (getWidth() - 390, 14, 130, 26);
    clearButton.setBounds (getWidth() - 252, 14, 100, 26);
    dimButton.setBounds (getWidth() - 144, 14, 128, 26);
}

//==============================================================================
void MainView::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a sound for GRAIN", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile()) { proc.loadSample (f); grainView->repaint(); }
        });
}

void MainView::filesDropped (const juce::StringArray& files, int, int)
{
    grainView->dragHover = false;
    for (auto& path : files)
        if (proc.loadSample (juce::File (path))) break;
    grainView->repaint();
}

void MainView::showModMenu (ModSlider& s)
{
    if (s.paramIndex < 0) return;
    auto& slot = *proc.mods[(size_t) s.paramIndex];
    const int current = slot.source.load();
    const float depth = slot.depth.load();

    struct Src { int out; const char* name; };
    const Src handSources[] { { O_CAM_X, "Hand X  (left / right)" }, { O_CAM_Y, "Hand Y  (up / down)" },
                              { O_CAM_MOTION, "Hand motion  (how much you move)" } };
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
    return { b.getCentreX(), b.getBottom() - 11.0f };
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
    if (dragOut < 0 && dragIn < 0)
    {
        const auto ps = proc.getPatchString();
        if (ps != lastPatch) { lastPatch = ps; cables->repaint(); }
    }
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

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

    slime->advance();
    slime->advance();
    if (fullCam->isVisible())
    {
        fullCam->pushTrail();
        fullCam->repaint();
        return;
    }
    slime->repaint();
    scope->repaint();
    grainView->repaint();
    if ((++frameCounter & 1) == 0) cameraView->repaint();
    repaint (arpLedArea.expanded (8));
    repaint (playRect.withTrimmedTop (250));
    for (auto& r : moduleLedAreas) if (! r.isEmpty()) repaint (r.expanded (10));
}

//==============================================================================
DriftwerkEditor::DriftwerkEditor (DriftwerkProcessor& p) : AudioProcessorEditor (&p), view (p)
{
    addAndMakeVisible (view);
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

void DriftwerkEditor::resized()
{
    // Fit the whole rack inside the window whatever shape the host gives us, centred.
    const float scale = juce::jmin ((float) getWidth() / (float) MainView::designW,
                                    (float) getHeight() / (float) MainView::designH);
    const float ox = ((float) getWidth()  - MainView::designW * scale) * 0.5f;
    const float oy = ((float) getHeight() - MainView::designH * scale) * 0.5f;
    view.setTransform (juce::AffineTransform::scale (scale).translated (ox, oy));
    view.setBounds (0, 0, MainView::designW, MainView::designH);
}
