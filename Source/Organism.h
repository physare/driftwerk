// The organism: the slime mold living inside Timewind. It grows on a thread of its own, so it
// keeps living (and playing) with the window closed, and the window shows this same slime.
// Ears listen to it: when a vein reaches an ear, the ear sends an event to the audio thread,
// which turns it into a note (SLIME PLAYS) or a drum hit (PULSE style ORGANISM).
#pragma once

#include "SlimeSim.h"
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

class Organism : private juce::Thread
{
public:
    static constexpr int W = 300, H = 128, maxEars = 12;
    struct Event { int ear = 0; float strength = 0.0f, x = 0.5f, y = 0.5f; };

    Organism() : juce::Thread ("Timewind organism"), sim (W, H, 22000)
    {
        for (int k = 0; k < maxEars; ++k)
        {
            earX[(size_t) k].store (homeX (k)); earY[(size_t) k].store (homeY (k));
            earLevel[(size_t) k].store (0.0f); earHits[(size_t) k].store (0);
        }
    }
    ~Organism() override { stopThread (4000); }

    void ensureRunning() { if (! manual && ! isThreadRunning()) startThread (juce::Thread::Priority::low); }

    // Inputs (any thread)
    std::atomic<float> handX { 0.0f }, handY { 0.0f }, motion { 0.0f }, level { 0.0f };
    std::atomic<bool> handActive { false }, listening { false };
    std::atomic<int> viewers { 0 }, earCount { 7 };
    std::atomic<float> growth { 1.0f }, activity { 0.5f }, weird { 0.3f };
    bool manual = false;                 // offline renders call tick() themselves

    // The ears, for the editor
    std::array<std::atomic<float>, maxEars> earX, earY, earLevel;
    std::array<std::atomic<int>, maxEars> earHits;
    std::atomic<int> totalHits { 0 };

    // One frame of life (1/30 s): grow, then listen.
    void tick()
    {
        const float dt = 1.0f / 30.0f;
        const juce::ScopedLock sl (lock);
        stepAcc += 2.0f * juce::jlimit (0.0f, 4.0f, growth.load());
        while (stepAcc >= 1.0f)
        {
            stepAcc -= 1.0f;
            sim.step (handX.load(), handY.load(), motion.load(), handActive.load(), level.load());
        }
        t += dt;
        const int n = juce::jlimit (1, maxEars, earCount.load());
        const float act = juce::jlimit (0.0f, 1.0f, activity.load()), w = juce::jlimit (0.0f, 1.0f, weird.load());
        const bool on = listening.load();
        globalCool -= dt;
        for (int k = 0; k < maxEars; ++k)
        {
            if (k >= n) { earLevel[(size_t) k].store (0.0f); continue; }
            // With WEIRD the ears wander about.
            const float wx = w * 0.07f * std::sin (t * (0.05f + 0.011f * (float) k) + (float) k * 1.7f);
            const float wy = w * 0.09f * std::sin (t * (0.041f + 0.013f * (float) k) + (float) k * 2.3f);
            const float ex = juce::jlimit (0.03f, 0.6f, homeX (k) + wx), ey = juce::jlimit (0.06f, 0.94f, homeY (k) + wy);
            earX[(size_t) k].store (ex); earY[(size_t) k].store (ey);
            const float d = sim.density (ex, ey, 3);
            if (! primed) { fast[k] = slow[k] = d; }
            fast[k] += (d - fast[k]) * 0.35f;
            slow[k] += (d - slow[k]) * 0.02f;
            const float rise = fast[k] - slow[k];
            const float thresh = (0.12f + 2.4f * (1.0f - act) * (1.0f - act)) * (1.0f + 0.12f * slow[k]);
            earLevel[(size_t) k].store (juce::jlimit (0.0f, 1.0f, rise / thresh));
            cool[k] -= dt;
            if (on && primed && rise > thresh && cool[k] <= 0.0f && globalCool <= 0.0f)
            {
                push ({ k, juce::jlimit (0.0f, 1.0f, (rise - thresh) / thresh), juce::jlimit (0.0f, 1.0f, (ex - 0.03f) / 0.57f), ey });
                cool[k] = 0.6f + 5.0f * (1.0f - act) * (1.0f - act) + 1.5f * rnd();
                globalCool = 0.06f + 0.9f * (1.0f - act) * (1.0f - act);
                earHits[(size_t) k].fetch_add (1);
                totalHits.fetch_add (1);
            }
        }
        primed = true;
    }

    // Audio thread: the next ear event, if any.
    bool popEvent (Event& e)
    {
        const auto scope = fifo.read (1);
        if (scope.blockSize1 > 0) { e = events[(size_t) scope.startIndex1]; return true; }
        if (scope.blockSize2 > 0) { e = events[(size_t) scope.startIndex2]; return true; }
        return false;
    }

    // Draw or read the slime (any thread except audio).
    template <typename Fn> void withSim (Fn&& fn) { const juce::ScopedLock sl (lock); fn (sim); }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            if (viewers.load() > 0 || listening.load()) tick();
            wait (33);
        }
    }
    static float frac (float x) { return x - std::floor (x); }
    static float homeX (int k) { return 0.05f + 0.52f * frac (0.5f + 0.6180339f * (float) k); }   // the left part: the controls sit on the right
    static float homeY (int k) { return 0.16f + 0.68f * frac (0.2f + 0.7548777f * (float) k); }

    void push (const Event& e)
    {
        const auto scope = fifo.write (1);
        if (scope.blockSize1 > 0) events[(size_t) scope.startIndex1] = e;
        else if (scope.blockSize2 > 0) events[(size_t) scope.startIndex2] = e;
    }

    juce::CriticalSection lock;
    SlimeSim sim;
    float stepAcc = 0.0f, t = 0.0f, globalCool = 0.0f;
    float fast[maxEars] {}, slow[maxEars] {}, cool[maxEars] {};
    bool primed = false;
    uint32_t seed = 0xEA25u;
    float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; }

    juce::AbstractFifo fifo { 64 };
    std::array<Event, 64> events {};
};
