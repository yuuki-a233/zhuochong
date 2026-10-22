#include "../src/animation.hpp"
#include <cassert>
#include <iostream>
#include <set>
int main(){
    for(unsigned seed=0;seed<100;++seed){
        std::mt19937 engine(seed);yuuki::ActionDeck deck;auto previous=yuuki::Pose::Idle;
        for(int round=0;round<30;++round){
            std::set<yuuki::Pose> seen;
            for(int i=0;i<9;++i){auto p=deck.next(engine);assert(p!=previous);seen.insert(p);previous=p;}
            assert(seen.size()==9);assert(!seen.count(yuuki::Pose::Clap)&&!seen.count(yuuki::Pose::Hop));
        }
    }
    for(int i=0;i<8;++i){
        auto p=static_cast<yuuki::Pose>(int(yuuki::Pose::Stretch)+i);std::set<int> frames;
        for(unsigned t=0;t<yuuki::actionDuration(p);++t){int frame=yuuki::actionFrame(p,t);assert(frame>=0&&frame<4);frames.insert(frame);}
        assert(frames.size()==4&&yuuki::actionRow(p)==5+i);
    }
    std::cout<<"27000 daily selections cover every pose without adjacent repeats; interaction-only exclusions and all 32 frame timings passed.\n";
}
