#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <random>

namespace yuuki {
enum class Pose { Idle, Walk, Greet, Read, Sleep, Grab, Pat, Eat, Anticipate,
                  Stretch, Yawn, Hair, Look, Swing, Think, Clap, Hop };
inline constexpr std::array<Pose,9> dailyPoses={Pose::Read,Pose::Sleep,Pose::Greet,
    Pose::Stretch,Pose::Yawn,Pose::Hair,Pose::Look,Pose::Swing,Pose::Think};
// Every daily pose appears once per shuffled round; round boundaries cannot repeat.
class ActionDeck {
    std::array<Pose,9> cards_=dailyPoses;
    std::size_t cursor_=cards_.size();
    Pose last_=Pose::Idle;
public:
    template<class Engine> Pose next(Engine& engine) {
        if(cursor_==cards_.size()) {
            std::shuffle(cards_.begin(),cards_.end(),engine);
            if(cards_[0]==last_)std::swap(cards_[0],cards_[1]);
            cursor_=0;
        }
        return last_=cards_[cursor_++];
    }
};
inline int actionRow(Pose pose) {
    return pose>=Pose::Stretch&&pose<=Pose::Hop?5+int(pose)-int(Pose::Stretch):-1;
}
inline unsigned actionDuration(Pose pose) {
    switch(pose) {
    case Pose::Stretch:return 3200;case Pose::Yawn:return 3200;
    case Pose::Hair:return 2800;case Pose::Look:return 3200;
    case Pose::Swing:return 4800;case Pose::Think:return 4400;
    case Pose::Clap:return 1600;case Pose::Hop:return 960;
    default:return 4500;
    }
}
inline int actionFrame(Pose pose,unsigned long long elapsed) {
    unsigned duration=actionDuration(pose);
    if(pose==Pose::Swing)return int(elapsed/300)%4;
    if(pose==Pose::Clap)return int(elapsed/200)%4;
    return std::min(3,int(elapsed/(duration/4)));
}
}
