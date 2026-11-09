// A small CPU Physarum (slime mold) simulation, after Jones (2010):
// agents sense the trail ahead, ahead-left and ahead-right, turn toward the strongest,
// move and deposit; the trail diffuses and decays. Agents stream out of a drifting core,
// are drawn toward the player's hand, and the body pulses with the music.
#pragma once

#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <cmath>

class SlimeSim
{
public:
    SlimeSim (int w, int h, int agents) : W (w), H (h), trail ((size_t) (w * h), 0.0f), tmp ((size_t) (w * h), 0.0f),
                                          vein ((size_t) (w * h), 0.0f), image (juce::Image::RGB, w, h, true)
    {
        a.resize ((size_t) agents);
        coreX = W * 0.32f; coreY = H * 0.55f;
        for (auto& ag : a) respawn (ag);
        for (int i = 0; i < 260; ++i) step (0.0f, 0.0f, 0.0f, false, 0.1f); // head start, never opens on black
    }

    // handX/handY in -1..1 (y up), motion 0..1, level 0..1 (audio peak)
    void step (float handX, float handY, float motion, bool handActive, float level)
    {
        ++tick;
        const float hx = (handX * 0.5f + 0.5f) * (float) (W - 1);
        const float hy = (0.5f - handY * 0.5f) * (float) (H - 1);

        // The core wanders slowly, and creeps toward the hand when you play.
        coreDrift += 0.0017f;
        float tx = W * (0.5f + 0.28f * std::sin (coreDrift * 0.9f)), ty = H * (0.5f + 0.25f * std::sin (coreDrift * 1.37f));
        if (handActive) { tx = tx * 0.5f + hx * 0.5f; ty = ty * 0.5f + hy * 0.5f; }
        coreX += (tx - coreX) * 0.004f; coreY += (ty - coreY) * 0.004f;

        // Food at the hand: the network grows toward movement.
        if (handActive && motion > 0.02f)
            stamp (hx, hy, 9.0f, 2.5f + motion * 14.0f);
        stamp (coreX, coreY, 6.0f, 3.0f);

        const float pulse = 1.0f + 0.35f * level * std::sin ((float) tick * 0.12f);
        const float SA = 0.45f, SO = 7.0f, RA = 0.32f, speed = 1.0f + motion * 0.8f;
        const float dep = 1.0f * pulse;

        for (auto& ag : a)
        {
            if (--ag.life <= 0) { respawn (ag); continue; }
            const float f = sense (ag, 0.0f, SO), l = sense (ag, SA, SO), r = sense (ag, -SA, SO);
            const float jit = (rnd() - 0.5f) * 0.4f;
            if (f >= l && f >= r) ag.h += jit;
            else if (l > r) ag.h += RA + jit;
            else if (r > l) ag.h -= RA + jit;
            else ag.h += (rnd() - 0.5f) * 1.2f;

            ag.x += std::cos (ag.h) * speed;
            ag.y += std::sin (ag.h) * speed;
            if (ag.x < 1 || ag.x >= W - 2 || ag.y < 1 || ag.y >= H - 2) { ag.h += 3.14159f; ag.x = juce::jlimit (1.0f, W - 2.0f, ag.x); ag.y = juce::jlimit (1.0f, H - 2.0f, ag.y); }
            { auto& t = trail[(size_t) ((int) ag.y * W + (int) ag.x)]; t = std::min (t + dep, 25.0f); }
        }

        // Diffuse (3x3 box) and decay.
        for (int y = 1; y < H - 1; ++y)
            for (int x = 1; x < W - 1; ++x)
            {
                const int i = y * W + x;
                const float sum = trail[(size_t) i - (size_t) W - 1] + trail[(size_t) i - (size_t) W] + trail[(size_t) i - (size_t) W + 1]
                                + trail[(size_t) i - 1] + trail[(size_t) i] + trail[(size_t) i + 1]
                                + trail[(size_t) i + (size_t) W - 1] + trail[(size_t) i + (size_t) W] + trail[(size_t) i + (size_t) W + 1];
                tmp[(size_t) i] = (trail[(size_t) i] * 0.5f + sum / 9.0f * 0.5f) * 0.86f;
            }
        std::swap (trail, tmp);

        // Veins: a slow field that integrates busy paths, so they harden.
        for (size_t i = 0; i < vein.size(); ++i)
            vein[i] += (std::min (trail[i], 12.0f) - vein[i]) * 0.03f;
    }

    const juce::Image& render (float level)
    {
        // Acid yellow (#f0f870) on black, hotter toward white in the thickest veins.
        juce::Image::BitmapData bd (image, juce::Image::BitmapData::writeOnly);
        const float pulseK = 0.9f + 0.6f * level;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const size_t i = (size_t) (y * W + x);
                // A contraction wave travelling out from the core.
                const float d = std::hypot ((float) x - coreX, (float) y - coreY);
                const float wave = 1.0f + 0.18f * level * std::sin (d * 0.12f - (float) tick * 0.09f);
                const float v = (trail[i] * 0.09f + vein[i] * 0.1f) * wave * pulseK;
                const float c = 1.0f - std::exp (-v * 1.4f);
                const float hot = juce::jlimit (0.0f, 1.0f, (c - 0.82f) * 4.0f);
                const auto r = (juce::uint8) juce::jlimit (0.0f, 255.0f, 240.0f * c + 15.0f * hot);
                const auto g = (juce::uint8) juce::jlimit (0.0f, 255.0f, 248.0f * c + 7.0f * hot);
                const auto b = (juce::uint8) juce::jlimit (0.0f, 255.0f, 112.0f * c * c + 140.0f * hot);
                bd.setPixelColour (x, y, juce::Colour (r, g, b));
            }
        return image;
    }

    float coreX = 0, coreY = 0;
    int tick = 0;

private:
    struct Agent { float x = 0, y = 0, h = 0; int life = 0; };

    void respawn (Agent& ag)
    {
        // Most agents land anywhere on the substrate; a stream keeps leaving the core.
        const float ang = rnd() * 6.2831853f;
        if (rnd() < 0.3f)
        {
            const float rad = rnd() * 6.0f;
            ag.x = juce::jlimit (1.0f, W - 2.0f, coreX + std::cos (ang) * rad);
            ag.y = juce::jlimit (1.0f, H - 2.0f, coreY + std::sin (ang) * rad);
        }
        else
        {
            ag.x = 1.0f + rnd() * (W - 3.0f);
            ag.y = 1.0f + rnd() * (H - 3.0f);
        }
        ag.h = ang;
        ag.life = 500 + (int) (rnd() * 1500.0f);
    }

    float sense (const Agent& ag, float angle, float dist) const
    {
        const int x = (int) (ag.x + std::cos (ag.h + angle) * dist);
        const int y = (int) (ag.y + std::sin (ag.h + angle) * dist);
        if (x < 0 || y < 0 || x >= W || y >= H) return -1.0f;
        return trail[(size_t) (y * W + x)];
    }

    void stamp (float cx, float cy, float radius, float amount)
    {
        const int r = (int) radius;
        for (int y = (int) cy - r; y <= (int) cy + r; ++y)
            for (int x = (int) cx - r; x <= (int) cx + r; ++x)
            {
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                const float d2 = ((float) x - cx) * ((float) x - cx) + ((float) y - cy) * ((float) y - cy);
                trail[(size_t) (y * W + x)] += amount * std::exp (-d2 / (radius * radius * 0.5f)) * 0.1f;
            }
    }

    float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; }

    int W, H;
    std::vector<float> trail, tmp, vein;
    std::vector<Agent> a;
    juce::Image image;
    float coreDrift = 0.0f;
    unsigned int seed = 0x51171Eu;
};
