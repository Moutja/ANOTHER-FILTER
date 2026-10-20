#pragma once
// Oscilloscope plumbing (no SDK dependency).
//  - ScopeBuffer: lock-free ring the audio thread writes and the GUI reads.
//  - ScopeRegistry: lets the controller find the processor's buffer by id (the id travels in a VST3 message).
//  - scopeWindow(): picks a steady, triggered slice of the latest audio to draw.

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

namespace MS20 {

class ScopeBuffer
{
public:
    static constexpr int kSize = 8192;  // power of two
    static constexpr unsigned kMask = kSize - 1;

    // audio thread: stores the mono mix of the block
    void push(const float* l, const float* r, int n)
    {
        unsigned w = wr_.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
            data_[(w + i) & kMask].store(r ? 0.5f * (l[i] + r[i]) : l[i], std::memory_order_relaxed);
        wr_.store(w + static_cast<unsigned>(n), std::memory_order_release);
    }

    // GUI thread: copies the newest `count` samples, oldest first
    void latest(float* out, int count) const
    {
        const unsigned w = wr_.load(std::memory_order_acquire);
        for (int i = 0; i < count; ++i)
            out[i] = data_[(w - static_cast<unsigned>(count) + static_cast<unsigned>(i)) & kMask].load(std::memory_order_relaxed);
    }

private:
    std::atomic<float> data_[kSize];
    std::atomic<unsigned> wr_{0};
};

class ScopeRegistry
{
public:
    static std::shared_ptr<ScopeBuffer> create(std::int64_t& idOut)
    {
        std::lock_guard<std::mutex> lock(mutex());
        auto& m = map();
        for (auto it = m.begin(); it != m.end();)       // forget buffers whose plugin instance is gone
            it = it->second.expired() ? m.erase(it) : std::next(it);
        idOut = ++counter();
        auto p = std::make_shared<ScopeBuffer>();
        m[idOut] = p;
        return p;
    }

    static std::shared_ptr<ScopeBuffer> find(std::int64_t id)
    {
        std::lock_guard<std::mutex> lock(mutex());
        auto it = map().find(id);
        return it == map().end() ? nullptr : it->second.lock();
    }

private:
    static std::mutex& mutex() { static std::mutex m; return m; }
    static std::map<std::int64_t, std::weak_ptr<ScopeBuffer>>& map() { static std::map<std::int64_t, std::weak_ptr<ScopeBuffer>> m; return m; }
    static std::int64_t& counter() { static std::int64_t c = 0; return c; }
};

// Fills out[0..outCount) from in[0..inCount) starting at the first rising zero crossing found in the first
// half of `in` (so the trace holds still), and returns the start index used (0 when nothing triggers).
inline int scopeWindow(const float* in, int inCount, float* out, int outCount)
{
    int start = 0;
    const int searchEnd = inCount - outCount;
    for (int i = 1; i < searchEnd; ++i)
        if (in[i - 1] < 0.0f && in[i] >= 0.0f) { start = i; break; }
    for (int i = 0; i < outCount; ++i)
        out[i] = in[start + i];
    return start;
}

} // namespace MS20
