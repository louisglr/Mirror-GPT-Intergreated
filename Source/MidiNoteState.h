#pragma once

#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstddef>

// Fixed-size, channel-aware ownership. One channel's release/pedal/panic must
// never release another channel's copy of the same note. Repeated Note On on
// one channel deliberately retriggers rather than accumulating stuck notes.
class MidiNoteState
{
public:
    void reset() { *this = MidiNoteState {}; }

    void noteOn(int channel, int note, float velocity)
    {
        if (!valid(channel, note)) return;
        const auto bit = channelBit(channel);
        down[(size_t) note] |= bit;
        sounding[(size_t) note] |= bit;
        velocities[(size_t) (channel - 1)][(size_t) note] = std::isfinite(velocity) ? std::clamp(velocity, 0.0f, 1.0f) : 0.0f;
    }

    void noteOff(int channel, int note)
    {
        if (!valid(channel, note)) return;
        const auto bit = channelBit(channel);
        down[(size_t) note] &= (std::uint16_t) ~bit;
        if (!pedals[(size_t) (channel - 1)])
            sounding[(size_t) note] &= (std::uint16_t) ~bit;
    }

    void sustain(int channel, bool enabled)
    {
        if (!valid(channel, 0)) return;
        pedals[(size_t) (channel - 1)] = enabled;
        if (!enabled)
            for (size_t note = 0; note < sounding.size(); ++note)
                if ((down[note] & channelBit(channel)) == 0)
                    sounding[note] &= (std::uint16_t) ~channelBit(channel);
    }

    void allNotesOff(int channel)
    {
        for (int note = 0; note < 128; ++note) noteOff(channel, note);
    }

    void allSoundOff(int channel)
    {
        if (!valid(channel, 0)) return;
        const auto mask = (std::uint16_t) ~channelBit(channel);
        for (size_t note = 0; note < sounding.size(); ++note)
        {
            down[note] &= mask;
            sounding[note] &= mask;
        }
    }

    template <size_t N>
    int collect(std::array<int, N>& notes, std::array<float, N>& gains) const
    {
        int count = 0;
        for (int note = 0; note < 128 && count < (int) N; ++note)
        {
            if (sounding[(size_t) note] == 0) continue;
            float velocity = 0.0f;
            for (int channel = 1; channel <= 16; ++channel)
                if ((sounding[(size_t) note] & channelBit(channel)) != 0)
                    velocity = std::max(velocity, velocities[(size_t) (channel - 1)][(size_t) note]);
            notes[(size_t) count] = note;
            gains[(size_t) count++] = velocity;
        }
        return count;
    }

private:
    static bool valid(int channel, int note)
    { return channel >= 1 && channel <= 16 && note >= 0 && note < 128; }
    static std::uint16_t channelBit(int channel)
    { return (std::uint16_t) (1u << (unsigned) (channel - 1)); }
    std::array<std::uint16_t, 128> down {}, sounding {};
    std::array<bool, 16> pedals {};
    std::array<std::array<float, 128>, 16> velocities {};
};
