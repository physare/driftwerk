// Webcam motion tracker: frame differencing on a small grayscale image gives
// hand position (centroid of movement), amount of motion, and gesture triggers.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#if JUCE_USE_CAMERA
 #include <juce_video/juce_video.h>
#endif
#include <atomic>
#include "HandTracker.h"
#include <cmath>

class CameraTracker
#if JUCE_USE_CAMERA
    : private juce::CameraDevice::Listener
#endif
{
public:
    static constexpr int W = 64, H = 48;

    ~CameraTracker() { close(); }

    static bool isSupported()
    {
       #if JUCE_USE_CAMERA
        return true;
       #else
        return false;
       #endif
    }

    // Message thread only.
    bool open()
    {
       #if JUCE_USE_CAMERA
        if (device != nullptr) return true;
        if (hand.enabled.load()) hand.start(); // loads the hand skeleton once; motion tracking works without it
        const auto names = juce::CameraDevice::getAvailableDevices();
        status = names.isEmpty() ? "NO CAMERA FOUND" : "STARTING";
        if (names.isEmpty()) return false;
        device.reset (juce::CameraDevice::openDevice (0, 160, 120, 640, 480, false));
        if (device == nullptr) { status = "CAMERA BUSY"; return false; }
        device->addListener (this);
        status = names[0].toUpperCase();
        return true;
       #else
        status = "CAMERA: WINDOWS/MAC ONLY";
        return false;
       #endif
    }

    void close()
    {
       #if JUCE_USE_CAMERA
        if (device != nullptr)
        {
            device->removeListener (this);
            device.reset();
        }
       #endif
        motion.store (0.0f);
        status = "CAMERA OFF";
        const juce::SpinLock::ScopedLockType sl (previewLock);
        preview = {};
        colour = {};
    }

    bool isOpen() const
    {
       #if JUCE_USE_CAMERA
        return device != nullptr;
       #else
        return false;
       #endif
    }

    // Analysis results (read by the audio thread)
    std::atomic<float> handX { 0.0f }, handY { 0.0f }, motion { 0.0f };
    std::atomic<int> triggerCount { 0 }, waveCount { 0 };
    std::atomic<float> sensitivity { 0.5f };
    juce::String status { "CAMERA OFF" };
    HandTracker hand;

    // Larger colour frame for the full-screen view (not mirrored).
    juce::Image getColourFrame()
    {
        const juce::SpinLock::ScopedLockType sl (previewLock);
        return colour.isValid() ? colour.createCopy() : juce::Image();
    }

    juce::Image getPreview()
    {
        const juce::SpinLock::ScopedLockType sl (previewLock);
        return preview.isValid() ? preview.createCopy() : juce::Image();
    }

    // Exposed for testing: analyse one frame.
    void analyse (const juce::Image& frame)
    {
        if (! frame.isValid()) return;
        const juce::Image::BitmapData src (frame, juce::Image::BitmapData::readOnly);

        float lum[W * H];
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const int sx = x * frame.getWidth() / W, sy = y * frame.getHeight() / H;
                const auto c = src.getPixelColour (sx, sy);
                lum[y * W + x] = 0.299f * c.getRed() + 0.587f * c.getGreen() + 0.114f * c.getBlue();
            }

        const float sens = sensitivity.load();
        const float thresh = 10.0f + (1.0f - sens) * 40.0f;
        double sx = 0, sy = 0; int count = 0;
        bool mask[W * H] {};
        if (hasPrev)
            for (int i = 0; i < W * H; ++i)
                if (std::abs (lum[i] - prev[i]) > thresh)
                {
                    mask[i] = true;
                    ++count;
                    sx += i % W; sy += i / W;
                }
        std::copy (lum, lum + W * H, prev);
        hasPrev = true;

        const float amount = juce::jlimit (0.0f, 1.0f, (float) count / (float) (W * H) * (5.0f + sens * 20.0f));
        motion.store (amount);
        if (count > 6)
        {
            // Mirror X so moving your hand to your right moves HAND X up, like a mirror.
            handX.store (1.0f - 2.0f * (float) (sx / count) / (float) (W - 1));
            handY.store (1.0f - 2.0f * (float) (sy / count) / (float) (H - 1));
        }

        if (! armed && amount < 0.08f) armed = true;
        if (armed && amount > 0.25f) { armed = false; triggerCount.fetch_add (1); waveCount.fetch_add (1); }

        // With the hand skeleton running, HAND X/Y follow the palm itself, and a pinch also triggers.
        if (hand.present.load())
        {
            handX.store (1.0f - 2.0f * hand.palmX.load());
            handY.store (1.0f - 2.0f * hand.palmY.load());
        }
        const int pc = hand.pinchCount.load();
        if (pc != pinchSeen) { pinchSeen = pc; triggerCount.fetch_add (1); }

        // Preview: mirrored grayscale with moving pixels tinted amber.
        juce::Image img (juce::Image::RGB, W, H, false);
        {
            juce::Image::BitmapData dst (img, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                {
                    const int i = y * W + (W - 1 - x);
                    const auto v = (juce::uint8) juce::jlimit (0.0f, 255.0f, lum[i] * 0.6f);
                    dst.setPixelColour (x, y, mask[i] ? juce::Colour (255, 180, 58) : juce::Colour (v, (juce::uint8) juce::jmin (255, v + 18), v));
                }
        }
        auto big = frame.rescaled (juce::jmin (480, frame.getWidth()), juce::jmin (360, frame.getHeight()), juce::Graphics::lowResamplingQuality);
        hand.submit (big);
        const juce::SpinLock::ScopedLockType sl (previewLock);
        preview = img;
        colour = big;
    }

private:
   #if JUCE_USE_CAMERA
    void imageReceived (const juce::Image& image) override { analyse (image); }
    std::unique_ptr<juce::CameraDevice> device;
   #endif
    float prev[W * H] {};
    bool hasPrev = false, armed = true;
    int pinchSeen = 0;
    juce::SpinLock previewLock;
    juce::Image preview, colour;
};
