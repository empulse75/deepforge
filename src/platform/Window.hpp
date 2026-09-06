// SDL window and renderer ownership with RAII.
#pragma once

#include "core/Result.hpp"
#include "platform/Settings.hpp"

struct SDL_Window;
struct SDL_Renderer;

namespace df {

class Window {
public:
    static Result<Window> create(const VideoSettings& settings, const char* title);

    Window() = default;
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    SDL_Window* handle() const noexcept { return mWindow; }

    SDL_Renderer* renderer() const noexcept { return mRenderer; }

    int width() const noexcept { return mWidth; }

    int height() const noexcept { return mHeight; }

    float dpi_scale() const noexcept { return mDpiScale; }

    /// Refreshes cached size after a resize event.
    void refresh_size();

    void set_fullscreen(bool on);

    void set_vsync(bool on);

private:
    SDL_Window* mWindow = nullptr;
    SDL_Renderer* mRenderer = nullptr;
    int mWidth = 0;
    int mHeight = 0;
    float mDpiScale = 1.0f;
};

} // namespace df
