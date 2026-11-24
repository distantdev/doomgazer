#pragma once

#include "doomgazer/MathConstants.h"

#include <vector>

namespace doomgazer {

// Fixed ring allocated in prepare(). setLength() only changes the active wrap.
class RingBuffer
{
public:
    void assign(int capacity)
    {
        const int n = std::max(1, capacity);
        buffer.assign(static_cast<std::size_t>(n), 0.0);
        active = n;
        cursor = 0;
    }

    void clear() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0);
        cursor = 0;
    }

    void setLength(int length) noexcept
    {
        if (buffer.empty())
            return;
        active = std::max(1, std::min(length, static_cast<int>(buffer.size())));
        // Keep the read/write relationship instead of snapping the cursor to
        // zero; a shrinking reverb should not cut its tail.
        if (cursor >= active)
            cursor %= active;
    }

    int size() const noexcept { return active; }
    int capacity() const noexcept { return static_cast<int>(buffer.size()); }
    int cursorIndex() const noexcept { return cursor; }

    double atCursor() const noexcept
    {
        return buffer.empty() ? 0.0 : buffer[static_cast<std::size_t>(cursor)];
    }

    void writeCursor(double value) noexcept
    {
        if (!buffer.empty())
            buffer[static_cast<std::size_t>(cursor)] = value;
    }

    void advance() noexcept
    {
        if (active <= 0)
            return;
        if (++cursor >= active)
            cursor = 0;
    }

    // Integer delay relative to the current write cursor. Positive delay reads the past.
    double readSamplesBack(int delay) const noexcept
    {
        if (active <= 0)
            return 0.0;
        int pos = cursor - delay;
        while (pos < 0)
            pos += active;
        while (pos >= active)
            pos -= active;
        return buffer[static_cast<std::size_t>(pos)];
    }

    double readInterp(double delaySamps) const noexcept
    {
        if (active <= 1)
            return 0.0;
        double pos = static_cast<double>(cursor) - delaySamps;
        const double len = static_cast<double>(active);
        while (pos < 0.0)
            pos += len;
        while (pos >= len)
            pos -= len;
        const int i0 = static_cast<int>(std::floor(pos));
        const double frac = pos - static_cast<double>(i0);
        const int i1 = (i0 + 1 >= active) ? 0 : (i0 + 1);
        const double s0 = buffer[static_cast<std::size_t>(i0)];
        const double s1 = buffer[static_cast<std::size_t>(i1)];
        return s0 + frac * (s1 - s0);
    }

    double readWrapped(double position) const noexcept
    {
        if (active <= 0)
            return 0.0;
        double pos = position;
        const double len = static_cast<double>(active);
        while (pos < 0.0)
            pos += len;
        while (pos >= len)
            pos -= len;
        const int i0 = static_cast<int>(std::floor(pos));
        const double frac = pos - static_cast<double>(i0);
        const int i1 = (i0 + 1 >= active) ? 0 : (i0 + 1);
        const double s0 = buffer[static_cast<std::size_t>(i0)];
        const double s1 = buffer[static_cast<std::size_t>(i1)];
        return s0 + frac * (s1 - s0);
    }

    void writeWrapped(int index, double value) noexcept
    {
        if (buffer.empty())
            return;
        int i = index;
        const int cap = static_cast<int>(buffer.size());
        while (i < 0)
            i += cap;
        while (i >= cap)
            i -= cap;
        buffer[static_cast<std::size_t>(i)] = value;
    }

    int wrapIndex(int index) const noexcept
    {
        const int cap = static_cast<int>(buffer.size());
        if (cap <= 0)
            return 0;
        int i = index;
        while (i < 0)
            i += cap;
        while (i >= cap)
            i -= cap;
        return i;
    }

private:
    std::vector<double> buffer;
    int active = 0;
    int cursor = 0;
};

} // namespace doomgazer
