#ifndef SDL_PTRS_H
#define SDL_PTRS_H

#include <memory>

#include "SDL2/SDL.h"

namespace detail {

struct SdlWindowDeleter {
  void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }
};

struct SdlSurfaceDeleter {
  void operator()(SDL_Surface* surface) const { SDL_FreeSurface(surface); }
};

struct SdlRendererDeleter {
  void operator()(SDL_Renderer* renderer) const {
    SDL_DestroyRenderer(renderer);
  }
};

struct SdlTextureDeleter {
  void operator()(SDL_Texture* texture) const { SDL_DestroyTexture(texture); }
};
} // namespace detail

// Managed RAII pointers to SDL objects.
using SdlWindowPtr = std::unique_ptr<SDL_Window, detail::SdlWindowDeleter>;
using SdlSurfacePtr = std::unique_ptr<SDL_Surface, detail::SdlSurfaceDeleter>;
using SdlRendererPtr =
    std::unique_ptr<SDL_Renderer, detail::SdlRendererDeleter>;
using SdlTexturePtr = std::unique_ptr<SDL_Texture, detail::SdlTextureDeleter>;

#endif /* SDL_PTRS_H */
