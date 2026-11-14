#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gs {
// Single audio writer, UI readers. Atomic cells permit overwrite without a race.
// Snapshots are best-effort visual telemetry; audio never waits for the display.
class Scope {
    static constexpr unsigned capacity=512;
    std::array<std::atomic<int32_t>,capacity> ring{};
    std::atomic<uint32_t> published{0};
    uint32_t cursor=0;
    unsigned decimation=0;
public:
    static_assert(std::atomic<int32_t>::is_always_lock_free);
    void push(const float* samples,unsigned count){
        for(unsigned i=0;i<count;i++)if(decimation++%4==0){
            float x=std::isfinite(samples[i])?std::clamp(samples[i],-1.f,1.f):0;
            ring[cursor++&(capacity-1)].store(int32_t(x*32767),std::memory_order_relaxed);
        }
        published.store(cursor,std::memory_order_release);
    }
    std::array<float,128> snapshot()const{
        std::array<float,256> window{};
        uint32_t end=published.load(std::memory_order_acquire);
        for(unsigned i=0;i<256;i++)window[i]=ring[(end-256+i)&(capacity-1)].load(std::memory_order_relaxed)/32767.f;
        // GlitchDrums-FM1: rising crossing, peak-normalized trace and quiet floor.
        unsigned start=0;
        for(unsigned i=1;i<128;i++)if(window[i-1]<=0&&window[i]>0){start=i;break;}
        std::array<float,128> result{};
        std::copy_n(window.begin()+start,128,result.begin());return result;
    }
};
}
