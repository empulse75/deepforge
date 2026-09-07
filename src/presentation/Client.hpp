// The game client: window, input, audio, screens and the fixed-step loop.
#pragma once

#include "app/GameSession.hpp"
#include "platform/Audio.hpp"
#include "platform/Input.hpp"
#include "platform/Window.hpp"
#include "presentation/Draw.hpp"
#include "presentation/Ui.hpp"
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

/// A short-lived on-screen message. Also carries audio captions when the
/// subtitles accessibility option is on.
struct Toast {
    std::string text;
    Icon icon = Icon::None;
    float life = 0.0f;
    float age = 0.0f;
    Color color = theme::kText;
};

/// One contextual tutorial card (Section 9).
struct TutorialStep {
    std::string id;
    std::string title;
    std::string body;
    Icon icon = Icon::None;
    bool shown = false;
};

class Client {
public:
    Client(ContentDatabase db, std::filesystem::path userDir);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    Status initialise();

    int run();

    /// Runs a fixed number of frames without user input; used by smoke tests.
    int run_headless_frames(int frames);

    /// QA helper: plays a scripted world, then writes a screenshot.
    Status capture_demo(const std::filesystem::path& outFile, u64 seed, int ticks, Screen screen);

private:
    void poll_events();
    void update(float frameSeconds);
    void render(float alpha);

    void build_screen();
    void activate(int id);
    void navigate(int delta);
    void push_toast(std::string text, Color c = theme::kText, Icon icon = Icon::None);
    void play(Sfx s, float pitch = 0.0f);
    void drain_domain_events();
    void update_tutorials();
    void set_screen(Screen s);
    /// Settings sliders: one range table drives the row, the keys and the drag.
    float slider_fraction(int id) const;
    float slider_value(int id) const;
    void set_slider(int id, float fraction);
    float ui_scale() const;

    /// Authored player-facing text for a content key. Falls back to a tidied
    /// form of the key itself, so a gap in the string table is visible rather
    /// than blank - the content loader rejects such gaps anyway.
    std::string name_of(std::string_view key) const;
    /// Display name for a content id (e.g. "material.ferrite").
    std::string id_name(std::string_view id) const;
    /// Rewrites a domain refusal into player-facing prose, replacing any content
    /// id it names with that content's display name.
    std::string humanise(std::string_view reason) const;
    /// Authored description for a content key, or an empty string.
    std::string body_of(std::string_view key) const;

    // --- screen painters ---------------------------------------------------
    void paint_main_menu(Draw& d);
    void paint_menu_screen(Draw& d);
    void paint_hud(Draw& d, const RenderSnapshot& snap);
    void paint_map(Draw& d);
    void paint_debug(Draw& d);
    void paint_toasts(Draw& d, float x, float bottom, bool overWorld);
    void paint_tutorial_card(Draw& d, const Rect& area);

    // --- screen builders ---------------------------------------------------
    void build_main_menu();
    void build_new_game();
    void build_load_game();
    void build_hub();
    void build_garage();
    void build_modules();
    void build_skills();
    void build_research();
    void build_refinery();
    void build_contracts();
    void build_facilities();
    void build_codex();
    void build_pause();
    void build_results();
    void build_failure();
    void build_settings();
    void build_bindings();
    void build_credits();
    void build_error();

    ContentDatabase mDb;
    std::filesystem::path mUserDir;
    Settings mSettings;
    Window mWindow;
    std::unique_ptr<InputSystem> mInput;
    std::unique_ptr<AudioSystem> mAudio;
    std::unique_ptr<GameSession> mSession;
    WorldRenderer mRenderer;
    Camera mCamera;
    Ui mUi;

    Screen mScreen = Screen::MainMenu;
    Screen mPreviousScreen = Screen::MainMenu;
    MenuScreen mMenu;
    int mSelected = 0;
    float mScroll = 0.0f;
    float mScreenIntro = 0.0f; ///< 0..1 transition-in for the active screen.
    std::string mErrorText;

    std::deque<Toast> mToasts;
    std::vector<TutorialStep> mTutorials;
    std::string mTutorialTitle;
    std::string mTutorialBody;
    float mTutorialLife = 0.0f;

    // Smoothed instrument readings so gauges settle rather than snap.
    float mFuelShown = 0.0f;
    float mHullShown = 0.0f;
    float mHeatShown = 0.0f;
    float mCargoShown = 0.0f;

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
    float mTime = 0.0f;
    u64 mFrameCount = 0;
};

} // namespace df
