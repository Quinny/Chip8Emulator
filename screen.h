#ifndef SCREEN_H
#define SCREEN_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <unordered_map>

#include "sdl-ptrs.h"

struct Color {
  unsigned char r, g, b;

  static const Color& Black() {
    static auto* c = new Color{.r = 0, .g = 0, .b = 0};
    return *c;
  }

  static const Color& White() {
    static auto* c = new Color{.r = 255, .g = 255, .b = 255};
    return *c;
  }

  static const Color& Red() {
    static auto* c = new Color{.r = 255, .g = 0, .b = 0};
    return *c;
  }
};

// A nice wrapper around an SDL screen.
class Screen {
public:
  Screen(const std::string& title) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(/* display_index = */ 0, &display_mode);
    width_ = display_mode.w;
    height_ = display_mode.h;
    font_ = TTF_OpenFont("./font.ttf", /* size = */ 24);

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

  // Check if the provided key is currently pressed.
  bool IsPressed(SDL_Scancode key) {
    return *(SDL_GetKeyboardState(/* numkeys = */ nullptr) + key);
  }

  // Fire `handler` when to provided key is pressed down.
  void OnKeyDown(SDL_Scancode key, std::function<void()> handler) {
    key_down_handlers_[key].push_back(std::move(handler));
  }

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
    case SDL_KEYDOWN: {
      for (const auto& handler :
           key_down_handlers_[sdl_event.key.keysym.scancode]) {
        handler();
      }
      break;
    }
    }
    return true;
  }

  // Commit all rendering and update the screen.
  void Update() { SDL_RenderPresent(renderer_.get()); }

  // Draw the provided rects to the screen.
  void DrawRects(const std::vector<SDL_Rect>& rects, const Color& c) {
    SDL_SetRenderDrawColor(renderer_.get(), c.r, c.g, c.b, 255);
    SDL_RenderFillRects(renderer_.get(), rects.data(), rects.size());
  }

  // Draws the provided text to the screen at the given x,y coordinates and
  // returns the rect which encapsulates the text. The returned rect can be used
  // to make sure other text or objects don't collide with the text.
  SDL_Rect DrawText(const std::string& text, int x, int y, const Color& c) {
    SDL_Color color = {c.r, c.g, c.b};
    auto message_surface =
        SdlSurfacePtr(TTF_RenderText_Solid(font_, text.c_str(), color));
    auto message_texture = SdlTexturePtr(
        SDL_CreateTextureFromSurface(renderer_.get(), message_surface.get()));

    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    TTF_SizeText(font_, text.c_str(), &rect.w, &rect.h);
    SDL_RenderCopy(renderer_.get(), message_texture.get(),
                   /* crop_rect= */ nullptr, &rect);
    return rect;
  }

  // Clear the screen with the provided color.
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
    TTF_Quit();
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
  std::unordered_map<SDL_Scancode, std::vector<std::function<void()>>>
      key_down_handlers_;
  TTF_Font* font_;
};

#endif /* SCREEN_H */
