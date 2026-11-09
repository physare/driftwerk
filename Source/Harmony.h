#pragma once

// Harmony: scales, chord progressions and chord names. Pure functions, shared by the audio
// engine (no allocation on that side) and the editor (names, the chord wheel).

#include <juce_core/juce_core.h>
#include <vector>

namespace harm
{
inline const juce::StringArray scaleNames { "MINOR", "DORIAN", "PHRYG", "PENTA", "HARM", "WHOLE", "MAJOR", "LYDIAN" };
inline const std::vector<std::vector<int>> scales {
    { 0, 2, 3, 5, 7, 8, 10 },
    { 0, 2, 3, 5, 7, 9, 10 },
    { 0, 1, 3, 5, 7, 8, 10 },
    { 0, 3, 5, 7, 10 },
    { 0, 2, 3, 5, 7, 8, 11 },
    { 0, 2, 4, 6, 8, 10 },
    { 0, 2, 4, 5, 7, 9, 11 },
    { 0, 2, 4, 6, 7, 9, 11 },
};
inline const std::vector<int>& scaleOf (int s) { return scales[(size_t) juce::jlimit (0, (int) scales.size() - 1, s)]; }
// Chords are built on the 7-note scale a pentatonic comes from.
inline int parentScale (int s) { return s == 3 ? 0 : juce::jlimit (0, 7, s); }
inline bool isMajorFamily (int s) { return s == 6 || s == 7; }

// Dream pad voicings (semitones above the chord root), used by the first four progressions.
inline constexpr int dreamChords[6][6] {
    { 0, 4, 7, 11, 14, 19 }, { 0, 3, 7, 10, 14, 17 }, { 0, 2, 7, 12, 14, 19 },
    { 0, 4, 6, 11, 14, 18 }, { 0, 4, 7, 14, 16, 19 }, { 0, 5, 10, 15, 19, 22 } };
inline const juce::StringArray dreamChordNames { "MAJ 7", "MIN 9", "SUS 2", "LYDIAN", "ADD 9", "QUARTAL" };

//==============================================================================
// Progressions
inline const juce::StringArray progNames { "OFF", "PEACEFUL", "DREAMY", "DRIFTING", "CREEPY",
                                           "NOSTALGIC", "HOPEFUL", "MODAL", "DESCENDING", "FLOATING", "WANDER", "MY CHORDS" };
constexpr int progClassicLast = 4, progTableLast = 9, progWander = 10, progMine = 11, numProgModes = 12;
inline const juce::StringArray progBarNames { "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS" };
constexpr int progBars[] { 1, 2, 4, 8, 16 };
inline const juce::StringArray colourNames { "NATURAL", "TRIADS", "7THS", "9THS", "SUS", "4THS", "OPEN 5THS" };
inline const juce::StringArray rhythmNames { "EVEN", "GENTLE", "VARIED", "RESTLESS" };
inline const juce::StringArray lengthNames { "2", "3", "4", "5", "6", "7", "8" };
constexpr int maxSlots = 8;
inline const juce::StringArray degreeNames { "I", "II", "III", "IV", "V", "VI", "VII" };
inline const juce::StringArray slotTypeNames { "COLOUR", "TRIAD", "7TH", "9TH", "SUS2", "SUS4", "4THS", "OPEN" };
inline const juce::StringArray slotLenNames { "EVEN", "1/2 BAR", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS" };
constexpr double slotLenBeats[] { 0.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0 };   // 0 = CHANGE EVERY
constexpr int defaultDegrees[maxSlots] { 0, 5, 3, 4, 0, 3, 5, 4 };          // I vi IV V I IV vi V

// Chord types, as steps up the scale (so every note stays in the key).
enum ChordType { TNat, TTriad, TSeventh, TNinth, TSus2, TSus4, TQuartal, TOpen, NumTypes };
inline constexpr int typeSteps[NumTypes][6] {
    { 0, 2, 4, 6, 7, 9 },     // (natural: resolved before use)
    { 0, 2, 4, 7, 9, 11 },    // triad, doubled an octave up
    { 0, 2, 4, 6, 7, 9 },     // 7th
    { 0, 2, 4, 6, 8, 11 },    // 9th
    { 0, 1, 4, 7, 8, 11 },    // sus2
    { 0, 3, 4, 7, 10, 11 },   // sus4
    { 0, 3, 6, 9, 12, 15 },   // stacked 4ths
    { 0, 4, 7, 8, 11, 14 },   // open 5ths with a 9th
};
inline constexpr int colourType[7] { TNat, TTriad, TSeventh, TNinth, TSus2, TQuartal, TOpen };

// The first four progressions: { root offset in semitones, pad voicing, drone chord }, minor and major.
struct ProgChord { int offset, dream, drone; };
inline constexpr ProgChord progTable[5][2][4] {
    { { { 0, 1, 1 }, { 0, 1, 1 }, { 0, 1, 1 }, { 0, 1, 1 } }, { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } } },
    // PEACEFUL   minor: i  VI  III  VII              major: I  vi  IV  V
    { { { 0, 1, 1 }, { 8, 0, 1 }, { 3, 4, 1 }, { 10, 2, 1 } }, { { 0, 0, 1 }, { 9, 1, 1 }, { 5, 0, 1 }, { 7, 2, 1 } } },
    // DREAMY     minor: i  iv  i  VI                 major: I  IV(lyd)  I  vi
    { { { 0, 1, 2 }, { 5, 1, 2 }, { 0, 1, 2 }, { 8, 0, 2 } }, { { 0, 0, 2 }, { 5, 3, 2 }, { 0, 0, 2 }, { 9, 1, 2 } } },
    // DRIFTING   minor: i(quartal) VII(sus) VI(add9) iv    major: I(add9) V(sus) vi IV(lyd)
    { { { 0, 5, 4 }, { 10, 2, 4 }, { 8, 4, 1 }, { 5, 1, 4 } }, { { 0, 4, 4 }, { 7, 2, 4 }, { 9, 1, 1 }, { 5, 3, 4 } } },
    // CREEPY     i  bII  bVI  tritone, slow glides
    { { { 0, 5, 3 }, { 1, 2, 5 }, { 8, 1, 3 }, { 6, 5, 5 } }, { { 0, 5, 3 }, { 1, 2, 5 }, { 8, 1, 3 }, { 6, 5, 5 } } },
};

// The next five: scale degrees and chord types, so they fit any scale.
struct DegChord { int deg, type; };
inline constexpr DegChord degProgs[5][2][4] {
    // NOSTALGIC   minor: i9 VI7 III7 iv9                major: I7 iii7 vi9 IV7
    { { { 0, TNinth }, { 5, TSeventh }, { 2, TSeventh }, { 3, TNinth } }, { { 0, TSeventh }, { 2, TSeventh }, { 5, TNinth }, { 3, TSeventh } } },
    // HOPEFUL     minor: VI7 VII i9 isus2               major: IV9 I V(sus4) vi7
    { { { 5, TSeventh }, { 6, TTriad }, { 0, TNinth }, { 0, TSus2 } }, { { 3, TNinth }, { 0, TTriad }, { 4, TSus4 }, { 5, TSeventh } } },
    // MODAL       minor: i9 iv9 i9 VIIsus2              major: I7 ii9 I7 ii9
    { { { 0, TNinth }, { 3, TNinth }, { 0, TNinth }, { 6, TSus2 } }, { { 0, TSeventh }, { 1, TNinth }, { 0, TSeventh }, { 1, TNinth } } },
    // DESCENDING  minor: i7 VII VI7 v7                  major: I9 vi7 Vsus4 IV7
    { { { 0, TSeventh }, { 6, TTriad }, { 5, TSeventh }, { 4, TSeventh } }, { { 0, TNinth }, { 5, TSeventh }, { 4, TSus4 }, { 3, TSeventh } } },
    // FLOATING    minor: i(4ths) iv(sus2) VII(4ths) v(sus4)    major: I(sus2) V(4ths) IV(sus2) ii(4ths)
    { { { 0, TQuartal }, { 3, TSus2 }, { 6, TQuartal }, { 4, TSus4 } }, { { 0, TSus2 }, { 4, TQuartal }, { 3, TSus2 }, { 1, TQuartal } } },
};

// WANDER: how likely each chord is to follow each other chord (by scale degree).
inline constexpr int wanderWeights[2][7][7] {
    {   // minor: i ii III iv v VI VII
        { 0, 0, 3, 3, 2, 4, 3 }, { 3, 0, 1, 1, 3, 1, 1 }, { 2, 0, 0, 3, 1, 3, 3 }, { 3, 0, 1, 0, 2, 2, 3 },
        { 4, 0, 1, 2, 0, 3, 1 }, { 2, 0, 3, 3, 1, 0, 4 }, { 4, 0, 4, 2, 1, 2, 0 } },
    {   // major: I ii iii IV V vi vii
        { 0, 2, 1, 4, 3, 4, 0 }, { 1, 0, 1, 2, 4, 2, 0 }, { 1, 2, 0, 3, 1, 4, 0 }, { 4, 2, 1, 0, 3, 2, 0 },
        { 5, 1, 1, 2, 0, 3, 0 }, { 2, 3, 2, 4, 2, 0, 0 }, { 4, 0, 1, 0, 1, 1, 0 } },
};

//==============================================================================
struct Harm
{
    int offset = 0;     // chord root, semitones from the key root (-5..6)
    int iv[6] {};       // voicing, semitones above the chord root
    int drone = 1;      // drone chord (UNISON, FIFTH, OCTAVE, MINOR, SUS, CLUSTER)
    int degree = 0;     // scale degree of the root, -1 when it is outside the scale
    int type = -1;      // ChordType, -1 for a pad voicing
    int dream = -1;     // pad voicing (dreamChords), -1 for a scale chord

    int ivMask() const { int m = 0; for (int x : iv) m |= 1 << (((x % 12) + 12) % 12); return m; }
    bool has (int semis) const { return (ivMask() >> semis) & 1; }
};

inline int fold (int o) { o = ((o % 12) + 12) % 12; return o > 6 ? o - 12 : o; }   // keep roots close
inline int resolveType (int type, int deg) { return type == TNat ? (deg == 0 ? TNinth : TSeventh) : juce::jlimit (1, NumTypes - 1, type); }
inline int applyColour (int type, int colour) { return colour > 0 ? colourType[juce::jlimit (0, 6, colour)] : type; }

// Drone under a moving chord: the user's UNISON or OCTAVE stays, otherwise a fifth when the chord has one.
inline int droneFor (const Harm& h, int userDrone)
{
    if (userDrone == 0 || userDrone == 2) return userDrone;
    return h.has (7) ? 1 : 2;
}

inline bool hasFifth (int scale, int deg)
{
    const auto& sc = scaleOf (parentScale (scale));
    const int n = (int) sc.size();
    deg %= n;
    return sc[(size_t) ((deg + 4) % n)] + 12 * ((deg + 4) / n) - sc[(size_t) deg] == 7;
}

// A chord built from the scale on one degree.
inline Harm diatonic (int scale, int deg, int type, int userDrone)
{
    const auto& sc = scaleOf (parentScale (scale));
    const int n = (int) sc.size();
    deg = ((deg % n) + n) % n;
    Harm h;
    h.degree = deg;
    h.type = resolveType (type, deg);
    for (int k = 0; k < 6; ++k)
    {
        const int s = deg + typeSteps[h.type][k];
        h.iv[k] = sc[(size_t) (s % n)] + 12 * (s / n) - sc[(size_t) deg];
    }
    // Keep it sweet: a ninth or second only a half step above the root (the v chord in minor,
    // iii and vii in major, i in phrygian) drops to the root, and a sus2 there becomes a sus4.
    if (h.type == TSus2 && h.iv[1] == 1) return diatonic (scale, deg, TSus4, userDrone);
    if (h.type == TNinth || h.type == TOpen)
        for (int& x : h.iv) if (x % 12 == 1) x -= 1;
    h.offset = fold (sc[(size_t) deg]);
    h.drone = droneFor (h, userDrone);
    return h;
}

inline int degreeOf (int scale, int offset)
{
    const auto& sc = scaleOf (parentScale (scale));
    const int pc = ((offset % 12) + 12) % 12;
    for (size_t k = 0; k < sc.size(); ++k) if (sc[k] == pc) return (int) k;
    return -1;
}

inline Harm padChord (int offset, int dream, int drone, int degree)
{
    Harm h;
    h.offset = fold (offset);
    h.dream = juce::jlimit (0, 5, dream);
    for (int k = 0; k < 6; ++k) h.iv[k] = dreamChords[h.dream][k];
    h.drone = drone;
    h.degree = degree;
    return h;
}

// Progression OFF: one chord. NATURAL keeps the DREAM chord and drone chord you picked.
inline Harm staticChord (int scale, int colour, int dream, int userDrone)
{
    if (colour <= 0) return padChord (0, dream, userDrone, 0);
    Harm h = diatonic (scale, 0, colourType[juce::jlimit (0, 6, colour)], userDrone);
    h.drone = userDrone;
    return h;
}

// Chord idx (0..3) of one of the fixed four-chord progressions (modes 1..9).
inline Harm tableChord (int mode, int scale, int idx, int colour, int userDrone)
{
    const int family = isMajorFamily (scale) ? 1 : 0;
    idx &= 3;
    if (mode <= progClassicLast)
    {
        const auto& c = progTable[juce::jlimit (0, 4, mode)][family][idx];
        const int deg = degreeOf (scale, c.offset);
        if (colour > 0 && deg >= 0) return diatonic (scale, deg, colourType[juce::jlimit (0, 6, colour)], userDrone);
        return padChord (c.offset, c.dream, c.drone, deg);
    }
    const auto& c = degProgs[juce::jlimit (0, 4, mode - progClassicLast - 1)][family][idx];
    return diatonic (scale, c.deg, applyColour (c.type, colour), userDrone);
}

// WANDER chords are stored as degree * 16 + type.
inline Harm codeChord (int scale, int code, int colour, int userDrone)
{
    return diatonic (scale, code / 16, applyColour (code % 16, colour), userDrone);
}

// MY CHORDS: slot type 0 follows CHORD COLOUR, the rest pick a type.
inline Harm slotChord (int scale, int deg, int slotType, int colour, int userDrone)
{
    return diatonic (scale, deg, slotType <= 0 ? applyColour (TNat, colour) : slotType, userDrone);
}

// Next WANDER chord after `code`. rnd() returns 0..1.
template <typename Rnd>
int wanderPick (int scale, int code, Rnd&& rnd)
{
    const int n = (int) scaleOf (parentScale (scale)).size();
    const int deg = (code / 16) % 7;
    const int family = isMajorFamily (scale) ? 1 : 0;
    float w[7] {}, tot = 0.0f;
    for (int d = 0; d < 7; ++d)
    {
        w[d] = (d < n && d != deg && hasFifth (scale, d)) ? (float) wanderWeights[family][deg][d] : 0.0f;
        tot += w[d];
    }
    int next = 0;
    if (tot <= 0.0f)
        next = (deg + 1 + (int) (rnd() * (float) (n - 1) * 0.999f)) % n;
    else
    {
        float x = rnd() * tot;
        for (int d = 0; d < 7; ++d)
            if (w[d] > 0.0f) { next = d; x -= w[d]; if (x < 0.0f) break; }
    }
    const float t = rnd();
    const int type = next == 0 ? TNinth : (t < 0.6f ? TSeventh : (t < 0.8f ? TNinth : TSus2));
    return next * 16 + type;
}

//==============================================================================
// Names
inline bool useSharps (int keyPc, int scale)
{
    const int rel[] { 3, 10, 8, 3, 3, 0, 0, 7 };                  // to the major key with the same notes
    const int m = (keyPc + rel[juce::jlimit (0, 7, scale)]) % 12;
    return m == 7 || m == 2 || m == 9 || m == 4 || m == 11 || m == 6;
}

inline juce::String pcName (int pc, bool sharps)
{
    static const char* sh[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* fl[] { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    pc = ((pc % 12) + 12) % 12;
    return sharps ? sh[pc] : fl[pc];
}

// How to spell notes in a key: flats in flat keys (Db, not C#, in F minor), sharps in sharp keys,
// and the raised 7th of harmonic minor always a sharp (G# in A minor, C# in D minor).
struct Spelling
{
    int keyPc = 0, scale = 0;
    bool sharps = false;
    Spelling() = default;
    Spelling (int key, int sc) : keyPc (((key % 12) + 12) % 12), scale (sc), sharps (useSharps (keyPc, sc)) {}
    juce::String operator() (int pc) const
    {
        pc = ((pc % 12) + 12) % 12;
        if (scale == 4 && pc == (keyPc + 11) % 12) return pcName (pc, true);
        return pcName (pc, sharps);
    }
};

inline juce::String noteName (int midi, const Spelling& sp) { return sp (midi) + juce::String (midi / 12 - 1); }

inline juce::String keyName (const Spelling& sp)
{
    static const char* modes[] { "minor", "dorian", "phrygian", "minor penta", "harmonic minor", "whole tone", "major", "lydian" };
    return sp (sp.keyPc) + " " + modes[juce::jlimit (0, 7, sp.scale)];
}

enum Quality { Major, Minor, Dim, Aug, Sus };
inline int quality (const Harm& h)
{
    if (h.type == TSus2 || h.type == TSus4 || h.type == TQuartal || h.type == TOpen) return Sus;
    if (h.has (4)) return h.has (8) && ! h.has (7) ? Aug : Major;
    if (h.has (3)) return h.has (6) && ! h.has (7) ? Dim : Minor;
    return Sus;
}

inline juce::String suffix (const Harm& h)
{
    if (h.dream >= 0)
    {
        static const char* d[] { "maj7", "m9", "sus2", "maj7#11", "add9", " 4ths" };
        return d[h.dream];
    }
    const int q = quality (h);
    switch (h.type)
    {
        case TSus2: return "sus2";
        case TSus4: return h.has (5) ? "sus4" : "sus#4";
        case TQuartal: return " 4ths";
        case TOpen: return " open";
        default: break;
    }
    juce::String tri = q == Minor ? "m" : (q == Dim ? "dim" : (q == Aug ? "+" : ""));
    if (h.type == TTriad) return tri;
    juce::String sev;
    if (q == Major) sev = h.has (11) ? "maj7" : "7";
    else if (q == Minor) sev = h.has (11) ? "m(maj7)" : "m7";
    else if (q == Dim) sev = h.has (10) ? "m7b5" : "dim7";
    else sev = h.has (11) ? "maj7#5" : "7#5";
    if (h.type == TSeventh) return sev;
    if (h.has (2))
    {
        if (sev == "maj7") return "maj9";
        if (sev == "7") return "9";
        if (sev == "m7") return "m9";
        if (sev == "m(maj7)") return "m(maj9)";
        return sev + "(9)";
    }
    return h.has (1) ? sev + "b9" : sev;
}

inline juce::String chordName (const Harm& h, const Spelling& sp) { return sp (sp.keyPc + h.offset) + suffix (h); }

inline juce::String numeral (const Harm& h)
{
    static const char* deg[] { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const char* chrom[] { "I", "bII", "II", "bIII", "III", "IV", "bV", "V", "bVI", "VI", "bVII", "VII" };
    juce::String s = h.degree >= 0 ? juce::String (deg[h.degree % 7]) : juce::String (chrom[((h.offset % 12) + 12) % 12]);
    const int q = quality (h);
    if (q == Minor || q == Dim) s = s.toLowerCase();
    if (q == Dim) s << juce::String::fromUTF8 ("\xc2\xb0");
    if (q == Aug) s << "+";
    return s;
}
} // namespace harm
