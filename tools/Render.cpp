// Offline test tool: renders audio to WAV and the editor to PNG.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String outDir = argc > 1 ? argv[1] : ".";
    const int program = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const double seconds = argc > 3 ? juce::String (argv[3]).getDoubleValue() : 30.0;

    DriftwerkProcessor proc;

    // Camera tracker self-test: a bright square moving left to right, then a burst of motion.
    {
        CameraTracker t;
        for (int f = 0; f < 12; ++f)
        {
            juce::Image frame (juce::Image::RGB, 320, 240, true);
            juce::Graphics g (frame);
            g.fillAll (juce::Colours::black);
            g.setColour (juce::Colours::white);
            g.fillRect (20 + f * 22, 150 - f * 8, 50, 50);
            t.analyse (frame);
            std::printf ("cam frame %2d  x %+.2f  y %+.2f  motion %.2f  trig %d\n", f, t.handX.load(), t.handY.load(), t.motion.load(), t.triggerCount.load());
        }
        // Feed the same square to the processor's tracker so the editor preview shows something.
        for (int f = 0; f < 2; ++f)
        {
            juce::Image frame (juce::Image::RGB, 320, 240, true);
            juce::Graphics g (frame);
            g.fillAll (juce::Colour (40, 44, 40));
            g.setColour (juce::Colours::white);
            g.fillEllipse (60.0f + f * 30.0f, 70.0f, 70.0f, 90.0f);
            proc.camera.analyse (frame);
        }
    }
    proc.loadPreset (program);
    if (auto* env = std::getenv ("DW_SET"))
    {
        juce::StringArray kv; kv.addTokens (env, ",", "");
        for (auto& item : kv)
            if (auto* rp = proc.apvts.getParameter (item.upToFirstOccurrenceOf ("=", false, false)))
                rp->setValueNotifyingHost (rp->convertTo0to1 (item.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
        proc.syncGrainSource();   // no message loop runs here, so pick up grain.src directly
    }
    // MORPH: "preset:move:time": A = the program, B = that preset, then glide (move 1 A TO B,
    // 2 B TO A, 3 BACK + FORTH, 4 WANDER) over time seconds, starting from A.
    juce::StringArray morphShow;
    const bool morphing = std::getenv ("DW_MORPH") != nullptr;
    if (morphing)
    {
        juce::StringArray f; f.addTokens (std::getenv ("DW_MORPH"), ":", "");
        proc.storeMorph (0);
        std::vector<float> a;
        for (auto* rp : proc.paramList) a.push_back (rp->getValue());
        proc.loadPreset (f[0].getIntValue());
        proc.storeMorph (1);
        for (size_t k = 0; k < proc.paramList.size(); ++k)
            if (std::abs (proc.paramList[k]->getValue() - a[k]) > 1.0e-6f && ! DriftwerkProcessor::morphSkips (proc.paramList[k]->getParameterID()))
                morphShow.add (proc.paramList[k]->getParameterID());
        std::printf ("morph: %d knobs differ\n", morphShow.size());
        proc.morphTick (0.0);
        proc.apvts.getParameter ("morph.pos")->setValueNotifyingHost (0.0f);
        proc.morphTick (0.0);   // back to A
        auto* mv = proc.apvts.getParameter ("morph.move");
        mv->setValueNotifyingHost (mv->convertTo0to1 ((float) f[1].getIntValue()));
        auto* mt = proc.apvts.getParameter ("morph.time");
        mt->setValueNotifyingHost (mt->convertTo0to1 (f[2].getFloatValue()));
        proc.syncGrainSource();
    }
    if (auto* env = std::getenv ("DW_MOD"))   // knob modulation, as in presets: "drn.level<drift.fast@0.5;..."
    {
        proc.setModFromString (proc.getModString() + ";" + juce::String (env));
        std::printf ("mods: %s\n", proc.getModString().toRawUTF8());
    }
    if (auto* env = std::getenv ("DW_BREAK"))   // BREAK: load a drum loop
    {
        const bool ok = proc.loadBreak (juce::File (env));
        std::printf ("break: %s\n", ok ? juce::File (env).getFileName().toRawUTF8() : "failed to load");
    }
    if (auto* env = std::getenv ("DW_PSAMP"))   // PULSE: lane=path;lane=path one-shots
    {
        juce::StringArray items; items.addTokens (env, ";", "");
        for (auto& it : items)
        {
            const int lane = it.upToFirstOccurrenceOf ("=", false, false).getIntValue();
            const bool ok = proc.loadPulseSample (lane, juce::File (it.fromFirstOccurrenceOf ("=", false, false)));
            std::printf ("pulse sample %d: %s\n", lane, ok ? "loaded" : "failed");
        }
    }
    if (auto* env = std::getenv ("DW_DREAM"))   // DREAM: load a file for MY SOUND
    {
        const bool ok = proc.loadDreamSound (juce::File (env));
        std::printf ("dream sound: %s\n", ok ? juce::File (env).getFileName().toRawUTF8() : "failed to load");
    }
    if (auto* env = std::getenv ("DW_WORDS"))   // WORDS: dump the synthetic voice, and/or load a file or folder of speech
    {
        if (const auto* b = proc.wordsBankNow())
        {
            std::printf ("words: %s, %d words, %d phrases, %.1f s\n", b->name.c_str(), (int) b->words.size(), (int) b->phrases.size(),
                         (double) b->audio.size() / b->rate);
            juce::WavAudioFormat wf;
            juce::File f (outDir + "/words_synth.wav");
            f.deleteFile();
            if (auto os = f.createOutputStream())
                if (auto* w = wf.createWriterFor (os.get(), b->rate, 1, 16, {}, 0))
                {
                    os.release();
                    std::unique_ptr<juce::AudioFormatWriter> writer (w);
                    juce::AudioBuffer<float> buf (1, (int) b->audio.size());
                    for (int i = 0; i < buf.getNumSamples(); ++i) buf.setSample (0, i, b->audio[(size_t) i] / 32768.0f);
                    writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
                }
        }
        if (juce::String (env) != "1")
        {
            proc.loadWords (juce::StringArray::fromTokens (env, ";", ""));
            for (int k = 0; k < 600 && proc.wordsLoading.load(); ++k) juce::Thread::sleep (50);
            if (const auto* m = proc.myWords())
                std::printf ("my words: %s, %d words, %d phrases, %.1f s\n", m->name.c_str(), (int) m->words.size(), (int) m->phrases.size(),
                             (double) m->audio.size() / m->rate);
            else std::printf ("my words: none (%s)\n", proc.wordsError.toRawUTF8());
        }
    }
    if (auto* env = std::getenv ("DW_ACT"))   // press buttons: copy (EDIT THESE), new (NEW CHORDS), hits (NEW)
    {
        const juce::String a (env);
        if (a.contains ("copy")) proc.copyProgressionToMine();
        if (a.contains ("new")) proc.randomiseMyChords();
        if (a.contains ("hits")) proc.newHits();
        if (a.contains ("save"))   // MY PRESETS: save, change something, load it back
        {
            proc.setParam ("pulse.kit", 6.0f);
            const bool ok = proc.saveUserPreset ("Render Test");
            proc.setParam ("pulse.kit", 2.0f);
            proc.loadPreset (3);
            const auto files = DriftwerkProcessor::userPresets();
            bool loaded = false;
            for (auto& f : files) if (f.getFileNameWithoutExtension() == "Render Test") loaded = proc.loadUserPreset (f);
            std::printf ("my presets: saved %d, %d files, loaded %d as \"%s\", kit %d (want 6)\n", (int) ok, files.size(), (int) loaded,
                         proc.userPresetName.toRawUTF8(), (int) proc.apvts.getRawParameterValue ("pulse.kit")->load());
            DriftwerkProcessor::userPresetFolder().getChildFile ("Render Test.timewind").deleteFile();
        }
        if (a.contains ("morph"))   // MORPH: A and B come back with a set, and with a preset of yours only while moving
        {
            juce::MemoryBlock mb;
            proc.getStateInformation (mb);
            DriftwerkProcessor p2;
            p2.setStateInformation (mb.getData(), (int) mb.getSize());
            const int move = (int) proc.apvts.getRawParameterValue ("morph.move")->load();
            proc.saveUserPreset ("Render Morph");
            DriftwerkProcessor p3;
            p3.loadUserPreset (DriftwerkProcessor::userPresetFolder().getChildFile ("Render Morph.timewind"));
            std::printf ("morph state: A %d B %d  set copy: A %d B %d  my preset (move %d): A %d B %d\n", (int) proc.hasMorph (0), (int) proc.hasMorph (1),
                         (int) p2.hasMorph (0), (int) p2.hasMorph (1), move, (int) p3.hasMorph (0), (int) p3.hasMorph (1));
            DriftwerkProcessor::userPresetFolder().getChildFile ("Render Morph.timewind").deleteFile();
        }
        std::printf ("after %s: mode %d, %d chords\n", env, (int) proc.apvts.getRawParameterValue ("prog.mode")->load(),
                     (int) proc.apvts.getRawParameterValue ("prog.len")->load() + 2);
    }
    if (auto* env = std::getenv ("DW_HAND"))
    {
        juce::StringArray v; v.addTokens (env, ",", "");
        proc.camera.handX.store (v[0].getFloatValue()); proc.camera.handY.store (v[1].getFloatValue()); proc.camera.motion.store (v[2].getFloatValue());
    }
    const double sr = 48000.0;
    const int block = 512;
    proc.organism.manual = std::getenv ("DW_THREAD") == nullptr;   // the organism grows in step with the render, not on its own thread
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    juce::AudioBuffer<float> all (2, (int) (sr * seconds));
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    float peak = 0; double rms = 0;
    const bool trace = std::getenv ("DW_TRACE") != nullptr;   // print each chord change
    int lastSig = -1;
    double organismClock = 0.0;
    for (int pos = 0; pos + block <= all.getNumSamples(); pos += block)
    {
        organismClock += block / sr;
        while (organismClock >= 1.0 / 30.0) { organismClock -= 1.0 / 30.0; if (proc.organism.manual) proc.organism.tick(); }
        if (morphing)
        {
            proc.morphTick (block / sr);
            if (pos % (int) (sr * 5) < block)
            {
                juce::String line;
                for (int i = 0; i < juce::jmin (6, morphShow.size()); ++i)
                    line << morphShow[i] << " " << proc.apvts.getParameter (morphShow[i])->getCurrentValueAsText() << "  ";
                std::printf ("  %5.1fs  pos %.3f  %s\n", pos / sr, proc.apvts.getRawParameterValue ("morph.pos")->load(), line.toRawUTF8());
            }
        }
        buf.clear();
        proc.processBlock (buf, midi);
        if (trace)
        {
            const int sig = proc.progIndex.load() * 100000 + proc.progOffset.load() * 5000 + proc.chordIvMask.load();
            if (sig != lastSig)
            {
                lastSig = sig;
                const auto v = proc.getProgressionView();
                juce::String notes;
                for (int i = 0; i < proc.chordNoteCount.load(); ++i) notes << harm::noteName (proc.chordNotes[(size_t) i].load(), v.spell) << " ";
                std::printf ("  %6.2fs  #%d  %-10s %-5s  next %-10s  [%s]  %s\n", pos / sr, v.current, v.curName.toRawUTF8(), v.curNumeral.toRawUTF8(),
                             v.nextName.toRawUTF8(), v.keyName.toRawUTF8(), notes.toRawUTF8());
            }
        }
        for (int c = 0; c < 2; ++c)
        {
            all.copyFrom (c, pos, buf, c, 0, block);
            for (int i = 0; i < block; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) { std::printf ("NaN/inf at %d\n", pos + i); return 1; }
                peak = std::max (peak, std::abs (v)); rms += v * v;
            }
        }
    }
    rms = std::sqrt (rms / (2.0 * all.getNumSamples()));
    std::printf ("program %d peak %.3f rms %.4f\n", program, peak, rms);
    std::printf ("words played %d\n", proc.wordsPlayed.load());
    if (const auto* bl = proc.breakForDisplay())
        std::printf ("break %s: %.1f BPM, %d bars, %d slices, %d hits\n", bl->name.c_str(), bl->bpm, bl->bars, bl->steps, proc.breakHits.load());
    {
        juce::String hist;
        const int head = proc.bassHistHead.load();
        for (int i = 0; i < DriftwerkProcessor::bassHistSize; ++i)
        {
            const int v = proc.bassHist[(size_t) ((head + i) % DriftwerkProcessor::bassHistSize)].load();
            if (v < 0) hist << ". ";
            else hist << harm::pcName ((v & 255) % 12, true) << ((v & 255) / 12 - 1) << ((v & 256) ? "!" : "") << ((v & 512) ? "~" : "") << " ";
        }
        std::printf ("bass notes %d: %s\n", proc.bassNotes.load(), hist.toRawUTF8());
        juce::String lay;
        for (int k = 0; k < DriftwerkProcessor::numLayers; ++k)
            lay << DriftwerkProcessor::layerDefs[k].name << " " << juce::String (proc.layerMeter[(size_t) k].load(), 3) << "  ";
        std::printf ("layers: %s\n", lay.toRawUTF8());
    }
    std::printf ("slime notes %d (ear events %d)  pulse hits %d %d %d %d %d  texture events %d  memory %.1fs\n", proc.slimeNotes.load(),
                 proc.organism.totalHits.load(), proc.pulseLaneHits[0].load(), proc.pulseLaneHits[1].load(), proc.pulseLaneHits[2].load(),
                 proc.pulseLaneHits[3].load(), proc.pulseLaneHits[4].load(), proc.texEvents.load(), proc.memSeconds.load());

    const juce::String tag = std::getenv ("DW_TAG") ? juce::String (std::getenv ("DW_TAG")) : juce::String (program);
    juce::File wav (outDir + "/render_" + tag + ".wav");
    wav.deleteFile();
    juce::WavAudioFormat fmt;
    if (auto os = wav.createOutputStream())
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (fmt.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
        {
            os.release();
            w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
        }

    if (program == 0 || argc > 4)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        if (auto* env = std::getenv ("DW_HANDIMG"))
        {
            // Run the real hand skeleton on a still photo so the views show it.
            const auto photo = juce::ImageFileFormat::loadFrom (juce::File (juce::String (env)));
            proc.camera.hand.start();
            for (int k = 0; k < 8; ++k) { proc.camera.analyse (photo); juce::Thread::sleep (150); }
            std::printf ("hand %s  pinch %.2f spread %.2f fist %.2f tilt %.2f  %.1f ms  (%s)\n", proc.camera.hand.present.load() ? "found" : "none",
                         proc.camera.hand.pinch.load(), proc.camera.hand.spread.load(), proc.camera.hand.fist.load(), proc.camera.hand.tilt.load(),
                         proc.camera.hand.inferenceMs.load(), proc.camera.hand.getStatus().toRawUTF8());
            juce::AudioBuffer<float> tmp (2, 512); juce::MidiBuffer mb;
            for (int k = 0; k < 400; ++k) proc.processBlock (tmp, mb);   // let the gesture outputs settle
        }
        if (auto* de = dynamic_cast<DriftwerkEditor*> (ed.get()))
        {
            de->view.advanceSlime (600);
            if (std::getenv ("DW_FULL")) de->view.showFullCamera();
            if (auto* t = std::getenv ("DW_TAB")) de->view.showPerformTab (juce::String (t).getIntValue());
            if (std::getenv ("DW_MORPHUI")) de->view.showMorph (true);
            de->refreshNow();
        }
        ed->setSize (MainView::designW, MainView::designH);
        if (auto* de = dynamic_cast<DriftwerkEditor*> (ed.get()))
            for (int k = 0; k < 20; ++k) de->refreshNow();          // let the animations settle
        if (auto* env = std::getenv ("DW_FS"))
            if (auto* de = dynamic_cast<DriftwerkEditor*> (ed.get()))
            {
                // FULL SCREEN in a host: the rack moves to its own window and back again.
                DriftwerkEditor::fullScreenOnDesktop = false;
                de->setFull (true);
                auto* win = de->view.getParentComponent();
                std::printf ("full screen: window %s, %d x %d, on desktop %d, editor holds view %d\n", win != nullptr ? win->getName().toRawUTF8() : "none",
                             win != nullptr ? win->getWidth() : 0, win != nullptr ? win->getHeight() : 0, win != nullptr && win->isOnDesktop(),
                             (int) (win == de));
                if (win != nullptr && win != de)
                {
                    const juce::String size (env);   // DW_FS=1920x1200 picks the screen size (default 1920 x 1080)
                    const int sw = size.containsChar ('x') ? size.upToFirstOccurrenceOf ("x", false, false).getIntValue() : 1920;
                    const int sh = size.containsChar ('x') ? size.fromFirstOccurrenceOf ("x", false, false).getIntValue() : 1080;
                    win->setBounds (0, 0, sw, sh);
                    juce::PNGImageFormat pf;
                    juce::File f (outDir + "/fullscreen_" + juce::String (env) + ".png");
                    f.deleteFile();
                    if (auto os = f.createOutputStream()) pf.writeImageToStream (win->createComponentSnapshot (win->getLocalBounds(), true, 1.0f), *os);
                    auto edImg = ed->createComponentSnapshot (ed->getLocalBounds(), true, 0.5f);
                    juce::File f2 (outDir + "/fullscreen_editor.png");
                    f2.deleteFile();
                    if (auto os = f2.createOutputStream()) pf.writeImageToStream (edImg, *os);
                }
                de->setFull (false);
                std::printf ("back: editor holds view %d, full %d\n", (int) (de->view.getParentComponent() == de), (int) de->isFull());
            }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File png (outDir + "/editor_" + tag + ".png");
        png.deleteFile();
        juce::PNGImageFormat pf;
        if (auto os = png.createOutputStream()) pf.writeImageToStream (img, *os);
        ed = nullptr;
    }
    return 0;
}
