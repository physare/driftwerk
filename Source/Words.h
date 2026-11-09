// WORDS: half-heard speech drifting through the music. Load your own recordings (a file or a
// folder) and Timewind cuts them into words and phrases by listening for the gaps; until you do,
// a little synthetic voice (made here, a formant speech synthesiser) says a few calm words.
// The player picks a word or a whole phrase now and then and plays it like an old tape: slower,
// wobbly, through a small radio, sometimes backwards or caught repeating. All original code.
#pragma once

#include "DSP.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace dw
{
//==============================================================================
// The words: one long mono recording (16-bit) and where each word and phrase is in it.
struct WordBank
{
    double rate = 22050.0;
    std::vector<int16_t> audio;
    struct Seg { int start = 0, end = 0, phrase = 0; };
    std::vector<Seg> words;
    struct Phrase { int first = 0, count = 0; };
    std::vector<Phrase> phrases;
    std::vector<std::string> text;     // what each word says (the synthetic voice), else empty
    std::string name;                  // what to call it in the editor
    bool synthetic = false;

    float at (double pos) const
    {
        const int n = (int) audio.size();
        if (pos < 0.0 || pos >= (double) (n - 1)) return 0.0f;
        const int i = (int) pos;
        const float f = (float) (pos - (double) i);
        return ((float) audio[(size_t) i] + ((float) audio[(size_t) i + 1] - (float) audio[(size_t) i]) * f) * (1.0f / 32768.0f);
    }

    void append (const float* x, int n, float gain)
    {
        audio.reserve (audio.size() + (size_t) n);
        for (int i = 0; i < n; ++i)
            audio.push_back ((int16_t) std::lround (clampf (x[i] * gain, -1.0f, 1.0f) * 32767.0f));
    }

    // Find the words in a recording: frames louder than the room are speech, short gaps join a
    // word, long gaps end a phrase. Works on whatever is in audio, from sample `from` on.
    void slice (int from = 0)
    {
        const int n = (int) audio.size();
        const int hop = std::max (1, (int) (rate * 0.01));
        const int frames = (n - from) / hop;
        if (frames < 4) return;
        std::vector<float> env ((size_t) frames);
        for (int f = 0; f < frames; ++f)
        {
            double s = 0.0;
            for (int i = 0; i < hop; ++i) { const double v = audio[(size_t) (from + f * hop + i)] / 32768.0; s += v * v; }
            env[(size_t) f] = (float) std::sqrt (s / hop);
        }
        std::vector<float> sorted (env);
        std::nth_element (sorted.begin(), sorted.begin() + (long) (sorted.size() * 9 / 10), sorted.end());
        const float loud = sorted[sorted.size() * 9 / 10];
        std::nth_element (sorted.begin(), sorted.begin() + (long) (sorted.size() / 10), sorted.end());
        const float room = sorted[sorted.size() / 10];
        const float thr = std::max ({ loud * 0.06f, room * 2.5f, 2.0e-4f });
        const int joinGap = 6, phraseGap = 30, minWord = 8, maxWord = 250;   // in 10 ms frames
        int f = 0, phrase = (int) phrases.size(), lastEnd = -1000;
        Phrase cur { (int) words.size(), 0 };
        while (f < frames)
        {
            while (f < frames && env[(size_t) f] < thr) ++f;
            if (f >= frames) break;
            const int start = f;
            int quiet = 0, end = f;
            while (f < frames && quiet <= joinGap && f - start < maxWord)
            {
                if (env[(size_t) f] >= thr) { quiet = 0; end = f; } else ++quiet;
                ++f;
            }
            if (end - start + 1 < minWord) continue;
            if (start - lastEnd > phraseGap && cur.count > 0)
            {
                phrases.push_back (cur);
                cur = { (int) words.size(), 0 };
                ++phrase;
            }
            Seg s;
            s.start = from + std::max (0, start * hop - (int) (rate * 0.03));
            s.end = std::min (n - 1, from + (end + 1) * hop + (int) (rate * 0.07));
            s.phrase = phrase;
            words.push_back (s);
            text.emplace_back();
            ++cur.count;
            lastEnd = end;
        }
        if (cur.count > 0) phrases.push_back (cur);
    }
};

//==============================================================================
// A small formant speech synthesiser: a buzzing glottis through three moving formants, plus
// hiss for s, sh, f and the bursts of p, t, k. Robotic and soft, like a talking toy.
struct SpeechSynth
{
    enum Kind { Vowel, Glide, Nasal, Fric, VFric, Stop, VStop, Aspirate };
    struct Ph
    {
        const char* name; int kind;
        float f1, f2, f3;          // formant targets (an adult man; the speaker scales them)
        float b1, b2, b3;          // second target for diphthongs (0 = none)
        float dur;                 // seconds
        float voice, noise, nf, nbw;
    };
    static const Ph* find (const std::string& name)
    {
        static const Ph table[] {
            { "IY", Vowel, 270, 2290, 3010, 0, 0, 0, 0.11f, 1, 0, 0, 0 },     { "IH", Vowel, 390, 1990, 2550, 0, 0, 0, 0.08f, 1, 0, 0, 0 },
            { "EH", Vowel, 530, 1840, 2480, 0, 0, 0, 0.09f, 1, 0, 0, 0 },     { "AE", Vowel, 660, 1720, 2410, 0, 0, 0, 0.12f, 1, 0, 0, 0 },
            { "AA", Vowel, 730, 1090, 2440, 0, 0, 0, 0.12f, 1, 0, 0, 0 },     { "AO", Vowel, 570, 840, 2410, 0, 0, 0, 0.12f, 1, 0, 0, 0 },
            { "UH", Vowel, 440, 1020, 2240, 0, 0, 0, 0.08f, 1, 0, 0, 0 },     { "UW", Vowel, 300, 870, 2240, 0, 0, 0, 0.11f, 1, 0, 0, 0 },
            { "AH", Vowel, 620, 1190, 2390, 0, 0, 0, 0.09f, 1, 0, 0, 0 },     { "ER", Vowel, 490, 1350, 1690, 0, 0, 0, 0.12f, 1, 0, 0, 0 },
            { "AX", Vowel, 500, 1500, 2500, 0, 0, 0, 0.055f, 0.9f, 0, 0, 0 },
            { "EY", Vowel, 530, 1840, 2480, 330, 2200, 2900, 0.15f, 1, 0, 0, 0 }, { "AY", Vowel, 730, 1090, 2440, 380, 2050, 2800, 0.17f, 1, 0, 0, 0 },
            { "OW", Vowel, 570, 840, 2410, 360, 780, 2300, 0.15f, 1, 0, 0, 0 },   { "AW", Vowel, 730, 1090, 2440, 420, 900, 2300, 0.17f, 1, 0, 0, 0 },
            { "L", Glide, 360, 1050, 2900, 0, 0, 0, 0.06f, 0.6f, 0, 0, 0 },      { "R", Glide, 420, 1100, 1500, 0, 0, 0, 0.06f, 0.7f, 0, 0, 0 },
            { "W", Glide, 300, 650, 2200, 0, 0, 0, 0.055f, 0.65f, 0, 0, 0 },     { "Y", Glide, 260, 2070, 3020, 0, 0, 0, 0.05f, 0.65f, 0, 0, 0 },
            { "M", Nasal, 270, 1000, 2200, 0, 0, 0, 0.07f, 0.4f, 0, 0, 0 },      { "N", Nasal, 270, 1600, 2600, 0, 0, 0, 0.06f, 0.4f, 0, 0, 0 },
            { "NG", Nasal, 270, 2000, 2700, 0, 0, 0, 0.07f, 0.4f, 0, 0, 0 },
            { "S", Fric, 320, 1700, 2600, 0, 0, 0, 0.11f, 0, 0.55f, 5600, 1400 }, { "Z", VFric, 300, 1700, 2600, 0, 0, 0, 0.09f, 0.35f, 0.4f, 5400, 1400 },
            { "SH", Fric, 300, 1900, 2500, 0, 0, 0, 0.12f, 0, 0.6f, 2900, 1700 },  { "ZH", VFric, 300, 1900, 2500, 0, 0, 0, 0.09f, 0.35f, 0.45f, 2800, 1700 },
            { "F", Fric, 300, 900, 2200, 0, 0, 0, 0.1f, 0, 0.22f, 4200, 5000 },   { "V", VFric, 300, 900, 2200, 0, 0, 0, 0.07f, 0.45f, 0.14f, 4000, 5000 },
            { "TH", Fric, 300, 1400, 2500, 0, 0, 0, 0.1f, 0, 0.16f, 4800, 5000 }, { "DH", VFric, 300, 1400, 2500, 0, 0, 0, 0.05f, 0.55f, 0.07f, 4500, 5000 },
            { "HH", Aspirate, 500, 1500, 2500, 0, 0, 0, 0.065f, 0, 0.5f, 0, 0 },
            { "P", Stop, 250, 850, 2200, 0, 0, 0, 0.07f, 0, 0.3f, 1100, 2500 },  { "B", VStop, 250, 850, 2200, 0, 0, 0, 0.06f, 0.2f, 0.2f, 1000, 2500 },
            { "T", Stop, 250, 1750, 2600, 0, 0, 0, 0.065f, 0, 0.5f, 4500, 3000 }, { "D", VStop, 250, 1750, 2600, 0, 0, 0, 0.055f, 0.2f, 0.35f, 4000, 3000 },
            { "K", Stop, 250, 2000, 2500, 0, 0, 0, 0.075f, 0, 0.5f, 2100, 1500 }, { "G", VStop, 250, 2000, 2500, 0, 0, 0, 0.06f, 0.2f, 0.35f, 1900, 1500 },
            { "CH", Fric, 300, 1900, 2500, 0, 0, 0, 0.13f, 0, 0.6f, 3000, 1800 }, { "JH", VFric, 300, 1900, 2500, 0, 0, 0, 0.1f, 0.35f, 0.45f, 2900, 1800 },
        };
        for (const auto& p : table) if (name == p.name) return &p;
        return nullptr;
    }

    // Klatt's digital resonator
    struct Reso
    {
        float a = 0, b = 0, c = 0, y1 = 0, y2 = 0;
        void set (float f, float bw, float sr)
        {
            const float r = std::exp (-kPi * bw / sr);
            c = -r * r; b = 2.0f * r * std::cos (kTwoPi * f / sr); a = 1.0f - b - c;
        }
        float process (float x) { const float y = a * x + b * y1 + c * y2; y2 = y1; y1 = y; return y; }
    };

    struct Speaker { float f0, scale, breath; };

    // Speaks a phrase (words of phonemes, e.g. {"HH AH L OW", "W ER L D"}) into out at sr;
    // wordEnds gets where each word ends.
    static void speak (const std::vector<std::string>& words, const Speaker& sp, float sr, uint32_t seed,
                       std::vector<float>& out, std::vector<int>& wordEnds)
    {
        struct Seg { const Ph* p; int len; bool last; int word; };
        std::vector<Seg> segs;
        for (int w = 0; w < (int) words.size(); ++w)
        {
            size_t i = 0; const std::string& s = words[(size_t) w];
            std::vector<const Ph*> phs;
            while (i < s.size())
            {
                while (i < s.size() && s[i] == ' ') ++i;
                size_t j = i; while (j < s.size() && s[j] != ' ') ++j;
                if (j > i) if (auto* p = find (s.substr (i, j - i))) phs.push_back (p);
                i = j;
            }
            for (size_t k = 0; k < phs.size(); ++k)
            {
                const bool lastWord = w == (int) words.size() - 1;
                float d = phs[k]->dur * (lastWord ? 1.25f : 1.0f) * 1.15f;     // calm: a touch slow, the last word longest
                if (k + 1 == phs.size() && phs[k]->kind == Vowel) d *= 1.3f;
                segs.push_back ({ phs[k], (int) (d * sr), k + 1 == phs.size(), w });
            }
            // a tiny gap between words so they can be cut apart
            segs.push_back ({ nullptr, (int) (0.06f * sr), false, w });
        }
        int total = 0; for (auto& s : segs) total += s.len;
        const int tail = (int) (0.12f * sr);
        out.assign ((size_t) (total + tail), 0.0f);
        wordEnds.assign (words.size(), 0);

        Random rng (seed | 1u);
        Reso r1, r2, r3, r4, r5, nres;
        float F1 = 500, F2 = 1500, F3 = 2500, av = 0, an = 0, nf = 3000, nbw = 2000, asp = 0;
        const float sm = 1.0f - std::exp (-1.0f / (0.018f * sr));   // formants glide (~18 ms)
        const float sa = 1.0f - std::exp (-1.0f / (0.006f * sr));   // amplitudes move fast
        r4.set (3400.0f * sp.scale, 250.0f, sr); r5.set (std::min (4200.0f * sp.scale, sr * 0.45f), 300.0f, sr);
        double gph = 0.0; float flowPrev = 0.0f, hp = 0.0f, hpx = 0.0f, jit = 0.0f, tilt = 0.0f;
        const float tiltK = 1.0f - std::exp (-kTwoPi * 1500.0f / sr);
        int pos = 0;
        for (size_t si = 0; si < segs.size(); ++si)
        {
            const auto& sg = segs[si];
            const Ph* p = sg.p;
            for (int n = 0; n < sg.len; ++n, ++pos)
            {
                const float x = (float) n / (float) std::max (1, sg.len);
                // targets
                float t1 = F1, t2 = F2, t3 = F3, tv = 0, tn = 0, ta = 0;
                if (p != nullptr)
                {
                    t1 = p->f1; t2 = p->f2; t3 = p->f3;
                    if (p->b1 > 0.0f && x > 0.35f) { const float k = std::min (1.0f, (x - 0.35f) / 0.5f); t1 += (p->b1 - p->f1) * k; t2 += (p->b2 - p->f2) * k; t3 += (p->b3 - p->f3) * k; }
                    t1 *= sp.scale; t2 *= sp.scale; t3 *= sp.scale;
                    tv = p->voice;
                    switch (p->kind)
                    {
                        case Fric: case VFric: tn = p->noise; nf = p->nf * std::sqrt (sp.scale); nbw = p->nbw; break;
                        case Stop: case VStop:
                        {
                            // closure, then a burst, then (p, t, k) a breath of aspiration
                            const float burstAt = 0.62f;
                            tv = p->kind == VStop ? (x < burstAt ? 0.12f : 0.6f) : 0.0f;
                            tn = x > burstAt && x < burstAt + 0.12f ? p->noise * 1.4f : 0.0f;
                            ta = p->kind == Stop && x >= burstAt + 0.1f ? 0.35f : 0.0f;
                            nf = p->nf * std::sqrt (sp.scale); nbw = p->nbw;
                            break;
                        }
                        case Aspirate: ta = p->noise; break;
                        default: break;
                    }
                }
                F1 += (t1 - F1) * sm; F2 += (t2 - F2) * sm; F3 += (t3 - F3) * sm;
                av += (tv - av) * sa; an += (tn - an) * sa; asp += (ta - asp) * sa;
                if ((pos & 15) == 0)
                {
                    const bool nasal = p != nullptr && p->kind == Nasal;
                    r1.set (F1, nasal ? 200.0f : 70.0f, sr); r2.set (F2, 100.0f, sr); r3.set (F3, 150.0f, sr);
                    nres.set (std::min (nf, sr * 0.45f), nbw, sr);
                }
                // the voice's pitch: falls gently through the phrase, a lift at the start, a little jitter
                const float through = (float) pos / (float) std::max (1, total);
                jit += (rng.next() * 0.02f - jit) * 0.002f;
                const float f0 = sp.f0 * (1.12f - 0.25f * through + 0.06f * std::sin (kPi * std::min (1.0f, through * 3.0f))) * (1.0f + jit);
                gph += f0 / sr;
                if (gph >= 1.0) gph -= 1.0;
                // glottal flow (Rosenberg) and its slope, which is what the ear hears
                const float g = (float) gph, tp = 0.4f, tn2 = 0.16f;
                const float flow = g < tp ? 0.5f * (1.0f - std::cos (kPi * g / tp)) : (g < tp + tn2 ? std::cos (kPi * (g - tp) / (2.0f * tn2)) : 0.0f);
                const float pulse = (flow - flowPrev) * sr / (f0 * 6.0f);
                flowPrev = flow;
                const float noise = rng.next();
                tilt += (pulse - tilt) * tiltK;                          // a softer glottis: the highs fall away faster
                const float src = av * tilt * 2.2f + (asp + sp.breath * av * flow) * noise * 0.45f;
                float y = r1.process (src);
                y = r2.process (y); y = r3.process (y); y = r4.process (y); y = r5.process (y);
                y += nres.process (noise) * an * 0.9f;
                // remove rumble
                hp = 0.995f * (hp + y - hpx); hpx = y;
                out[(size_t) pos] = hp;
            }
            if (si + 1 == segs.size() || segs[si + 1].word != sg.word || sg.p == nullptr)
                wordEnds[(size_t) sg.word] = pos;
        }
        // let the last sound ring off
        for (int n = 0; n < tail; ++n, ++pos)
        {
            float y = r1.process (0.0f); y = r2.process (y); y = r3.process (y); y = r4.process (y); y = r5.process (y);
            hp = 0.995f * (hp + y - hpx); hpx = y;
            out[(size_t) pos] = hp;
        }
    }

    // The built-in voice: a few calm phrases, each said by two different speakers.
    static std::unique_ptr<WordBank> makeBank()
    {
        struct W { const char* text; const char* ph; };
        static const W lex[] {
            { "hello", "HH AX L OW" }, { "good", "G UH D" }, { "morning", "M AO R N IH NG" }, { "one", "W AH N" }, { "two", "T UW" },
            { "three", "TH R IY" }, { "four", "F AO R" }, { "five", "F AY V" }, { "six", "S IH K S" }, { "seven", "S EH V AX N" },
            { "eight", "EY T" }, { "nine", "N AY N" }, { "ten", "T EH N" }, { "zero", "Z IY R OW" }, { "water", "W AO T ER" },
            { "the", "DH AX" }, { "sun", "S AH N" }, { "sea", "S IY" }, { "is", "IH Z" }, { "warm", "W AO R M" }, { "slowly", "S L OW L IY" },
            { "listen", "L IH S AX N" }, { "remember", "R IH M EH M B ER" }, { "i", "AY" }, { "we", "W IY" }, { "are", "AA R" },
            { "here", "HH IY R" }, { "home", "HH OW M" }, { "dream", "D R IY M" }, { "rain", "R EY N" }, { "summer", "S AH M ER" },
            { "light", "L AY T" }, { "forest", "F AO R AX S T" }, { "green", "G R IY N" }, { "blue", "B L UW" }, { "yellow", "Y EH L OW" },
            { "music", "M Y UW Z IH K" }, { "sleep", "S L IY P" }, { "again", "AX G EH N" }, { "look", "L UH K" }, { "now", "N AW" },
            { "everything", "EH V R IY TH IH NG" }, { "fine", "F AY N" }, { "tomorrow", "T AX M AA R OW" }, { "children", "CH IH L D R AX N" },
            { "follow", "F AA L OW" }, { "me", "M IY" }, { "breathe", "B R IY DH" }, { "over", "OW V ER" }, { "time", "T AY M" },
            { "far", "F AA R" }, { "away", "AX W EY" }, { "wind", "W IH N D" }, { "hold", "HH OW L D" }, { "on", "AA N" },
            { "it", "IH T" }, { "was", "W AA Z" }, { "so", "S OW" }, { "long", "L AO NG" }, { "ago", "AX G OW" }, { "see", "S IY" },
            { "you", "Y UW" }, { "soon", "S UW N" }, { "the", "DH AX" }, { "garden", "G AA R D AX N" }, { "lights", "L AY T S" } };
        static const char* phrases[] {
            "hello", "good morning", "one two three four", "five six seven eight", "the sun is warm", "i remember the sea",
            "listen", "we are home", "everything is fine", "follow me", "slowly now", "breathe", "far away", "the rain again",
            "nine", "zero", "green", "dream", "look", "summer light", "tomorrow", "children", "music", "over time", "water",
            "forest", "hold on", "it was so long ago", "see you soon", "the garden lights", "the wind", "seven", "blue", "yellow" };
        const Speaker speakers[] { { 255.0f, 1.28f, 0.10f }, { 205.0f, 1.16f, 0.08f }, { 118.0f, 1.0f, 0.05f } };
        auto bank = std::make_unique<WordBank>();
        bank->rate = 22050.0;
        bank->synthetic = true;
        bank->name = "the synthetic voice";
        std::vector<float> buf;
        std::vector<int> ends;
        uint32_t seed = 0x51EE7u;
        int pi = 0;
        for (const char* phrase : phrases)
            for (int rep = 0; rep < 2; ++rep)
            {
                std::vector<std::string> ph, tx;
                std::string s (phrase);
                size_t i = 0;
                while (i < s.size())
                {
                    size_t j = s.find (' ', i); if (j == std::string::npos) j = s.size();
                    const std::string w = s.substr (i, j - i);
                    for (const auto& e : lex) if (w == e.text) { ph.push_back (e.ph); tx.push_back (w); break; }
                    i = j + 1;
                }
                if (ph.empty()) continue;
                const Speaker& sp = speakers[(pi + rep) % 3];
                seed = seed * 1664525u + 1013904223u;
                speak (ph, sp, (float) bank->rate, seed, buf, ends);
                float peak = 1.0e-4f; for (float v : buf) peak = std::max (peak, std::abs (v));
                const int base = (int) bank->audio.size();
                bank->append (buf.data(), (int) buf.size(), 0.7f / peak);
                WordBank::Phrase phr { (int) bank->words.size(), 0 };
                int st = 0;
                for (size_t w = 0; w < ends.size(); ++w)
                {
                    WordBank::Seg sg;
                    sg.start = base + std::max (0, st - (int) (0.01 * bank->rate));
                    sg.end = base + std::min ((int) buf.size() - 1, ends[w] + (int) (0.05 * bank->rate));
                    sg.phrase = (int) bank->phrases.size();
                    bank->words.push_back (sg);
                    bank->text.push_back (tx[w]);
                    ++phr.count;
                    st = ends[w];
                }
                bank->phrases.push_back (phr);
                // a short silence between phrases
                for (int k = 0; k < (int) (0.2 * bank->rate); ++k) bank->audio.push_back (0);
            }
        return bank;
    }
};

//==============================================================================
// Plays the words: now and then a word or a phrase, slowed and worn like old tape.
struct WordsPlayer
{
    struct Voice
    {
        bool on = false, reverse = false;
        const WordBank* bank = nullptr;
        double pos = 0.0, a = 0.0, b = 0.0;     // now, and the part being played [a, b)
        double speed = 1.0;
        float gain = 1.0f, pan = 0.0f, delay = 0.0f;
        int repeats = 0, firstWord = 0, lastWord = 0;
        double repA = 0.0;
    };
    static constexpr int V = 3;
    Voice v[V];
    Random rng { 0x30BD5u };
    float sr = 44100.0f;
    float wowPh = 0.0f, flutPh = 0.0f, hpL = 0.0f, hpX = 0.0f, crushPh = 0.0f, crushV = 0.0f;
    Svf radioLp, radioHp;
    int coefCounter = 0;
    // for the editor: the word playing now, and how far through it is
    int nowWord = -1; float nowLevel = 0.0f; int played = 0;

    void prepare (float sampleRate) { sr = sampleRate; for (auto& x : v) x.on = false; }
    bool busy() const { for (auto& x : v) if (x.on) return true; return false; }

    // Start a word (count = 1) or a run of words; delaySec: wait before it starts.
    void start (const WordBank* bank, int first, int count, float speed, bool reverse, int repeats, float pan, float delaySec)
    {
        if (bank == nullptr || bank->words.empty()) return;
        first = std::max (0, std::min ((int) bank->words.size() - 1, first));
        const int last = std::min ((int) bank->words.size() - 1, first + std::max (1, count) - 1);
        int slot = 0;
        for (int i = 0; i < V; ++i) if (! v[i].on) { slot = i; break; }
        auto& x = v[slot];
        x = Voice {};
        x.on = true; x.bank = bank; x.reverse = reverse; x.repeats = repeats; x.pan = pan; x.delay = delaySec;
        x.firstWord = first; x.lastWord = last;
        x.a = bank->words[(size_t) first].start; x.b = bank->words[(size_t) last].end;
        x.speed = speed * bank->rate / sr;
        x.pos = reverse ? x.b : x.a;
        // a repeat catches the start of the word again (the first ~0.25 s), like a sampler retriggering
        x.repA = std::min (x.b, x.a + 0.25 * bank->rate);
        ++played;
    }

    // wobble: tape wow; radio: a small band-limited, crunchy speaker
    void process (float wobble, float radio, float& l, float& r)
    {
        if (coefCounter-- <= 0)
        {
            coefCounter = 64;
            radioLp.set (14000.0f * std::exp2 (-radio * 2.6f), 0.8f, sr);
            radioHp.set (80.0f + 520.0f * radio, 0.7f, sr);
        }
        wowPh += 0.55f / sr; if (wowPh >= 1.0f) wowPh -= 1.0f;
        flutPh += 7.3f / sr; if (flutPh >= 1.0f) flutPh -= 1.0f;
        const double wow = 1.0 + wobble * (0.022 * std::sin (kTwoPi * wowPh) + 0.004 * std::sin (kTwoPi * flutPh));
        float ol = 0.0f, orr = 0.0f, lvl = 0.0f;
        nowWord = -1;
        for (auto& x : v)
        {
            if (! x.on) continue;
            if (x.delay > 0.0f) { x.delay -= 1.0f / sr; continue; }
            const double len = std::max (1.0, x.b - x.a);
            const double through = x.reverse ? (x.b - x.pos) / len : (x.pos - x.a) / len;
            // soft edges so nothing clicks
            const double fadeS = 0.008 * x.bank->rate;
            const float edge = (float) std::min (1.0, std::min ((x.pos - x.a) / fadeS, (x.b - x.pos) / fadeS));
            const float y = x.bank->at (x.pos) * std::max (0.0f, edge) * x.gain;
            ol += y * std::sqrt (0.5f * (1.0f - x.pan));
            orr += y * std::sqrt (0.5f * (1.0f + x.pan));
            lvl = std::max (lvl, std::abs (y));
            // which word is this?
            for (int w = x.firstWord; w <= x.lastWord; ++w)
                if (x.pos >= x.bank->words[(size_t) w].start && x.pos < x.bank->words[(size_t) w].end) { nowWord = w; break; }
            x.pos += (x.reverse ? -1.0 : 1.0) * x.speed * wow;
            if (through >= 1.0 || x.pos <= x.a - 1.0 || x.pos >= x.b)
            {
                if (x.repeats > 0)   // caught repeating: back to the start of the word, a little quieter
                {
                    --x.repeats;
                    x.b = x.repA; x.lastWord = x.firstWord;
                    x.pos = x.reverse ? x.b : x.a;
                    x.gain *= 0.72f;
                }
                else x.on = false;
            }
        }
        // RADIO: band-limited and a little crushed
        float m = 0.5f * (ol + orr);
        if (radio > 0.001f)
        {
            float lp;
            radioHp.process (m, lp);
            const float band = radioLp.lowpass (m - lp);   // the lows taken out, then the highs
            crushPh += (16000.0f - 11000.0f * radio) / sr;
            if (crushPh >= 1.0f) { crushPh -= 1.0f; const float q = 256.0f - 200.0f * radio; crushV = std::round (softClip (band * (1.0f + 2.0f * radio)) * q) / q; }
            const float wet = radio;
            ol = ol * (1.0f - wet) + crushV * wet;
            orr = orr * (1.0f - wet) + crushV * wet;
        }
        nowLevel = lvl;
        l += ol; r += orr;
    }
};
}
