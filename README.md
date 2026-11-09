# Driftwerk

An original, self-running modular synthesizer plugin (VST3) for slow, cosmic, Berlin-school
music: long drones, randomly evolving arpeggios, slow filter sweeps, tape echo and a huge hall.
The interface is a 90s-style modular rack with real drag-and-drop patch cables.

## Modules

| Row | Module | What it does |
|---|---|---|
| Top | CLOCK | Master clock. Follows Ableton's tempo and transport; free-runs at the host tempo when stopped. |
| Top | RANDOM ARP | Shift-register sequencer: a looping pattern of scale notes that slowly mutates. MUTATE = how often steps change, WALK = stepwise motion vs. leaps, DENSITY = how many steps play. |
| Top | VCO 1 / VCO 2 | Saw, pulse (with PWM) and sub outputs. PITCH input is 1 unit per octave. |
| Top | DRONE BANK | Six detuned, drifting saws tuned to a chord. Always sounding. |
| Top | NOISE / S&H | White noise and a slewed sample & hold, clocked from CLK /4 by default. |
| Top | LFO 1 / LFO 2 | Very slow LFOs (down to 0.005 Hz) with sine, triangle, S&H and drift shapes. |
| Bottom | ENV 1 / ENV 2 | ADSR envelopes triggered from a GATE input. |
| Bottom | MIXER, VCF 1/2, VCA 1/2 | 4-input mixer, 24 dB ladder filters with two CV inputs, linear VCAs. |
| Bottom | OUTPUT | Two inputs with level and pan, string ensemble, master volume. |
| Top | MONITOR / CAMERA | Output scope, plus webcam hand tracking. Turn CAM ON and patch HAND X, HAND Y (both -1..+1), MOTION (0..1) and TRIG (a gate on each sudden gesture) to anything. SENS sets how much movement counts, SMOOTH slows the outputs down. Windows and Mac only. |
| Bottom | TAPE ECHO / HALL | Tempo-synced ping-pong tape echo with wow, and a long FDN hall (tails up to ~40 s). |

## Patching

* Drag from any jack to another jack to connect a cable (output to input).
* Drag a plugged input to move that cable; drop it on empty space to remove it.
* Right-click a jack to pull all of its cables.
* RANDOM PATCH re-wires the modulation for happy accidents; PULL ALL clears everything.
* The patch is saved with your Ableton set and with presets.
* MIDI notes transpose the whole system (C3 = no transpose), but it plays on its own without any MIDI.

## Playing knobs with your hands

Right-click any knob and pick Hand X, Hand Y or Hand motion (or an LFO, S&H, ENV 2 or the arp's random output),
then pick a depth. Negative depths turn the knob the other way. An amber arc shows the range the knob can move
through and a bright dot shows where it is right now. Choose "Nothing" to go back to manual.
The CLOCK module's TUNE knob shifts the pitch of everything, so assigning it to Hand Y lets you bend the whole
system up and down. Hand control needs CAM ON in the camera module. Assignments are saved with your set.

## Presets

Drift Cathedral, Mirror Sequence, Black Tide, Pulse Engine, Hand Weather (camera: hand height sweeps the sequence filter, left/right sweeps the drone, a quick wave opens it up).

## Building

Every push builds the Windows VST3 on GitHub Actions (see the Actions tab, artifact `Driftwerk-Windows`).

Locally (needs CMake 3.22+ and Visual Studio 2022 or another C++17 compiler):

```
cmake -B build
cmake --build build --config Release --target Driftwerk_VST3
```

## Installing in Ableton (Windows)

1. Copy `Driftwerk.vst3` to `C:\Program Files\Common Files\VST3\`.
2. In Ableton: Options > Preferences > Plug-Ins, turn on "Use VST3 Plug-In System Folders", then Rescan.
3. Drag Driftwerk from Plug-Ins > Driftwerk onto a MIDI track and press play.
