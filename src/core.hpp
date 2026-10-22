#pragma once
#include <cstdint>
#include <algorithm>

namespace yuuki {
struct Affection {
    int value=20, earned=0, day=0;
    std::uint64_t lastPat=0;
    bool hasPat=false;
    void refresh(int today) {
        value=std::clamp(value,0,100); earned=std::clamp(earned,0,10);
        if(today>day) {day=today;earned=0;}
    }
    int add(int amount,int today) {
        refresh(today);
        int gain=std::max(0,std::min({amount,10-earned,100-value}));
        value+=gain; earned+=gain; return gain;
    }
    int pat(std::uint64_t now,int today) {
        if(hasPat && now-lastPat<30000) return 0;
        hasPat=true;lastPat=now;return add(1,today);
    }
};
class Countdown {
    std::uint64_t deadline_ = 0, saved_ = 0;
    bool running_ = false, paused_ = false;
public:
    void start(std::uint64_t now, std::uint64_t duration) {
        deadline_ = now + duration; saved_ = 0; running_ = duration > 0; paused_ = false;
    }
    void stop() { deadline_ = saved_ = 0; running_ = paused_ = false; }
    bool active() const { return running_; }
    bool paused() const { return paused_; }
    std::uint64_t remaining(std::uint64_t now) const {
        return !running_ ? 0 : paused_ ? saved_ : now < deadline_ ? deadline_ - now : 0;
    }
    void togglePause(std::uint64_t now) {
        if (!running_) return;
        if (paused_) { deadline_ = now + saved_; paused_ = false; }
        else { saved_ = remaining(now); paused_ = true; }
    }
    bool tick(std::uint64_t now) {
        if (running_ && !paused_ && now >= deadline_) { stop(); return true; }
        return false;
    }
};
inline int clampAxis(int position, int size, int low, int high) {
    return std::max(low, std::min(position, std::max(low, high - size)));
}
}
