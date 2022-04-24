#include "chip8emulator.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " <rom file>" << std::endl;
    return 1;
  }
  Chip8Emulator emulator(argv[1]);
  emulator.BlockingExecute();
}
