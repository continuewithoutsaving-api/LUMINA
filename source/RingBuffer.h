#pragma once

#include <atomic>
#include <cstddef>

namespace TrueLevel
{

/**
 * Lock-free Single-Producer Single-Consumer (SPSC) Circular Buffer.
 * Zero dynamic memory allocations during runtime.
 * Audio thread pushes without mutex locks or system calls.
 */
template <typename T, size_t Capacity>
class LockFreeRingBuffer
{
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    LockFreeRingBuffer() : writeIndex(0), readIndex(0) {}

    bool push(const T& item) noexcept
    {
        const size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
        const size_t currentRead  = readIndex.load(std::memory_order_acquire);

        if ((currentWrite - currentRead) >= Capacity)
            return false; // Queue full, drop sample safely

        buffer[currentWrite & IndexMask] = item;
        writeIndex.store(currentWrite + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) noexcept
    {
        const size_t currentRead  = readIndex.load(std::memory_order_relaxed);
        const size_t currentWrite = writeIndex.load(std::memory_order_acquire);

        if (currentRead == currentWrite)
            return false; // Queue empty

        item = buffer[currentRead & IndexMask];
        readIndex.store(currentRead + 1, std::memory_order_release);
        return true;
    }

    size_t getNumAvailable() const noexcept
    {
        const size_t r = readIndex.load(std::memory_order_relaxed);
        const size_t w = writeIndex.load(std::memory_order_relaxed);
        return (w >= r) ? (w - r) : 0;
    }

    void reset() noexcept
    {
        writeIndex.store(0, std::memory_order_relaxed);
        readIndex.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr size_t IndexMask = Capacity - 1;
    T buffer[Capacity];
    alignas(64) std::atomic<size_t> writeIndex;
    alignas(64) std::atomic<size_t> readIndex;
};

struct GoniometerSample
{
    float x; // Side: (L - R) / sqrt(2)
    float y; // Mid:  (L + R) / sqrt(2)
};

} // namespace TrueLevel