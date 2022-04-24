#ifndef SCREEN_H
#define SCREEN_H

#include <SDL2/SDL.h>
#include <unordered_map>

#include "sdl-ptrs.h"

struct Color {
  int r, g, b;

  static const Color& Black() {
    static auto* c = new Color{.r = 0, .g = 0, .b = 0};
    return *c;
  }

  static const Color& White() {
    static auto* c = new Color{.r = 255, .g = 255, .b = 255};
    return *c;
  }
};

// A nice wrapper around an SDL screen.
class Screen {
public:
  Screen(const std::string& title) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(/* display_index = */ 0, &display_mode);
    width_ = display_mode.w;
    height_ = display_mode.h;

    window_.reset(SDL_CreateWindow(title.c_str(), /* x= */ 0, /* y= */ 0,
                                   width_, height_, SDL_WINDOW_SHOWN));
    window_surface_.reset(SDL_GetWindowSurface(window_.get()));

// For some reason unknown to me, the window renderer needs to be created
// in a different way depending on the platform. Doing it the wrong way
// will result in a window which is never updated.
#ifdef __APPLE__
    renderer_.reset(SDL_GetRenderer(window_.get()));
#else
    renderer_.reset(
        SDL_CreateRenderer(window_.get(), /* index = */ -1, /* flags= */ 0));
#endif
  }

  int width() { return width_; }
  int height() { return height_; }

  bool IsPressed(int key) { return key_pressed_[key]; }

  // Returns false if you should stop polling for events.
  bool PollEvent() {
    SDL_Event sdl_event;
    bool got_event = SDL_PollEvent(&sdl_event);
    if (!got_event) {
      return true;
    }

    switch (sdl_event.type) {
    case SDL_QUIT: {
      return false;
    }
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      key_pressed_[sdl_event.key.keysym.scancode] =
          sdl_event.type == SDL_KEYDOWN;
      break;
    }
    }
    return true;
  }

  void Update() { SDL_RenderPresent(renderer_.get()); }

  void DrawRects(const std::vector<SDL_Rect>& rects, const Color& c) {
    SDL_SetRenderDrawColor(renderer_.get(), c.r, c.g, c.b, 255);
    SDL_RenderFillRects(renderer_.get(), rects.data(), rects.size());
  }

  void Clear(const Color& c) {
    SDL_SetRenderDrawColor(renderer_.get(), c.r, c.g, c.b, 255);
    SDL_RenderClear(renderer_.get());
  }

  void Close() {
    if (!open_) {
      return;
    }

    window_surface_.reset();
    window_.reset();
    renderer_.reset();
    SDL_Quit();
    open_ = false;
  }

  ~Screen() { Close(); }

private:
  SdlWindowPtr window_;
  SdlSurfacePtr window_surface_;
  SdlRendererPtr renderer_;
  bool open_ = true;
  int width_;
  int height_;
  std::unordered_map<int, bool> key_pressed_;
};

#endif /* SCREEN_H */
