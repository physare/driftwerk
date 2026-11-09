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
* **GRAIN**: pick one of thirteen built-in sounds from the menu (glass choir, bowed metal, tape piano, vox pad, music box, wind organ, string dust, tuned rain, kalimba, singing bowl, whale glass, breath flute, slime choir), or drop any sound file (wav, aiff, flac, ogg, mp3) onto the waveform or click LOAD SOUND (it becomes MY FILE, and stays loaded when you change presets). POSITION, SPRAY, SIZE, DENSITY, PITCH, REVERSE and SCAN pull it apart.
* **IN KEY** (on by default) keeps GRAIN in tune: Timewind finds the main pitch of your file and moves it onto the current chord root, and PITCH snaps to octaves (and fifths, when the scale has the notes for them). The built-in sounds use only a root and a fifth, so they fit every chord. A sound with no clear pitch (noise, drums) only snaps to octaves.
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

The panel over the slime mold has ten tabs:

* **SEQUENCE**: the arp's 16 steps. **HOLD** freezes the pattern so it repeats exactly. Click a step to switch it on or off, drag up and down to set its note (drag across steps to draw a line). **REC NOTES** lets you play a MIDI keyboard into the running sequence. **NEW** writes a fresh singable pattern, **CLEAR** empties it, **< >** shift it. **SAVE** then a number stores the sequence in that slot; a number on its own recalls it. Slots are shared by every Timewind on your computer.
* **ARP SOUND**: octave, glide, soft attack, pluck length, brightness, resonance, drive, detune, echo and level of the arp voice.
* **DRONE**: TYPE picks what the six voices are made of: **SAWS** (the original), **ORGAN** (drawbars), **STRINGS** (an old string machine, two saws per voice), **GLASS** (shimmering FM bells, whole-number ratios so it stays in tune) or **CHOIR** (saws through moving vowel formants). COLOUR shapes each type: saw to soft, more drawbars, a wider ensemble, more shimmer, or the vowel from oo to ah to eh. Then chord, octave, detune, drift, level, tone, breath (each voice swells on its own slow cycle), air (breathy noise) and sub.
* **CHORDS**: a second layer that plays chords. It always plays the current HARMONY chord (the same one as the pads, and the arp in PADS mode), with smooth voice leading, so it is in tune with everything. When the arp plays the whole SCALE, chord notes outside the scale are left out. STYLE: **PAD** (held, gliding between chords), **SWELL** (fades in every 2 bars), **PULSE** (on every beat), **ARP HITS** (a chord with every arp note), **STRUM** (strummed every bar) or **STEPS**, which plays the **HITS** pattern underneath. VOICES, OCTAVE, COLOUR (sine to soft to two detuned saws), TONE, ATTACK, RELEASE and WIDTH shape it. "Now playing" shows its notes.
  * **HITS** is a step pattern for when the chords play, like the arp's sequence. **STEPS** sets how many steps it has (2 to 16), **STEP** how long each one is (1/16 to a bar), **DENSITY** how many of them play, and **GATE** how long each hit rings, as a share of the time until the next one (all the way up is legato). **NEW** shuffles a fresh pattern, **VARY** changes a few steps every time round. Click a step to force it on or off (a small dot marks it), drag to paint several.
* **HARMONY**: chord progressions, all built from the notes of your key so nothing clashes.
  * **PROGRESSION**: PEACEFUL, DREAMY, DRIFTING and CREEPY (outside chords, on purpose), plus NOSTALGIC, HOPEFUL, MODAL, DESCENDING and FLOATING. **WANDER** never repeats: each chord leads to one that tends to follow it. **MY CHORDS** is your own progression.
  * **CHORD COLOUR** re-voices every chord: NATURAL (each progression's own voicings), TRIADS, 7THS, 9THS, SUS, 4THS or OPEN 5THS. **CHANGE EVERY** sets how long each chord lasts (1 to 16 bars), **CHORD GLIDE** how slowly they slide into each other.
  * **The chord wheel** is the circle of fifths in your key, with your key at the top. The key's notes are joined by an arc, the chord playing now glows with its shape drawn across the wheel, the dashed shape and the arrow show the next chord, and arp notes flash where they land. The middle shows the chord's name, its Roman numeral and how far through it you are. Note names follow the key (Db in F minor, F# in D major).
  * **The loop** on the right shows the chords in order, each with a bar for its length. Click any chord to change it: pick its chord (I to VII), its type (triad, 7th, 9th, sus2, sus4, 4ths, open, or COLOUR to follow CHORD COLOUR) and its length (half a bar to 16 bars, or EVEN to follow CHANGE EVERY). Editing a built-in progression copies it into MY CHORDS first, so you can tweak one chord without losing the rest; **EDIT THESE** does the same for the whole progression. The **+** adds a chord, and "remove this chord" takes one out.
  * **NEW CHORDS** writes a fresh MY CHORDS progression of **LENGTH** chords (2 to 8). **RHYTHM** decides how long they last: EVEN (all the same), GENTLE (now and then one lasts twice as long), VARIED (half to double) or RESTLESS (short and quick). RHYTHM also sets the pace of WANDER.
  * **NEW SCENE KEY** decides whether a new scene keeps the key, moves to a nearby key or jumps anywhere.
* **LOOPS**: drifting tape loops. Each loop is a ring: a length of tape that rings one note of the chord each time round (a slow bell or glass tone that swells and fades). The loops all have different lengths, so they drift in and out of phase and the music never quite repeats. **NOTES** picks what they play (the CHORD, the whole SCALE or just ROOTS), **LENGTH** how long the shortest loop is, **LOOPS** how many run, **SWELL** and **DECAY** shape each note, **COLOUR** goes from bell to glass, **TONE** darkens it, **WOBBLE** is tape wow. **WEIRD** reverses some swells, bends notes as if the tape stretched, adds quiet ghost notes an octave or a twelfth away and now and then leaves a loop silent. **SHUFFLE** gives every loop a new length and note.
* **PULSE**: soft drums, all synthesised: **THUMP** (a low heartbeat, tuned to your key), **SKIN** (a hand drum), **WOOD** (a block tuned to the chord), **BRUSH** and **TICK** (a tiny bell). **STYLE**: HEARTBEAT (lub-dub), DRIFT (loose and sparse), LAZY (a slow broken beat), RAIN (no beat, things falling), MOTORIK (a soft steady pulse) or ORGANISM (the slime's ears play the drums: high ears tick, low ears thump). **STEP** is 1/16, 1/8 (half time, the default) or 1/4. **DENSITY** fills the pattern, **SWING** and **HUMAN** loosen it, **TONE** darkens it, **SPACE** sends it into the echo and hall, **VARY** changes a few hits each time round, **WEIRD** adds ratchets, dropped hits, falling wood and reversed brushes. **NEW** writes a fresh pattern; click a step to force it on or off, drag to paint.
* **TEXTURE**: weather and creatures, made from nothing (no samples): RAIN, WIND, STREAM, CRACKLE, CREATURES (crickets, birds, a frog), FIRE and SHORE. Pick two (**TEXTURE A** and **B**) and blend them with **A / B**. **DENSITY**, **TONE** and **MOTION** (gusts, waves) shape them. **IN KEY** tunes the rain drops, bubbles, wind whistles and birds to the chord. **SPACE** sends them into the hall.
* **MEMORY**: Timewind remembers everything it plays (the last three minutes or so) and two tape heads keep drifting back into it, playing old moments in again. The strip shows what it remembers, newest on the right, and where the heads are. **HOW LONG AGO** sets how far back they reach, **FRAGMENT** how long each memory is, **SCATTER** how far they wander from that, **SPEED** how they play (1x, 1/2, 1/4, REVERSE, REV 1/2, or WANDER, which picks for each fragment), **BLUR** softens and wobbles them, **DISSOLVE** lets the memories be remembered again, so old moments slowly dissolve into each other. **FORGET** starts again with an empty memory.
* **ORGANISM**: the slime mold. It now lives inside the instrument (it keeps growing with the window closed) and has ears. Turn on **SLIME PLAYS** and whenever a vein of slime reaches an ear, the ear plays its note: left to right is low to high, always from the chord (with WEIRD, sometimes from the scale). **VOICE**: DROPS (water drops), BREATH (blown noise), TINES (little metal tongues) or MIXED (high ears tine, middle ears drop, low ears breathe). **ACTIVITY** sets how readily the ears hear (a few notes a minute up to one a second), **EARS** how many there are, **GROWTH** how fast the slime grows, **RANGE** and **OCTAVE** the notes, **WEIRD** makes ears wander and adds swarms of quiet notes, slides, detuned and reversed notes. Your hand (with the camera on) still pulls the slime about, so you can steer which ears it reaches.

**FULL SCREEN** (or F11) fills your screen with Timewind. In Ableton it opens Timewind in a full-screen window of its own (the plugin window says so while it is away); press Esc or F11, or click EXIT FULL, to bring it back. The standalone app goes full screen itself. **CAMERA** opens the full-screen camera stage.

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
* **Chords + textures** (the drone types, the CHORDS layer, the built-in GRAIN sounds and the new harmony): Choir Of Rooms, Organ Sunday, Glass Weather, String Machine Dawn, Rain Stabs, Bowed Iron Creep, Wandering Hymn (WANDER with a HITS pattern), Postcard Loop (MY CHORDS with chords of different lengths).
* **Weird + calm** (the loops, the slime, PULSE, TEXTURE and MEMORY): Orbiting Bells, Reverse Garden, Slime Choir, Heartbeat Moss, Lazy Tape Afternoon, Rain on the Organism, Memory Hall, Night Creatures, Motorik Dream.
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
It also puts a **Timewind** shortcut for the standalone app on your Desktop and in the Start menu (once; if you delete
them they stay deleted).

## Installing in Ableton manually (Windows)

1. Copy `Timewind.vst3` to `C:\Program Files\Common Files\VST3\`.
2. In Ableton: Options > Preferences > Plug-Ins, turn on "Use VST3 Plug-In System Folders", then Rescan.
3. Drag Timewind from Plug-Ins > Timewind onto a MIDI track and press play.
