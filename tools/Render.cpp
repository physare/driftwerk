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
    proc.setCurrentProgram (program);
    const double sr = 48000.0;
    const int block = 512;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    juce::AudioBuffer<float> all (2, (int) (sr * seconds));
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    float peak = 0; double rms = 0;
    for (int pos = 0; pos + block <= all.getNumSamples(); pos += block)
    {
        buf.clear();
        proc.processBlock (buf, midi);
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

    juce::File wav (outDir + "/render_" + juce::String (program) + ".wav");
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
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File png (outDir + "/editor_" + juce::String (program) + ".png");
        png.deleteFile();
        juce::PNGImageFormat pf;
        if (auto os = png.createOutputStream()) pf.writeImageToStream (img, *os);
        ed = nullptr;
    }
    return 0;
}
