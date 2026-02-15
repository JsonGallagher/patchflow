#pragma once
#include <atomic>

namespace pf
{

class RenderConfig
{
public:
    static int getWidth()  { return width_.load (std::memory_order_relaxed); }
    static int getHeight() { return height_.load (std::memory_order_relaxed); }

    static void setResolution (int w, int h)
    {
        width_.store (w, std::memory_order_relaxed);
        height_.store (h, std::memory_order_relaxed);
    }

private:
    static inline std::atomic<int> width_  { 512 };
    static inline std::atomic<int> height_ { 512 };
};

} // namespace pf
