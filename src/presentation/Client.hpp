// The game client: window, input, audio, screens and the fixed-step loop.
#pragma once

#include "app/GameSession.hpp"
#include "platform/Audio.hpp"
#include "platform/Input.hpp"
#include "platform/Window.hpp"
#include "presentation/Draw.hpp"
#include "presentation/WorldRenderer.hpp"

#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace df {

enum class Screen : u8 {
    MainMenu,
    NewGame,
    LoadGame,
    Hub,
    Garage,
    Modules,
    Skills,
    Research,
    Refinery,
    Contracts,
    Facilities,
    Codex,
    Expedition,
    Map,
    Pause,
    Results,
    Failure,
    Settings,
    Bindings,
    Credits,
    Error,
};

struct MenuEntry {
    std::string label;
    std::string detail;
    std::string hint;
    bool enabled = true;
    int id = 0;
    Color accent = palette::kText;
};

/// A short-lived on-screen message (also used for audio captions).
struct Toast {
    std::string text;
    float life = 0.0f;
    Color color = palette::kText;
};

/// One contextual tutorial step (Section 9).
struct TutorialStep {
    std::string id;
    std::string title;
    std::string body;
    bool shown = false;
};

class Client {
public:
    Client(ContentDatabase db, std::filesystem::path userDir);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    Status initialise();

    /// Runs until the player quits. Returns the process exit code.
    int run();

    /// Runs a fixed number of frames without user input; used by smoke tests.
    int run_headless_frames(int frames);

    /// QA helper: starts a scripted world, plays it for `ticks`, then writes a
    /// screenshot. Used by the automated visual smoke test.
    Status capture_demo(const std::filesystem::path& outFile, u64 seed, int ticks, Screen screen);

private:
    void poll_events();
    void update(float frameSeconds);
    void render(float alpha);

    void build_menu();
    void activate(int id);
    void navigate(int delta);
    void push_toast(std::string text, Color c = palette::kText);
    void play(Sfx s, float pitch = 0.0f);
    void drain_domain_events();
    void update_tutorials();
    void set_screen(Screen s);

    void render_hud(Draw& d, const RenderSnapshot& snap);
    void render_menu(Draw& d, std::string_view title, std::string_view subtitle);
    void render_map(Draw& d);
    void render_debug(Draw& d);

    std::string format_stat_delta(float before, float after, bool lowerIsBetter = false) const;

    ContentDatabase mDb;
    std::filesystem::path mUserDir;
    Settings mSettings;
    Window mWindow;
    std::unique_ptr<InputSystem> mInput;
    std::unique_ptr<AudioSystem> mAudio;
    std::unique_ptr<GameSession> mSession;
    WorldRenderer mRenderer;
    Camera mCamera;

    Screen mScreen = Screen::MainMenu;
    Screen mPreviousScreen = Screen::MainMenu;
    std::vector<MenuEntry> mMenu;
    int mSelected = 0;
    int mScroll = 0;
    std::string mMenuTitle;
    std::string mMenuSubtitle;
    std::string mStatusLine;
    std::string mErrorText;

    std::deque<Toast> mToasts;
    std::vector<TutorialStep> mTutorials;
    std::string mActiveTutorial;

    bool mRunning = true;
    bool mDebugOverlay = false;
    bool mAwaitingRebind = false;
    Action mRebindAction = Action::Count;
    u64 mSeedInput = 0;
    bool mUseRandomSeed = true;
    int mLoadSlot = 0;

    double mAccumulator = 0.0;
    double mFrameTimeMs = 0.0;
    double mTickTimeMs = 0.0;
    u64 mFrameCount = 0;
};

} // namespace df
