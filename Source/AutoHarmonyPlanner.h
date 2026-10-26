#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

// Deterministic, bounded, allocation-free melody harmonisation. This is a
// key-aware arranger, not an estimate of the accompaniment's actual chords.
// Call only on pitch-detector revisions (normally about every 10 ms).
class AutoHarmonyPlanner
{
public:
    static constexpr int voices = 4;
    using Mask = std::array<bool, voices>;
    using Notes = std::array<int, voices>;

    void reset()
    {
        notes.fill(-1);
        previous.fill(-1);
        ready = false;
        pendingLead = lastLead = -1;
        pendingSeconds = quietSeconds = 0.0;
        chordDegree = -1;
        lastRoot = lastScale = lastVoicing = lastMotion = -1;
        lastMask.fill(false);
    }

    bool update(int lead, float confidence, double elapsedSeconds, int root,
                int scale, int voicing, int motion, const Mask& enabled)
    {
        const double dt = std::isfinite(elapsedSeconds)
            ? std::clamp(elapsedSeconds, 0.0, 0.1) : 0.0;
        if (!std::isfinite(confidence) || confidence < 0.58f || lead < 0 || lead > 127)
        {
            pendingLead = -1;
            pendingSeconds = 0.0;
            quietSeconds += dt;
            if (quietSeconds > 0.45 && ready)
            {
                reset();
                return true;
            }
            return false;
        }
        quietSeconds = 0.0;
        root = ((root % 12) + 12) % 12;
        scale = std::clamp(scale, 0, 2);
        voicing = std::clamp(voicing, 0, 2);
        motion = std::clamp(motion, 0, 2);
        const bool contextChanged = root != lastRoot || scale != lastScale
            || voicing != lastVoicing || motion != lastMotion || enabled != lastMask;
        if (lead != pendingLead)
        {
            pendingLead = lead;
            pendingSeconds = 0.0;
        }
        else
            pendingSeconds += dt;

        // Ignore a brief passing note. First entry and user context changes
        // need no extra dwell; the pitch tracker already supplies evidence.
        static constexpr double dwell[] = { 0.065, 0.030, 0.012 };
        if (ready && !contextChanged
            && (lead == lastLead || pendingSeconds + 1.0e-8 < dwell[motion]))
            return false;

        const auto old = notes;
        previous = notes;
        lastRoot = root; lastScale = scale; lastVoicing = voicing; lastMotion = motion;
        lastMask = enabled;
        lastLead = lead;
        arrange(lead, root, scale, voicing, enabled);
        ready = std::any_of(notes.begin(), notes.end(), [](int n) { return n >= 0; });
        return notes != old;
    }

    const Notes& getNotes() const { return notes; }
    bool isReady() const { return ready; }
    int getChordDegree() const { return chordDegree; } // -1 = chromatic fifths / inactive

private:
    static int pitchClass(int note) { return ((note % 12) + 12) % 12; }
    static constexpr int major[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int minor[7] = { 0, 2, 3, 5, 7, 8, 10 };
    struct Search
    {
        std::array<int, 9> candidates {};
        std::array<int, voices> slots {}, chosen {}, best {}, desired {};
        int count = 0, active = 0, lead = 60;
        float bestCost = std::numeric_limits<float>::max();
    };

    void choose(Search& s, int depth, int from, float cost)
    {
        if (depth == s.active)
        {
            if (cost < s.bestCost) { s.bestCost = cost; s.best = s.chosen; }
            return;
        }
        for (int i = from; i <= s.count - (s.active - depth); ++i)
        {
            const int note = s.candidates[(size_t) i];
            const int old = previous[(size_t) s.slots[(size_t) depth]];
            const int distance = std::abs(note - s.lead);
            float local = 0.24f * (float) std::abs(note - s.desired[(size_t) depth]);
            if (old >= 0)
            {
                const int move = std::abs(note - old);
                local += (float) move * 0.75f + (float) std::max(0, move - 5) * 1.2f;
            }
            if (distance == 1) local += 12.0f;
            if (distance == 2) local += 5.0f;
            if (depth > 0)
            {
                const int spacing = note - s.chosen[(size_t) depth - 1];
                if (spacing < 3) local += 5.0f;
                if (spacing > 12) local += (float) (spacing - 12);
            }
            s.chosen[(size_t) depth] = note;
            choose(s, depth + 1, i + 1, cost + local);
        }
    }

    void arrange(int lead, int root, int scale, int voicing, const Mask& enabled)
    {
        notes.fill(-1);
        Search base;
        base.lead = lead;
        for (int i = 0; i < voices; ++i)
            if (enabled[(size_t) i]) base.slots[(size_t) base.active++] = i;
        if (base.active == 0) { chordDegree = -1; return; }
        static constexpr int offsets[3][4][4] = {
            { {4,0,0,0}, {3,7,0,0}, {-5,3,7,0}, {-5,3,7,12} },
            { {7,0,0,0}, {-5,4,0,0}, {-12,4,7,0}, {-12,-5,4,9} },
            { {12,0,0,0}, {-12,7,0,0}, {-12,7,12,0}, {-12,-5,7,12} }
        };
        for (int i = 0; i < base.active; ++i)
            base.desired[(size_t) i] = lead + offsets[voicing][base.active - 1][i];

        // Chromatic contains no tonal context. Use consonant parallel fifths
        // and octaves rather than silently assuming C major.
        const int* steps = scale == 2 ? minor : major;
        const int lastChord = chordDegree;
        float best = std::numeric_limits<float>::max();
        Notes result; result.fill(-1);
        int selectedChord = -1;
        for (int degree = 0; degree < (scale == 0 ? 1 : 7); ++degree)
        {
            std::array<int, 3> chord {};
            if (scale == 0)
                chord = { pitchClass(lead), pitchClass(lead + 7), pitchClass(lead + 5) };
            else
                for (int j = 0; j < 3; ++j)
                    chord[(size_t) j] = pitchClass(root + steps[(degree + j * 2) % 7]);
            if (std::find(chord.begin(), chord.end(), pitchClass(lead)) == chord.end())
                continue;

            Search s = base;
            for (int n = std::max(0, lead - 12); n <= std::min(127, lead + 12); ++n)
                if (n != lead && std::find(chord.begin(), chord.end(), pitchClass(n)) != chord.end()
                    && s.count < (int) s.candidates.size())
                    s.candidates[(size_t) s.count++] = n;
            if (s.count < s.active) continue;
            static constexpr float preference[] = { 0.0f, 1.5f, 2.0f, 0.6f, 0.8f, 0.5f, 4.5f };
            float chordCost = scale == 0 ? 0.0f : preference[degree];
            if (ready && scale != 0 && lastChord != degree) chordCost += 3.5f;
            // Prefer roots/thirds to making a melody constantly the fifth.
            if (scale != 0 && pitchClass(lead) == chord[2]) chordCost += 0.7f;
            choose(s, 0, 0, chordCost);
            if (s.bestCost < best)
            {
                best = s.bestCost;
                result.fill(-1);
                for (int i = 0; i < s.active; ++i)
                    result[(size_t) s.slots[(size_t) i]] = s.best[(size_t) i];
                selectedChord = scale == 0 ? -1 : degree;
            }
        }
        notes = result;
        chordDegree = selectedChord;
    }

    Notes notes { {-1,-1,-1,-1} }, previous { {-1,-1,-1,-1} };
    Mask lastMask {};
    int lastLead = -1, pendingLead = -1, chordDegree = -1;
    int lastRoot = -1, lastScale = -1, lastVoicing = -1, lastMotion = -1;
    double pendingSeconds = 0.0, quietSeconds = 0.0;
    bool ready = false;
};
