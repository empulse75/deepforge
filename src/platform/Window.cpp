#include "platform/Window.hpp"

#include "core/Log.hpp"

#include <SDL3/SDL.h>

#include <utility>

namespace df {

Result<Window> Window::create(const VideoSettings& settings, const char* title) {
    Window w;
    w.mWindow = SDL_CreateWindow(title, settings.windowWidth, settings.windowHeight,
                                 SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (w.mWindow == nullptr) {
        return fail(Error{std::string("could not create window: ") + SDL_GetError(),
                          {},
                          {},
                          {},
                          "check that a display is available"});
    }
    w.mRenderer = SDL_CreateRenderer(w.mWindow, nullptr);
    if (w.mRenderer == nullptr) {
        SDL_DestroyWindow(w.mWindow);
        w.mWindow = nullptr;
        return fail(Error{std::string("could not create renderer: ") + SDL_GetError(), {}, {}, {}, {}});
    }
    SDL_SetRenderVSync(w.mRenderer, settings.vsync ? 1 : 0);
    if (settings.fullscreen) {
        SDL_SetWindowFullscreen(w.mWindow, true);
    }
    w.refresh_size();
    DF_LOG_INFO("platform", "window {}x{} (dpi scale {:.2f}), renderer '{}'", w.mWidth, w.mHeight,
                w.mDpiScale, SDL_GetRendererName(w.mRenderer));
    return w;
}

Window::Window(Window&& other) noexcept
    : mWindow(std::exchange(other.mWindow, nullptr)), mRenderer(std::exchange(other.mRenderer, nullptr)),
      mWidth(other.mWidth), mHeight(other.mHeight), mDpiScale(other.mDpiScale) {}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        this->~Window();
        mWindow = std::exchange(other.mWindow, nullptr);
        mRenderer = std::exchange(other.mRenderer, nullptr);
        mWidth = other.mWidth;
        mHeight = other.mHeight;
        mDpiScale = other.mDpiScale;
    }
    return *this;
}

Window::~Window() {
    if (mRenderer != nullptr) {
        SDL_DestroyRenderer(mRenderer);
        mRenderer = nullptr;
    }
    if (mWindow != nullptr) {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }
}

void Window::refresh_size() {
    if (mRenderer == nullptr) {
        return;
    }
    SDL_GetCurrentRenderOutputSize(mRenderer, &mWidth, &mHeight);
    int logicalW = 0;
    SDL_GetWindowSize(mWindow, &logicalW, nullptr);
    mDpiScale = logicalW > 0 ? static_cast<float>(mWidth) / static_cast<float>(logicalW) : 1.0f;
}

void Window::set_fullscreen(bool on) {
    if (mWindow != nullptr) {
        SDL_SetWindowFullscreen(mWindow, on);
        refresh_size();
    }
}

void Window::set_vsync(bool on) {
    if (mRenderer != nullptr) {
        SDL_SetRenderVSync(mRenderer, on ? 1 : 0);
    }
}

} // namespace df
