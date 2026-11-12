#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "TR8Voices.h"

// ============================================================================
//  TR8Sequencer : séquenceur de la TR-8S, sans dépendance à JUCE.
//
//  - 16 pas de double-croche, 8 variations A à H + 2 fill-in par pattern
//  - fin de variation (Last Step) réglable, et fin de piste par instrument
//  - piste d'accent, notes faibles ("weak beats")
//  - plusieurs variations sélectionnées : elles jouent à la suite, de A vers H
//  - shuffle, fill-in manuel
// ============================================================================

namespace tr8
{
struct Pattern
{
    static constexpr int kVariations = 8;
    static constexpr int kFills      = 2;
    static constexpr int kSlots      = kVariations + kFills;   // 0..7 = A..H, 8..9 = fill-in 1..2
    static constexpr int kSteps      = 16;

    uint16_t step[kSlots][kNumInst];    // 1 bit par pas : l'instrument joue
    uint16_t weak[kSlots][kNumInst];    // 1 bit par pas : note faible
    uint16_t accent[kSlots];            // piste d'accent
    int      last[kSlots];              // dernier pas de la variation (1..16)
    int      trackLast[kNumInst];       // dernier pas propre à une piste (0 = suit la variation)

    Pattern() { clear(); }

    void clear()
    {
        std::memset (step, 0, sizeof (step));
        std::memset (weak, 0, sizeof (weak));
        std::memset (accent, 0, sizeof (accent));
        for (int& l : last) l = kSteps;
        for (int& t : trackLast) t = 0;
    }

    void set (int slot, Inst i, int s, bool on = true, bool isWeak = false)
    {
        const uint16_t m = (uint16_t) (1u << s);
        if (on) step[slot][i] |= m; else step[slot][i] &= (uint16_t) ~m;
        if (isWeak) weak[slot][i] |= m; else weak[slot][i] &= (uint16_t) ~m;
    }

    // Sérialisation en texte hexadécimal (pour sauvegarder l'état du plugin)
    std::string toHex() const
    {
        std::string out;
        auto put = [&] (unsigned v) { char b[8]; std::snprintf (b, sizeof (b), "%04x", v & 0xffffu); out += b; };
        for (auto& a : step)   for (auto v : a) put (v);
        for (auto& a : weak)   for (auto v : a) put (v);
        for (auto v : accent)  put (v);
        for (auto v : last)    put ((unsigned) v);
        for (auto v : trackLast) put ((unsigned) v);
        return out;
    }

    bool fromHex (const std::string& s)
    {
        const size_t count = (size_t) (kSlots * kNumInst * 2 + kSlots + kSlots + kNumInst);
        if (s.size() < count * 4) return false;

        size_t pos = 0;
        auto get = [&]() -> unsigned
        {
            unsigned v = 0;
            for (int i = 0; i < 4; ++i)
            {
                const char c = s[pos++];
                v = v * 16 + (unsigned) (c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0');
            }
            return v;
        };
        for (auto& a : step)   for (auto& v : a) v = (uint16_t) get();
        for (auto& a : weak)   for (auto& v : a) v = (uint16_t) get();
        for (auto& v : accent) v = (uint16_t) get();
        for (auto& v : last)   { v = (int) get(); if (v < 1 || v > kSteps) v = kSteps; }
        for (auto& v : trackLast) { v = (int) get(); if (v < 0 || v > kSteps) v = 0; }
        return true;
    }

    void setAccent (int slot, int s, bool on = true)
    {
        const uint16_t m = (uint16_t) (1u << s);
        if (on) accent[slot] |= m; else accent[slot] &= (uint16_t) ~m;
    }
};

class Sequencer
{
public:
    Pattern pattern;

    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void setTempo (float bpm)         { tempo = std::clamp (bpm, 40.0f, 300.0f); }
    void setShuffle (float s01)       { shuffle = std::clamp (s01, 0.0f, 1.0f); }
    void setAccentLevel (float a01)   { accentLevel = std::clamp (a01, 0.0f, 1.0f); }
    void setVariationMask (uint8_t m) { mask = m ? m : 1; }
    uint8_t getVariationMask() const  { return mask; }

    void start()
    {
        reset();
        running = true;
        curSlot = firstSelected();
    }
    // Démarre aligné sur une position de l'hôte : "step" est le numéro de double-croche global,
    // "delaySamples" le temps à attendre avant ce pas.
    void startAt (long long step, double delaySamples)
    {
        reset();
        running = true;
        curSlot = firstSelected();
        globalStep = std::max (0LL, step);
        stepIndex = (int) (globalStep % pattern.last[curSlot]);
        nextBoundary = std::max (0.0, delaySamples);
    }
    void stop()        { running = false; }
    bool isRunning() const { return running; }

    // Insère un fill-in (0 ou 1) au prochain début de variation.
    void triggerFill (int which) { pendingFill = Pattern::kVariations + std::clamp (which, 0, Pattern::kFills - 1); }

    int getCurrentStep() const   { return lastFiredStep; }
    int getCurrentSlot() const   { return curSlot; }

    // Avance de n échantillons ; onTrigger (offset, instrument, vélocité 0..1) est appelé pour chaque note.
    template <class F>
    void advance (int n, F&& onTrigger)
    {
        if (! running) { return; }

        const double stepLen = fs * 60.0 / ((double) tempo * 4.0);

        while (true)
        {
            const double shuffleOffset = (globalStep & 1) ? (double) shuffle * stepLen / 3.0 : 0.0;
            const double trigTime = nextBoundary + shuffleOffset;
            if (trigTime >= now + n) break;

            const int offset = std::max (0, (int) (trigTime - now));
            fireStep (offset, onTrigger);

            nextBoundary += stepLen;
            ++globalStep;
        }
        now += n;
    }

private:
    void reset()
    {
        now = 0.0; nextBoundary = 0.0; globalStep = 0; stepIndex = 0; lastFiredStep = 0;
        curSlot = firstSelected(); pendingFill = -1;
    }

    int firstSelected() const
    {
        for (int i = 0; i < Pattern::kVariations; ++i) if (mask & (1u << i)) return i;
        return 0;
    }

    int nextSelected (int from) const
    {
        for (int k = 1; k <= Pattern::kVariations; ++k)
        {
            const int i = (from + k) % Pattern::kVariations;
            if (mask & (1u << i)) return i;
        }
        return 0;
    }

    template <class F>
    void fireStep (int offset, F& onTrigger)
    {
        const int varLen = pattern.last[curSlot];
        const bool accented = (pattern.accent[curSlot] >> (stepIndex % varLen)) & 1;

        for (int i = 0; i < kNumInst; ++i)
        {
            const int len = pattern.trackLast[i] > 0 ? pattern.trackLast[i] : varLen;
            const int s   = stepIndex % len;

            if ((pattern.step[curSlot][i] >> s) & 1)
            {
                const bool isWeak = (pattern.weak[curSlot][i] >> s) & 1;
                float vel = isWeak ? 0.45f : 0.78f;
                if (accented) vel = std::min (1.0f, vel + 0.10f + 0.12f * accentLevel);
                onTrigger (offset, (Inst) i, vel);
            }
        }

        lastFiredStep = stepIndex % varLen;
        ++stepIndex;

        if (stepIndex >= varLen)
        {
            stepIndex = 0;

            if (inFill)                      // le fill-in vient de se terminer : on reprend le cycle normal
            {
                curSlot = fillReturn;
                inFill = false;
            }
            else if (pendingFill >= 0)       // un fill-in est demandé : il joue une fois
            {
                fillReturn = nextSelected (curSlot);
                curSlot = pendingFill;
                pendingFill = -1;
                inFill = true;
            }
            else
            {
                curSlot = nextSelected (curSlot);
            }
        }
    }

    double fs = 44100.0, now = 0.0, nextBoundary = 0.0;
    long long globalStep = 0;
    int stepIndex = 0, curSlot = 0, lastFiredStep = 0, pendingFill = -1, fillReturn = 0;
    bool inFill = false, running = false;
    float tempo = 120.0f, shuffle = 0.0f, accentLevel = 0.5f;
    uint8_t mask = 1;
};
} // namespace tr8
