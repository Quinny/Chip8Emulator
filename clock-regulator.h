#include <chrono>
#include <iostream>
#include <thread>

// A class which helps regulate CPU/frame clocks. The `Tick` method will return
// true once every `milliseconds_per_cycle` milliseconds. `Tick` should be
// called first/early in your main program loop. E.g.
//
// void MainLoop() {
//   while (true) {
//     if (!regulator.Tick()) {
//        // continue or do non-clock synced work.
//     }
//   }
// }
class ClockRegulator {
public:
  using Clock = std::chrono::high_resolution_clock;

  ClockRegulator(int milliseconds_per_cycle)
      : milliseconds_per_cycle_(milliseconds_per_cycle),
        ready_at_(Clock::now()) {}

  bool Tick() {
    auto now = Clock::now();
    // If enough time has elapsed since the previous tick, update the next tick
    // to happen in `milliseconds_per_cycle_` milliseconds.
    if (now >= ready_at_) {
      ready_at_ = now + std::chrono::milliseconds(milliseconds_per_cycle_);
      return true;
    }
    return false;
  }

  int milliseconds_per_cycle_;
  std::chrono::time_point<std::chrono::high_resolution_clock> ready_at_;
};
