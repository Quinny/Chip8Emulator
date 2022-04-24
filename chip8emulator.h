#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "clock-regulator.h"
#include "screen.h"

class Chip8Emulator {
public:
  Chip8Emulator(const std::string& rom_file_path)
      // Initialize 4kib of RAM memory and 16 1-byte registers.
      : memory_(4096, 0), variable_registers_(16, 0),
        screen_("Chip 8 Emulator"),
        // Execute at most 1 instruction per millisecond to emulate the speed
        // at which most Chip8 games were made to be run at. Without clock
        // regulation the games run way to fast.
        clock_regulator_(/* milliseconds_per_cycle = */ 1) {
    // The original Chip-8 interpreter stored the first byte of the program
    // at address 200 and so many programs rely on this.
    std::ifstream file_stream(rom_file_path);
    for (int program_load_location = 0x200; file_stream;
         ++program_load_location) {
      file_stream.read((char*)&memory_[program_load_location], 1);
    }

    // Initialize the display memory to 32x64 of all zeros.
    for (int row = 0; row < 32; ++row) {
      std::vector<unsigned char> columns(64, 0);
      display_.push_back(std::move(columns));
    }

    // Chip8 key codes range from 0x0 to 0xF (0-15). This mapping stores the
    // corresponding SDL scancode for each Chip8 code.
    key_mapping_ = {
        SDL_SCANCODE_1,
        SDL_SCANCODE_2,
        SDL_SCANCODE_3,
        SDL_SCANCODE_4,
        //
        SDL_SCANCODE_Q,
        SDL_SCANCODE_W,
        SDL_SCANCODE_E,
        SDL_SCANCODE_R,
        //
        SDL_SCANCODE_A,
        SDL_SCANCODE_S,
        SDL_SCANCODE_D,
        SDL_SCANCODE_F,
        //
        SDL_SCANCODE_Z,
        SDL_SCANCODE_X,
        SDL_SCANCODE_C,
        SDL_SCANCODE_V,
    };

    // A bitmapped font with characters 0-9 and A-F. Early Chip8 interpreters
    // stored this font starting at address 0x050.
    int font_load_location = font_address_;
    for (const auto font_byte : {
             0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
             0x20, 0x60, 0x20, 0x20, 0x70, // 1
             0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
             0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
             0x90, 0x90, 0xF0, 0x10, 0x10, // 4
             0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
             0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
             0xF0, 0x10, 0x20, 0x40, 0x40, // 7
             0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
             0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
             0xF0, 0x90, 0xF0, 0x90, 0x90, // A
             0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
             0xF0, 0x80, 0x80, 0x80, 0xF0, // C
             0xE0, 0x90, 0x90, 0x90, 0xE0, // D
             0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
             0xF0, 0x80, 0xF0, 0x80, 0x80  // F
         }) {
      memory_[font_load_location] = font_byte;
      ++font_load_location;
    }
  }

  // Chip8 Instructions commonly come either of form:
  //   - 0xTXYN or
  //   - 0xTXNN or
  //   - 0xTNNN or
  //   - 0xTXYN
  // where:
  //   - T is the type of instruction
  //   - X and Y are register indicies
  //   - N[NN] are integer "constants"
  //
  // The following functions define common bit masks for accessing parts of
  // the instructions
  int register1(uint16_t instruction) { return (instruction & 0x0F00) >> 8; }
  int register2(uint16_t instruction) { return (instruction & 0x00F0) >> 4; }
  int constant8(uint16_t instruction) { return (instruction & 0x00FF); }
  int constant12(uint16_t instruction) { return (instruction & 0x0FFF); }

  // Basic arithmetic functions which properly signal overflow into the 0xF
  // register.
  unsigned char add(unsigned char a, unsigned char b) {
    variable_registers_[0xF] = a + b > 0xFF;
    return a + b;
  }
  unsigned char subtract(unsigned char a, unsigned char b) {
    variable_registers_[0xF] = a >= b;
    return a - b;
  }

  // Prints the args to std::cout if DEBUG is defined with some common stream
  // manipulations that aid in debugging hex values.
  template <typename Arg, typename... Args>
  void Debug(Arg&& arg, Args&&... args) {
#ifdef DEBUG
    std::cout << std::hex << std::setfill('0') << std::setw(4)
              << std::forward<Arg>(arg);
    ((std::cout << std::forward<Args>(args)), ...);
    std::cout << std::endl;
#endif
  }

  // Executes the Chip8 program that was loaded from the file in the
  // constructor. This call will block until the graphics window is closed.
  void BlockingExecute() {
    while (screen_.PollEvent()) {
      // Regulate program speed to prevent the game from running too fast.
      if (!clock_regulator_.Tick()) {
        continue;
      }

      // Decrement the delay timer if it's set.
      if (delay_timer_ > 0) {
        delay_timer_--;
      }

      // Each Chip8 instruction is two bytes, so we read the next two bytes of
      // memory and then mask them into a single value to make handling easier.
      uint16_t instruction =
          (memory_[program_counter_] << 8) | (memory_[program_counter_ + 1]);
      Debug("Instruction 0x", instruction);
      program_counter_ += 2;

      switch (instruction & 0xF000) {
      case (0x0000): {
        auto flag = instruction & 0x000F;
        // Return from function instruction.
        if (flag == 0x000E) {
          program_counter_ = stack_.back();
          stack_.pop_back();
        } else if (flag == 0x0000) {
          // Clear screen instruction.
          for (auto& row : display_) {
            for (auto& cell : row) {
              cell = 0;
            }
          }
        }
        break;
      }

      // Unconditional jump.
      case (0x1000): {
        program_counter_ = constant12(instruction);
        break;
      }

      // Function call instruction.
      case (0x2000): {
        stack_.push_back(program_counter_);
        program_counter_ = constant12(instruction);
        break;
      }

      // Jump equal constant.
      case (0x3000): {
        if (variable_registers_[register1(instruction)] ==
            constant8(instruction)) {
          program_counter_ += 2;
        }
        break;
      }

      // Jump not equal.
      case (0x4000): {
        if (variable_registers_[register1(instruction)] !=
            constant8(instruction)) {
          program_counter_ += 2;
        }
        break;
      }

      // Jump equal register.
      case (0x5000): {
        if (variable_registers_[register1(instruction)] ==
            variable_registers_[register2(instruction)]) {
          program_counter_ += 2;
        }
        break;
      }

      // Set register.
      case (0x6000): {
        variable_registers_[register1(instruction)] = constant8(instruction);
        break;
      }

      // Add constant to register.
      case (0x7000): {
        variable_registers_[register1(instruction)] += constant8(instruction);
        break;
      }

      // Two register arithmetic.
      case (0x8000): {
        auto& vx = variable_registers_[register1(instruction)];
        auto& vy = variable_registers_[register2(instruction)];
        auto flag = instruction & 0x000F;
        if (flag == 0x0000) {
          vx = vy;
        } else if (flag == 0x0001) {
          vx |= vy;
        } else if (flag == 0x0002) {
          vx &= vy;
        } else if (flag == 0x0003) {
          vx ^= vy;
        } else if (flag == 0x0004) {
          vx = add(vx, vy);
        } else if (flag == 0x0005) {
          vx = subtract(vx, vy);
        } else if (flag == 0x0006) {
          vx >>= 1;
        } else if (flag == 0x0007) {
          vx = subtract(vy, vx);
        } else if (flag == 0x000E) {
          vx <<= 1;
        }
        break;
      }

      // Jump not equal register.
      case (0x9000): {
        if (variable_registers_[register1(instruction)] !=
            variable_registers_[register2(instruction)]) {
          program_counter_ += 2;
        }
        break;
      }

      // Index register set.
      case (0xA000): {
        index_register_ = constant12(instruction);
        break;
      }

      // Jump by offset.
      // TODO: Ambiguous
      case (0xB000): {
        program_counter_ = constant12(instruction) + variable_registers_[0];
        break;
      }

      // Generate random.
      case (0xC000): {
        variable_registers_[register1(instruction)] =
            (rand() % 255) & constant8(instruction);
        break;
      }

      // Draws the bitmap sprite pointed to by the index register to the
      // screen.
      case (0xD000): {
        auto row_start = variable_registers_[register2(instruction)] % 32;
        auto col_start = variable_registers_[register1(instruction)] % 64;
        auto height = instruction & 0x000F;
        variable_registers_[0xF] = 0;

        for (int sprite_row_offset = 0; sprite_row_offset < height;
             ++sprite_row_offset) {
          auto row = row_start + sprite_row_offset;
          if (row >= display_.size()) {
            break;
          }

          auto sprite_row = memory_[index_register_ + sprite_row_offset];
          for (int sprite_col_offset = 0; sprite_col_offset < 8;
               ++sprite_col_offset) {
            auto col = col_start + sprite_col_offset;
            if (col >= display_[row].size()) {
              break;
            }

            // Check if the `sprite_col_offset`th bit from the left is set.
            auto set = sprite_row & (1 << (7 - sprite_col_offset));
            auto before = display_[row][col];
            if (set) {
              display_[row][col] ^= 1;
              if (before) {
                variable_registers_[0xF] = 1;
              }
            }
          }
        }

        // Determine the scaling factors required to fit the chip8 display
        // memory fully to the screen.
        auto x_scale = screen_.width() / display_.front().size();
        // Leave some space at the bottom of the screen to prevent clipping.
        auto y_scale = (screen_.height() / display_.size()) - 3;

        // Generate a vector of all the filled rectangles that need to be drawn.
        std::vector<SDL_Rect> rects_to_draw;
        for (int row = 0; row < display_.size(); ++row) {
          for (int col = 0; col < display_[row].size(); ++col) {
            if (display_[row][col]) {
              SDL_Rect r;
              r.x = col * x_scale;
              r.y = row * y_scale;
              r.w = x_scale;
              r.h = y_scale;
              rects_to_draw.push_back(r);
            }
          }
        }

        // Update the screen.
        screen_.Clear(Color::Black());
        screen_.DrawRects(rects_to_draw, Color::White());
        screen_.Update();
        break;
      }

      // Skip instructions based on key state.
      case (0xE000): {
        auto skip_state = (instruction & 0x00FF) == 0X009E;
        auto key_code =
            key_mapping_[variable_registers_[register1(instruction)]];
        if (screen_.IsPressed(key_code) == skip_state) {
          program_counter_ += 2;
        }
        break;
      }

      // The F* instructions are a bit of grab bag...
      case (0xF000): {
        auto flag = instruction & 0x00FF;
        if (flag == 0x0007) {
          variable_registers_[register1(instruction)] = delay_timer_;
        } else if (flag == 0x0015) {
          delay_timer_ = variable_registers_[register1(instruction)];
        } else if (flag == 0x0018) {
          std::cout << "\a" << std::endl;
        } else if (flag == 0x000A) {
          if (!screen_.IsPressed(key_mapping_[0])) {
            program_counter_ -= 2;
          } else {
            variable_registers_[register1(instruction)] = 0;
          }
        } else if (flag == 0x001E) {
          index_register_ += variable_registers_[register1(instruction)];
        } else if (flag == 0x0029) {
          constexpr int kFontCharacterHeight = 5;
          index_register_ =
              font_address_ +
              (variable_registers_[register1(instruction)] & 0x000F) *
                  kFontCharacterHeight;
        } else if (flag == 0x0033) {
          auto vx = variable_registers_[register1(instruction)];
          memory_[index_register_] = vx / 100;
          vx %= 100;
          memory_[index_register_ + 1] = vx / 10;
          memory_[index_register_ + 2] = vx % 10;
        } else if (flag == 0x0055) {
          for (int i = 0; i <= register1(instruction); ++i) {
            memory_[index_register_ + i] = variable_registers_[i];
          }
        } else if (flag == 0x0065) {
          for (int i = 0; i <= register1(instruction); ++i) {
            variable_registers_[i] = memory_[index_register_ + i];
          }
        }
        break;
      }

      default: {
        std::cout << "Unknown instruction" << std::endl;
      }
      }
    }
  }

private:
  std::vector<unsigned char> memory_;
  std::vector<uint16_t> stack_;
  uint16_t program_counter_ = 0x200;
  std::vector<unsigned char> variable_registers_;
  int index_register_ = 0;
  std::vector<std::vector<unsigned char>> display_;
  Screen screen_;
  std::vector<int> key_mapping_;
  int delay_timer_ = 0;
  ClockRegulator clock_regulator_;
  int font_address_ = 0x050;
};
