#include "../src/core.hpp"
#include <cassert>
#include <iostream>
int main() {
    yuuki::Countdown c;
    assert(!c.tick(100));
    c.start(100, 1000);
    assert(c.active() && c.remaining(600) == 500);
    c.togglePause(600);
    assert(c.paused() && c.remaining(5000) == 500 && !c.tick(5000));
    c.togglePause(5000);
    assert(!c.tick(5499) && c.remaining(5499) == 1);
    assert(c.tick(5500) && !c.active());
    assert(!c.tick(5501)); // completion is emitted once
    c.start(6000, 1000); c.stop();
    assert(!c.active() && c.remaining(7000) == 0 && !c.tick(7000));
    c.start(9000, 0); assert(!c.active());
    assert(yuuki::clampAxis(-2400, 200, -1920, 0) == -1920);
    assert(yuuki::clampAxis(-100, 200, -1920, 0) == -200);
    assert(yuuki::clampAxis(70, 900, 0, 800) == 0);
    yuuki::Affection a;
    assert(a.pat(100,20261022)==1 && a.value==21);
    assert(a.pat(1000,20261022)==0 && a.value==21);
    assert(a.add(2,20261022)==2 && a.value==23);
    assert(a.add(20,20261022)==7 && a.earned==10);
    assert(a.add(1,20261022)==0);
    assert(a.add(2,20261023)==2 && a.earned==2 && a.value==32);
    assert(a.add(2,20261022)==2 && a.earned==4); // moving system date backwards does not reset cap
    a.value=99; assert(a.add(5,20261024)==1 && a.value==100);
    assert(a.add(2,20261025)==0 && a.value==100); // absence never removes points
    std::cout << "Countdown, monitor bounds, affection cooldown, daily cap and offline persistence passed.\n";
}
