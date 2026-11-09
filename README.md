# Timewind

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

## Ambient engine (new)

* **DREAM**: five soft voices on loops of unrelated lengths (17 to 32 seconds), so they drift in and out of phase, Eno style. WARP adds warped-cassette detune.
* **GRAIN**: drop any sound file (wav, aiff, flac, ogg, mp3) onto the waveform, or click LOAD SOUND. POSITION, SPRAY, SIZE, DENSITY, PITCH, REVERSE and SCAN pull it apart. It starts with a built-in glass choir.
* **TAPE**: wow, age, hiss and crush on the whole mix, for that worn Boards-of-Canada warmth.
* **HALL SHIMMER**: an octave-up cloud feeding back into the reverb.
* DREAM and GRAIN play straight into the output until you patch their OUT jacks somewhere else.

## Hand Play

Turn on HAND > CAM and HAND PLAY > PLAY. Then:

* Hand left to right moves between three worlds: **airport** (calm loops and shimmer), **tape** (warped cassette and grains) and **cosmic** (the sequencer and drone).
* Hand height makes it darker or brighter and drier or wetter.
* Moving more makes it denser and more chaotic.
* A quick wave rolls a **new scene**: a random key, scale, chords, rates, grain pitch and patch. The NEW SCENE button does the same.

DEPTH sets how much the hand overrides the knobs. The slime mold at the top grows toward your hand and pulses with the sound.

### Hand skeleton

On Windows the camera also runs a 21-joint hand skeleton (MediaPipe's hand models in ONNX Runtime, about 11 ms per frame).
You see the bones drawn over your hand in the HAND preview and the full-screen camera, and four more jacks appear:

* **PINCH**: thumb and index fingertips touching (0..1). Closing a pinch also fires TRIG.
* **SPREAD**: fingers fanned open (0..1).
* **FIST**: fingers curled (0..1).
* **TILT**: hand leaning left or right (-1..+1).

In Hand Play, pinch adds shimmer and grains, spreading opens up the space, a fist hushes the arp and darkens, and tilt scans the grains.
They are also mod sources: right-click any knob.

## Performance panel (top middle)

The panel over the slime mold has five tabs:

* **SEQUENCE**: the arp's 16 steps. **HOLD** freezes the pattern so it repeats exactly. Click a step to switch it on or off, drag up and down to set its note (drag across steps to draw a line). **REC NOTES** lets you play a MIDI keyboard into the running sequence. **NEW** writes a fresh singable pattern, **CLEAR** empties it, **< >** shift it. **SAVE** then a number stores the sequence in that slot; a number on its own recalls it. Slots are shared by every Timewind on your computer.
* **ARP SOUND**: octave, glide, soft attack, pluck length, brightness, resonance, drive, detune, echo and level of the arp voice.
* **DRONE**: chord, octave, detune, drift, level, tone, saw-to-soft wave, breath (each voice swells on its own slow cycle), air (breathy noise) and sub.
* **HARMONY**: chord progressions. PEACEFUL, DREAMY and DRIFTING move pads, drone and arp together through chords that stay in key, every 1 to 16 bars. CREEPY slides between outside chords. CHORD GLIDE sets how slowly they slide. NEW SCENE KEY decides whether a new scene keeps the key, moves to a nearby key or jumps anywhere.
* **ORGANISM**: just the slime mold.

**FULL SCREEN** (or F11) fills your screen with Timewind; click it again or press Esc to go back. **CAMERA** opens the full-screen camera stage.

**REC** in the header records everything Timewind plays to a WAV file in Music\Timewind. **RECORDINGS** opens that folder.

## Random Arp

* **ON** turns the plucks on and off. The pads and drone keep going.
* **HARMONY** chooses which notes it can play. **PADS** (the default) uses only the notes of the Dream chord, so it always fits the pads. **SCALE** plays the whole SCALE. **ROOTS** plays only the root and fifth.
* DENSITY sets how many steps play, MUTATE how often notes change, WALK how far they jump, and GATE how long each pluck lasts.

## Playing knobs with your hands

Right-click any knob and pick Hand X, Hand Y or Hand motion (or an LFO, S&H, ENV 2 or the arp's random output),
then pick a depth. Negative depths turn the knob the other way. An amber arc shows the range the knob can move
through and a bright dot shows where it is right now. Choose "Nothing" to go back to manual.
The CLOCK module's TUNE knob shifts the pitch of everything, so assigning it to Hand Y lets you bend the whole
system up and down. Hand control needs CAM ON in the camera module. Assignments are saved with your set.

## Presets

The preset menu is grouped from calm to strange. Every preset stays in key with itself, so you can move from a soundbath to plucks to something weird without clashes.

* **Soundbath** (no plucks, all breath): Drift Cathedral, Singing Bowls, Low Tide Breathing, Cathedral Ghosts, Airport Tapes.
* **Plucks + sequences**: Mirror Sequence, Berlin Night Drive, Glass Harp, Kalimba Rain, Toy Piano Ghost, Black Tide, Pulse Engine.
* **Weird**: Warped Cassette, Insect Choir, Broken Radio Prayer, Gravity Well, Clockwork Jellyfish.
* **Hands (camera)**: Hand Garden, Hand Weather, Puppet Strings (each gesture plays the next note), Theremin Fog (hand height is pitch, pinch to sound it).

## Building

Every push builds the Windows VST3 on GitHub Actions (see the Actions tab, artifact `Timewind-Windows`).

Locally (needs CMake 3.22+ and Visual Studio 2022 or another C++17 compiler):

```
cmake -B build
cmake --build build --config Release --target Timewind_VST3
```

## Automatic updates (Windows)

Every push to `main` builds the plugin and publishes it as the `latest` GitHub release.
To have your PC install new builds by itself, open PowerShell **as Administrator** in this repo and run:

```
gh auth login   # only if the GitHub CLI isn't signed in yet
powershell -ExecutionPolicy Bypass -File tools\windows\install-timewind.ps1
```

This installs Timewind, links it into `C:\Program Files\Common Files\VST3`, and adds a "Timewind Updater"
scheduled task that checks every 30 minutes and at logon. It waits while Ableton is open, since Windows locks
plugins that are in use, so new builds land the next time Ableton is closed. Log: `%LOCALAPPDATA%\Timewind\update.log`.

## Installing in Ableton manually (Windows)

1. Copy `Timewind.vst3` to `C:\Program Files\Common Files\VST3\`.
2. In Ableton: Options > Preferences > Plug-Ins, turn on "Use VST3 Plug-In System Folders", then Rescan.
3. Drag Timewind from Plug-Ins > Timewind onto a MIDI track and press play.
