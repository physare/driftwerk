// Offline check of the hand skeleton: runs HandTracker on still images and prints
// the 21 joints plus gesture features. Usage: TimewindHandTest image1.jpg [image2 ...]
// Set TIMEWIND_RESOURCES to the folder with the ONNX models and the ONNX Runtime library.
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/HandTracker.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    for (int a = 1; a < argc; ++a)
    {
        HandTracker ht;
        const auto img = juce::ImageFileFormat::loadFrom (juce::File (juce::String (argv[a])));
        const bool found = ht.processStill (img);
        std::printf ("%s %s %s\n", argv[a], found ? "HAND" : "none", ht.status.toRawUTF8());
        if (! found) continue;
        const auto pose = ht.getPose();
        std::printf ("J");
        for (auto& p : pose.pts) std::printf (" %.4f %.4f", p.x, p.y);
        std::printf ("\nF pinch %.2f spread %.2f fist %.2f tilt %.2f\n", ht.pinch.load(), ht.spread.load(), ht.fist.load(), ht.tilt.load());
    }
    return 0;
}
