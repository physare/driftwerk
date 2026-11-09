// Hand skeleton tracker: MediaPipe's palm detector + 21-point hand landmark model
// (Apache 2.0, converted to ONNX) running in ONNX Runtime on a background thread.
// ONNX Runtime is loaded at run time from the plugin bundle, so if it or the models
// are missing the plugin still works with plain motion tracking.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <array>
#include <cmath>
#include <vector>
#include "../third_party/onnxruntime/onnxruntime_c_api.h"

class HandTracker : private juce::Thread
{
public:
    static constexpr int NumJoints = 21;
    struct Pose
    {
        bool present = false;
        std::array<juce::Point<float>, NumJoints> pts {}; // normalised 0..1 in the camera image (not mirrored)
    };

    HandTracker() : juce::Thread ("Timewind hand tracker") {}
    ~HandTracker() override { shutdown(); }

    // Message thread. Returns true when the models are loaded and the thread is running.
    bool start()
    {
        if (ready) return true;
        if (triedLoad) return false;
        triedLoad = true;
        if (! load()) return false;
        ready = true;
        startThread (juce::Thread::Priority::low);
        return true;
    }

    void shutdown()
    {
        if (isThreadRunning()) { signalThreadShouldExit(); wake.signal(); stopThread (4000); }
        ready = false;
        // Sessions belong to this instance. The ONNX Runtime library and its environment are
        // shared by every Timewind in the process and deliberately never unloaded, because
        // unloading the runtime while the host keeps running is a known way to crash.
        if (api != nullptr)
        {
            if (palm) api->ReleaseSession (palm);
            if (marks) api->ReleaseSession (marks);
            if (mem) api->ReleaseMemoryInfo (mem);
        }
        palm = marks = nullptr; mem = nullptr;
    }

    bool isReady() const { return ready; }

    // Lets the user switch the skeleton off (it then uses no CPU at all).
    std::atomic<bool> enabled { true };

    juce::String getStatus() const { const juce::SpinLock::ScopedLockType sl (statusLock); return status; }

    // Camera thread: hand over the newest frame; older pending frames are dropped.
    void submit (const juce::Image& frame)
    {
        if (! ready || ! enabled.load() || ! frame.isValid()) return;
        const int w = frame.getWidth(), h = frame.getHeight();
        {
            const juce::SpinLock::ScopedLockType sl (frameLock);
            pendingW = w; pendingH = h;
            pending.resize ((size_t) (w * h * 3));
            const juce::Image::BitmapData bd (frame, juce::Image::BitmapData::readOnly);
            for (int y = 0; y < h; ++y)
            {
                auto* dst = pending.data() + (size_t) (y * w * 3);
                for (int x = 0; x < w; ++x)
                {
                    const auto c = bd.getPixelColour (x, y);
                    dst[x * 3] = c.getRed(); dst[x * 3 + 1] = c.getGreen(); dst[x * 3 + 2] = c.getBlue();
                }
            }
            hasPending = true;
        }
        wake.signal();
    }

    Pose getPose() const { const juce::SpinLock::ScopedLockType sl (poseLock); return pose; }

    // Gesture features, 0..1 (tilt -1..1). Smoothed lightly.
    std::atomic<bool> present { false };
    std::atomic<float> palmX { 0.5f }, palmY { 0.5f };   // normalised image coords
    std::atomic<float> pinch { 0.0f }, spread { 0.0f }, fist { 0.0f }, tilt { 0.0f };
    std::atomic<int> pinchCount { 0 };
    std::atomic<float> inferenceMs { 0.0f };

    // Exposed for the offline test: run detection on a still image.
    bool processStill (const juce::Image& img)
    {
        if (! ready)
        {
            if (triedLoad) return false;
            triedLoad = true;
            ready = load();
            if (! ready) return false;
        }
        tracking = false;
        for (int k = 0; k < 3; ++k) { submitDirect (img); }   // detect, then refine with the tracked crop twice
        return present.load();
    }

private:
    //==========================================================================
    bool load()
    {
        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        juce::Array<juce::File> dirs {
            exe.getParentDirectory().getParentDirectory().getChildFile ("Resources"), // VST3 bundle
            exe.getParentDirectory().getChildFile ("Resources"),
            exe.getParentDirectory(),
        };
        if (auto* env_ = std::getenv ("TIMEWIND_RESOURCES")) dirs.insert (0, juce::File (juce::String (env_)));

       #if JUCE_WINDOWS
        const char* libName = "tw_onnxruntime.dll";
       #elif JUCE_MAC
        const char* libName = "libonnxruntime.dylib";
       #else
        const char* libName = "libonnxruntime.so";
       #endif
        juce::File dir;
        for (auto& d : dirs)
            if (d.getChildFile ("hand_landmarks_detector.onnx").existsAsFile() && d.getChildFile (libName).existsAsFile()) { dir = d; break; }
        if (dir == juce::File()) { setStatus ("HAND MODEL NOT FOUND"); return false; }

        auto& rt = runtime();
        if (rt.api == nullptr && ! rt.failed)
        {
            // Loaded once per process and kept (never FreeLibrary'd).
            rt.failed = true;
            auto* lib = new juce::DynamicLibrary();
            if (! lib->open (dir.getChildFile (libName).getFullPathName())) { setStatus ("HAND ENGINE WON'T LOAD"); return false; }
            using GetBase = const OrtApiBase* (ORT_API_CALL*) ();
            auto getBase = (GetBase) lib->getFunction ("OrtGetApiBase");
            if (getBase == nullptr) { setStatus ("HAND ENGINE BROKEN"); return false; }
            rt.api = getBase()->GetApi (ORT_API_VERSION);
            if (rt.api == nullptr) { setStatus ("HAND ENGINE TOO OLD"); return false; }
            api = rt.api;
            if (! ok (api->CreateEnv (ORT_LOGGING_LEVEL_FATAL, "timewind", &rt.env))) { rt.api = nullptr; return false; }
            rt.failed = false;
        }
        if (rt.api == nullptr || rt.env == nullptr) { setStatus ("HAND ENGINE UNAVAILABLE"); return false; }
        api = rt.api;
        env = rt.env;
        OrtSessionOptions* so = nullptr;
        if (! ok (api->CreateSessionOptions (&so))) return false;
        ign (api->SetIntraOpNumThreads (so, 1));   // stay light next to the audio thread
        ign (api->SetInterOpNumThreads (so, 1));
        ign (api->SetSessionGraphOptimizationLevel (so, ORT_ENABLE_ALL));
        const bool s1 = makeSession (dir.getChildFile ("hand_detector.onnx"), so, palm);
        const bool s2 = s1 && makeSession (dir.getChildFile ("hand_landmarks_detector.onnx"), so, marks);
        api->ReleaseSessionOptions (so);
        if (! s2) return false;
        if (! ok (api->CreateCpuMemoryInfo (OrtArenaAllocator, OrtMemTypeDefault, &mem))) return false;
        buildAnchors();
        setStatus ("HAND SKELETON READY");
        return true;
    }

    bool makeSession (const juce::File& f, OrtSessionOptions* so, OrtSession*& out)
    {
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb)) { setStatus ("HAND MODEL UNREADABLE"); return false; }
        return ok (api->CreateSessionFromArray (env, mb.getData(), mb.getSize(), so, &out));
    }

    bool ok (OrtStatus* st)
    {
        if (st == nullptr) return true;
        setStatus ("HAND ENGINE: " + juce::String (api->GetErrorMessage (st)).substring (0, 60).toUpperCase());
        api->ReleaseStatus (st);
        return false;
    }

    void setStatus (const juce::String& s) { const juce::SpinLock::ScopedLockType sl (statusLock); status = s; }

    struct Runtime { const OrtApi* api = nullptr; OrtEnv* env = nullptr; bool failed = false; };
    static Runtime& runtime() { static Runtime r; return r; } // shared by every instance, set up on the message thread

    // Runs one model with a single NHWC float input; copies every output into outs.
    bool runModel (OrtSession* s, int size, std::vector<std::vector<float>>& outs, int numOuts)
    {
        const int64_t shape[] { 1, size, size, 3 };
        OrtValue* input = nullptr;
        if (! ok (api->CreateTensorWithDataAsOrtValue (mem, tensor.data(), tensor.size() * sizeof (float), shape, 4,
                                                       ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input))) return false;
        const char* inName = "input_1";
        const char* names4[] { "Identity", "Identity_1", "Identity_2", "Identity_3" };
        std::vector<OrtValue*> outVals ((size_t) numOuts, nullptr);
        const bool good = ok (api->Run (s, nullptr, &inName, (const OrtValue* const*) &input, 1, names4, (size_t) numOuts, outVals.data()));
        api->ReleaseValue (input);
        if (! good)
        {
            for (auto* v : outVals) if (v != nullptr) api->ReleaseValue (v);
            if (++failures > 20) { enabled.store (false); setStatus ("HAND SKELETON OFF (ENGINE ERRORS)"); }
            return false;
        }
        failures = 0;
        outs.resize ((size_t) numOuts);
        for (int k = 0; k < numOuts; ++k)
        {
            float* data = nullptr;
            OrtTensorTypeAndShapeInfo* info = nullptr;
            size_t n = 0;
            ign (api->GetTensorMutableData (outVals[(size_t) k], (void**) &data));
            ign (api->GetTensorTypeAndShape (outVals[(size_t) k], &info));
            ign (api->GetTensorShapeElementCount (info, &n));
            api->ReleaseTensorTypeAndShapeInfo (info);
            outs[(size_t) k].assign (data, data + n);
            api->ReleaseValue (outVals[(size_t) k]);
        }
        return true;
    }

    //==========================================================================
    // A rotated square crop of the image: centre and side in pixels, angle in radians.
    // Angle 0 is upright; the crop's "up" points along (sin a, -cos a) in the image.
    struct V3
    {
        float x, y, z;
        V3 operator- (V3 o) const { return { x - o.x, y - o.y, z - o.z }; }
        float operator* (V3 o) const { return x * o.x + y * o.y + z * o.z; }
        float length() const { return std::sqrt (x * x + y * y + z * z); }
    };
    void ign (OrtStatus* st) { if (st != nullptr) api->ReleaseStatus (st); }

    struct Roi { float cx = 0, cy = 0, size = 0, angle = 0; };

    juce::Point<float> roiToImage (const Roi& r, float u, float v) const // u, v in 0..1 across the crop
    {
        const float lx = (u - 0.5f) * r.size, ly = (v - 0.5f) * r.size;
        const float c = std::cos (r.angle), s = std::sin (r.angle);
        return { r.cx + lx * c - ly * s, r.cy + lx * s + ly * c };
    }

    void sampleCrop (const Roi& r, int n)
    {
        tensor.resize ((size_t) (n * n * 3));
        const float c = std::cos (r.angle), s = std::sin (r.angle), step = r.size / (float) n;
        for (int v = 0; v < n; ++v)
            for (int u = 0; u < n; ++u)
            {
                const float lx = ((float) u + 0.5f) * step - r.size * 0.5f, ly = ((float) v + 0.5f) * step - r.size * 0.5f;
                const float x = r.cx + lx * c - ly * s - 0.5f, y = r.cy + lx * s + ly * c - 0.5f;
                float* o = tensor.data() + (size_t) ((v * n + u) * 3);
                const int x0 = (int) std::floor (x), y0 = (int) std::floor (y);
                const float fx = x - (float) x0, fy = y - (float) y0;
                for (int ch = 0; ch < 3; ++ch)
                {
                    auto px = [&] (int xx, int yy) -> float
                    {
                        if (xx < 0 || yy < 0 || xx >= imgW || yy >= imgH) return 0.0f;
                        return (float) img[(size_t) ((yy * imgW + xx) * 3 + ch)];
                    };
                    const float top = px (x0, y0) + (px (x0 + 1, y0) - px (x0, y0)) * fx;
                    const float bot = px (x0, y0 + 1) + (px (x0 + 1, y0 + 1) - px (x0, y0 + 1)) * fx;
                    o[ch] = (top + (bot - top) * fy) / 255.0f;
                }
            }
    }

    void buildAnchors()
    {
        anchors.clear();
        // SSD anchors for the 192x192 palm model: stride 8 (2 per cell), then stride 16 (6 per cell).
        const int strides[] { 8, 16 }, perCell[] { 2, 6 };
        for (int l = 0; l < 2; ++l)
        {
            const int fm = 192 / strides[l];
            for (int y = 0; y < fm; ++y)
                for (int x = 0; x < fm; ++x)
                    for (int k = 0; k < perCell[l]; ++k)
                        anchors.push_back ({ ((float) x + 0.5f) / (float) fm, ((float) y + 0.5f) / (float) fm });
        }
    }

    static float angleFrom (juce::Point<float> a, juce::Point<float> b) // crop "up" points from a to b
    {
        return std::atan2 (b.x - a.x, -(b.y - a.y));
    }

    bool detectPalm (Roi& out)
    {
        Roi full { imgW * 0.5f, imgH * 0.5f, (float) juce::jmax (imgW, imgH), 0.0f };
        sampleCrop (full, 192);
        std::vector<std::vector<float>> o;
        if (! runModel (palm, 192, o, 2)) return false;
        const auto& boxes = o[0]; const auto& scores = o[1];
        int best = -1; float bestScore = 0.5f;
        for (size_t i = 0; i < anchors.size() && i < scores.size(); ++i)
        {
            const float sc = 1.0f / (1.0f + std::exp (-juce::jlimit (-100.0f, 100.0f, scores[i])));
            if (sc > bestScore) { bestScore = sc; best = (int) i; }
        }
        if (best < 0) return false;
        const float* b = boxes.data() + (size_t) best * 18;
        const auto& an = anchors[(size_t) best];
        auto toImg = [&] (float nx, float ny) { return roiToImage (full, nx, ny); };
        const auto centre = toImg (b[0] / 192.0f + an.x, b[1] / 192.0f + an.y);
        const float bw = b[2] / 192.0f * full.size, bh = b[3] / 192.0f * full.size;
        const auto wrist = toImg (b[4] / 192.0f + an.x, b[5] / 192.0f + an.y);
        const auto midMcp = toImg (b[8] / 192.0f + an.x, b[9] / 192.0f + an.y);
        out.angle = angleFrom (wrist, midMcp);
        const float side = juce::jmax (bw, bh);
        // Shift half a box toward the fingers, then grow to cover the whole hand.
        out.cx = centre.x + 0.5f * bh * std::sin (out.angle);
        out.cy = centre.y - 0.5f * bh * std::cos (out.angle);
        out.size = side * 2.6f;
        return true;
    }

    bool landmarks (const Roi& r, std::array<juce::Point<float>, NumJoints>& pts, std::array<float, 63>& world, float& presence)
    {
        sampleCrop (r, 224);
        std::vector<std::vector<float>> o;
        if (! runModel (marks, 224, o, 4)) return false;
        presence = o[1].empty() ? 0.0f : o[1][0];
        for (int j = 0; j < NumJoints; ++j)
            pts[(size_t) j] = roiToImage (r, o[0][(size_t) j * 3] / 224.0f, o[0][(size_t) j * 3 + 1] / 224.0f);
        for (size_t k = 0; k < 63 && k < o[3].size(); ++k) world[k] = o[3][k];
        return true;
    }

    Roi roiFromLandmarks (const std::array<juce::Point<float>, NumJoints>& p) const
    {
        const auto mid = ((p[5] + p[13]) * 0.5f + p[9]) * 0.5f;
        Roi r;
        r.angle = angleFrom (p[0], mid);
        const float c = std::cos (r.angle), s = std::sin (r.angle);
        float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
        for (int j : { 0, 1, 2, 3, 5, 6, 9, 10, 13, 14, 17, 18 })
        {
            const float lx = p[(size_t) j].x * c + p[(size_t) j].y * s, ly = -p[(size_t) j].x * s + p[(size_t) j].y * c;
            x0 = juce::jmin (x0, lx); x1 = juce::jmax (x1, lx); y0 = juce::jmin (y0, ly); y1 = juce::jmax (y1, ly);
        }
        const float lcx = (x0 + x1) * 0.5f, lcy = (y0 + y1) * 0.5f, w = x1 - x0, h = y1 - y0;
        r.cx = lcx * c - lcy * s; r.cy = lcx * s + lcy * c;
        r.cx += 0.1f * h * std::sin (r.angle);
        r.cy -= 0.1f * h * std::cos (r.angle);
        r.size = juce::jmax (w, h) * 2.0f;
        return r;
    }

    //==========================================================================
    void features (const std::array<juce::Point<float>, NumJoints>& p, const std::array<float, 63>& w)
    {
        auto P = [&] (int j) { return V3 { w[(size_t) j * 3], w[(size_t) j * 3 + 1], w[(size_t) j * 3 + 2] }; };
        auto dist = [] (V3 a, V3 b) { return (a - b).length(); };
        const float palmSize = juce::jmax (1.0e-4f, dist (P (0), P (9)));

        const float pinchRaw = 1.0f - juce::jlimit (0.0f, 1.0f, (dist (P (4), P (8)) / palmSize - 0.15f) / 0.75f);

        // Curl per finger: 0 straight, 1 folded (tip close to the knuckle relative to finger length).
        float curlSum = 0.0f;
        const int fingers[4][4] { { 5, 6, 7, 8 }, { 9, 10, 11, 12 }, { 13, 14, 15, 16 }, { 17, 18, 19, 20 } };
        for (auto& f : fingers)
        {
            const float len = dist (P (f[0]), P (f[1])) + dist (P (f[1]), P (f[2])) + dist (P (f[2]), P (f[3]));
            const float reach = dist (P (f[0]), P (f[3])) / juce::jmax (1.0e-4f, len);
            curlSum += juce::jlimit (0.0f, 1.0f, (0.95f - reach) / 0.65f);
        }
        const float fistRaw = curlSum / 4.0f;

        // Spread: angle between neighbouring fingers (knuckle to tip), averaged.
        float ang = 0.0f;
        for (int k = 0; k < 3; ++k)
        {
            auto a = P (fingers[k][3]) - P (fingers[k][0]), b = P (fingers[k + 1][3]) - P (fingers[k + 1][0]);
            const float d = (a * b) / juce::jmax (1.0e-6f, a.length() * b.length()); // dot product
            ang += std::acos (juce::jlimit (-1.0f, 1.0f, d));
        }
        const float spreadRaw = juce::jlimit (0.0f, 1.0f, (ang / 3.0f - 0.08f) / 0.35f) * (1.0f - fistRaw);

        const float roll = angleFrom (p[0], p[9]);
        const float tiltRaw = juce::jlimit (-1.0f, 1.0f, -roll / 1.2f); // mirrored: lean right = positive

        const float k = 0.5f;
        auto smooth = [k] (std::atomic<float>& a, float v) { a.store (a.load() + (v - a.load()) * k); };
        smooth (pinch, pinchRaw); smooth (spread, spreadRaw); smooth (fist, fistRaw); smooth (tilt, tiltRaw);
        const auto palmC = (p[0] + p[5] + p[9] + p[13] + p[17]) / 5.0f;
        smooth (palmX, palmC.x / (float) imgW); smooth (palmY, palmC.y / (float) imgH);

        if (! pinchArmed && pinch.load() < 0.35f) pinchArmed = true;
        if (pinchArmed && pinch.load() > 0.8f) { pinchArmed = false; pinchCount.fetch_add (1); }
    }

    void track()
    {
        Roi r;
        if (tracking) r = lastRoi;
        else if (! detectPalm (r)) { setAbsent(); return; }

        std::array<juce::Point<float>, NumJoints> pts;
        std::array<float, 63> world {};
        float presence = 0.0f;
        if (! landmarks (r, pts, world, presence) || presence < 0.5f)
        {
            tracking = false;
            setAbsent();
            return;
        }
        lastRoi = roiFromLandmarks (pts);
        tracking = true;
        features (pts, world);
        Pose ps; ps.present = true;
        for (int j = 0; j < NumJoints; ++j) ps.pts[(size_t) j] = { pts[(size_t) j].x / (float) imgW, pts[(size_t) j].y / (float) imgH };
        { const juce::SpinLock::ScopedLockType sl (poseLock); pose = ps; }
        present.store (true);
    }

    void setAbsent()
    {
        if (++missCount < 3 && present.load()) return; // ride over a dropped frame
        missCount = 0;
        present.store (false);
        const juce::SpinLock::ScopedLockType sl (poseLock);
        pose.present = false;
    }

    void submitDirect (const juce::Image& frame)
    {
        submit (frame);
        takePending();
        track();
    }

    bool takePending()
    {
        const juce::SpinLock::ScopedLockType sl (frameLock);
        if (! hasPending) return false;
        std::swap (img, pending);
        imgW = pendingW; imgH = pendingH;
        hasPending = false;
        return true;
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            wake.wait (200);
            if (threadShouldExit()) break;
            if (! takePending()) continue;
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            if (! enabled.load()) { setAbsent(); continue; }
            try { track(); } catch (...) { enabled.store (false); setStatus ("HAND SKELETON OFF (ERROR)"); }
            inferenceMs.store ((float) (juce::Time::getMillisecondCounterHiRes() - t0));
        }
    }

    const OrtApi* api = nullptr;
    OrtEnv* env = nullptr;
    OrtSession* palm = nullptr;
    OrtSession* marks = nullptr;
    OrtMemoryInfo* mem = nullptr;
    std::atomic<bool> ready { false };
    bool triedLoad = false;
    int failures = 0;
    mutable juce::SpinLock statusLock;
    juce::String status;

    juce::WaitableEvent wake;
    juce::SpinLock frameLock, poseLock;
    std::vector<juce::uint8> pending, img;
    int pendingW = 0, pendingH = 0, imgW = 0, imgH = 0;
    bool hasPending = false;

    std::vector<float> tensor;
    std::vector<juce::Point<float>> anchors;
    Roi lastRoi;
    bool tracking = false, pinchArmed = true;
    int missCount = 0;
    Pose pose;
};
