#include "presentation/Ui.hpp"

#include "core/Hash.hpp"
#include "presentation/FocusModel.hpp"

#include <algorithm>
#include <cmath>
#include <format>

namespace df {
namespace {

float ease(float current, float target, float rate, float dt, bool instant) {
    if (instant) {
        return target;
    }
    return approach(current, target, rate, dt);
}

std::string format_number(float v, int decimals) {
    switch (decimals) {
        case 0:
            return std::format("{:.0f}", v);
        case 2:
            return std::format("{:.2f}", v);
        default:
            return std::format("{:.1f}", v);
    }
}

} // namespace

void Ui::begin_frame(Draw& draw, const PointerState& pointer, float dt, bool reduceMotion) {
    mDraw = &draw;
    mPointer = pointer;
    mDt = dt;
    mReduceMotion = reduceMotion;
    mFrameTime += dt;
    // Drop animation state for widgets that stopped being drawn, so the store
    // cannot grow without bound across screens.
    if (mAnim.size() > 512) {
        for (auto it = mAnim.begin(); it != mAnim.end();) {
            it = (mFrameTime - it->second.touched > 5.0f) ? mAnim.erase(it) : std::next(it);
        }
    }
}

float Ui::anim(u64 key, bool hovered, bool focused, bool pressed) {
    WidgetAnim& a = mAnim[key];
    a.touched = mFrameTime;
    a.hover = ease(a.hover, hovered ? 1.0f : 0.0f, theme::kHoverRate, mDt, mReduceMotion);
    a.focus = ease(a.focus, focused ? 1.0f : 0.0f, theme::kFocusRate, mDt, mReduceMotion);
    a.press = ease(a.press, pressed ? 1.0f : 0.0f, 30.0f, mDt, mReduceMotion);
    return std::max(a.hover * 0.55f, a.focus) - a.press * 0.25f;
}

bool Ui::hit(const Rect& r) const {
    return mPointer.present && mPointer.position.x >= r.x && mPointer.position.x < r.x + r.w &&
           mPointer.position.y >= r.y && mPointer.position.y < r.y + r.h;
}

// ---------------------------------------------------------------------------
// Chrome
// ---------------------------------------------------------------------------

void Ui::status_bar(const Rect& r, std::string_view context, i64 credits, i64 insight, int proficiency,
                    std::string_view right) {
    Draw& d = *mDraw;
    d.vgradient(r, theme::kHousingHigh, theme::kHousing, 6);
    d.hline(r.x, r.y + r.h - std::max(1.0f, d.scale()), r.w, theme::kAmberDeep, std::max(1.0f, d.scale()));
    d.hline(r.x, r.y, r.w, theme::kBevelLight.with_alpha(120), std::max(1.0f, d.scale()));

    const float pad = d.px(theme::kSpace5);
    const float mid = r.y + (r.h - d.line_height(text_style::kHeading)) * 0.5f;

    // Wordmark: a small drilled-strata glyph plus the title.
    const float markSize = d.px(11.0f);
    const float markX = r.x + pad;
    const float markY = r.y + (r.h - markSize) * 0.5f;
    for (int i = 0; i < 4; ++i) {
        const float h = markSize / 5.0f;
        d.rect(markX, markY + static_cast<float>(i) * (h + h * 0.28f),
               markSize - static_cast<float>(i) * d.px(1.4f), h,
               mix(theme::kAmber, theme::kAmberDeep, static_cast<float>(i) / 3.0f));
    }
    float x = markX + markSize + d.px(theme::kSpace3);
    x += d.text("DEEPFORGE", x, mid, theme::kText, text_style::kHeading);
    x += d.px(theme::kSpace3);
    d.vline(x, r.y + r.h * 0.28f, r.h * 0.44f, theme::kRule);
    x += d.px(theme::kSpace3);
    d.text(context, x, mid, theme::kAmber, text_style::kHeading);

    // Resource cluster, right aligned.
    float rx = r.x + r.w - pad;
    if (!right.empty()) {
        rx -= d.text(right, rx, mid, theme::kTextMuted, text_style::kBody, TextAlign::Right);
        rx -= d.px(theme::kSpace5);
    }

    struct Res {
        Icon icon;
        std::string value;
        Color color;
    };

    const Res resources[] = {
        {Icon::Skill, std::format("{}", proficiency), theme::kGood},
        {Icon::Insight, std::format("{}", insight), theme::kCyan},
        {Icon::Credit, std::format("{}", credits), theme::kAmber},
    };
    const float iconSize = d.px(11.0f);
    for (const auto& res : resources) {
        rx -= d.text(res.value, rx, mid, theme::kText, text_style::kNumeric, TextAlign::Right);
        rx -= d.px(theme::kSpace2);
        rx -= iconSize;
        d.icon(res.icon, rx, r.y + (r.h - iconSize) * 0.5f, iconSize, res.color);
        rx -= d.px(theme::kSpace5);
    }
}

void Ui::screen_header(const Rect& r, std::string_view title, std::string_view subtitle, Icon icon) {
    Draw& d = *mDraw;
    float x = r.x;
    if (icon != Icon::None) {
        const float s = d.px(18.0f);
        d.icon(icon, x, r.y + d.px(1.0f), s, theme::kAmber);
        x += s + d.px(theme::kSpace4);
    }
    d.text(title, x, r.y, theme::kText, text_style::kTitle);
    const float subY = r.y + d.line_height(text_style::kTitle) + d.px(theme::kSpace2);
    if (!subtitle.empty()) {
        d.text(subtitle, x, subY, theme::kTextMuted, text_style::kBody);
    }
    // Rule with an amber lead-in, which reads as an instrument label underline.
    const float ruleY = r.y + r.h - d.px(theme::kSpace2);
    d.hline(r.x, ruleY, d.px(28.0f), theme::kAmber, std::max(1.0f, d.scale()));
    d.hline(r.x + d.px(30.0f), ruleY, r.w - d.px(30.0f), theme::kRuleSoft, std::max(1.0f, d.scale()));
}

void Ui::hint_bar(const Rect& r, std::string_view hint) {
    Draw& d = *mDraw;
    d.hline(r.x, r.y, r.w, theme::kRuleSoft, std::max(1.0f, d.scale()));
    d.text(hint, r.x + r.w * 0.5f, r.y + (r.h - d.line_height(text_style::kCaption)) * 0.5f,
           theme::kTextFaint, text_style::kCaption, TextAlign::Center);
}

// ---------------------------------------------------------------------------
// List rows
// ---------------------------------------------------------------------------

void Ui::list_row(const Rect& r, const MenuItem& item, bool hovered, float t) {
    Draw& d = *mDraw;
    const float unit = std::max(1.0f, std::round(d.scale()));
    const bool dim = !item.enabled;

    // Ground: focused rows get a lit well plus an amber leading edge.
    if (t > 0.01f) {
        d.rect(r, mix(Color{0, 0, 0, 0}, Color{44, 54, 70, 235}, t));
        d.rect(r.x, r.y, d.px(2.5f), r.h, mix(theme::kAmberDeep, theme::kAmber, t));
        d.hline(r.x, r.y, r.w, theme::kBevelLight.with_alpha_f(0.35f * t), unit);
        d.hline(r.x, r.y + r.h - unit, r.w, theme::kBevelDark.with_alpha_f(0.5f * t), unit);
    } else {
        d.hline(r.x, r.y + r.h - unit, r.w, theme::kRuleSoft.with_alpha(120), unit);
    }
    (void)hovered;

    const float pad = d.px(theme::kSpace4);
    // The focused row slides a couple of pixels toward the reader.
    const float shift = d.px(3.0f) * t;
    float x = r.x + pad + shift;

    const float iconSize = d.px(13.0f);
    const float iconY = r.y + (r.h - iconSize) * 0.5f;
    if (item.icon != Icon::None) {
        Color ic = dim ? theme::kTextDisabled : (item.marked ? theme::kGood : item.accent);
        if (t > 0.01f && !dim) {
            ic = mix(ic, theme::kAmberBright, t * 0.5f);
        }
        d.icon(item.icon, x, iconY, iconSize, ic);
        x += iconSize + d.px(theme::kSpace3);
    } else if (item.marked || item.locked) {
        d.icon(item.locked ? Icon::Lock : Icon::Check, x, iconY, iconSize,
               item.locked ? theme::kTextDisabled : theme::kGood);
        x += iconSize + d.px(theme::kSpace3);
    }

    // Right-aligned value column first, so the label knows how much room is left.
    float rightEdge = r.x + r.w - pad;
    if (item.slider >= 0.0f) {
        const Rect track = slider_track(r);
        const float f = clamp(item.slider, 0.0f, 1.0f);
        d.rect(track, theme::kWellDeep);
        d.bevel(track, true, unit);
        d.rect(track.x, track.y, std::round(track.w * f), track.h,
               t > 0.01f ? theme::kAmber : theme::kAmberDeep);
        // A knob, so the control reads as draggable rather than as a bar chart.
        const float knobW = d.px(4.0f);
        const float knobX = std::round(track.x + track.w * f - knobW * 0.5f);
        d.rect(knobX, track.y - d.px(3.0f), knobW, track.h + d.px(6.0f),
               t > 0.01f ? theme::kAmberBright : theme::kTextMuted);
        d.text(item.value, r.x + r.w - pad, r.y + (r.h - d.line_height(text_style::kNumeric)) * 0.5f,
               dim ? theme::kTextDisabled : theme::kText, text_style::kNumeric, TextAlign::Right);
        rightEdge = track.x - d.px(theme::kSpace4);
    } else if (!item.value.empty()) {
        // A dimmed row keeps its value hue so an unaffordable price is legible
        // in the list without focusing it; dimming still carries the state too.
        const Color vc = dim ? mix(item.valueColor, theme::kHousing, 0.42f) : item.valueColor;
        const float w = d.measure(item.value, text_style::kNumeric);
        d.text(item.value, rightEdge, r.y + (r.h - d.line_height(text_style::kNumeric)) * 0.5f, vc,
               text_style::kNumeric, TextAlign::Right);
        rightEdge -= w + d.px(theme::kSpace4);
    }
    if (!item.status.empty()) {
        const TextStyle chipStyle = text_style::kCaption;
        const float w = d.measure(item.status, chipStyle) + d.px(theme::kSpace3) * 2.0f;
        const float h = d.line_height(chipStyle) + d.px(theme::kSpace2);
        const Rect chipRect{rightEdge - w, r.y + (r.h - h) * 0.5f, w, h};
        d.chip(chipRect, item.status, item.statusColor, item.statusColor.with_alpha(34), chipStyle);
        rightEdge -= w + d.px(theme::kSpace3);
    }

    const float labelWidth = std::max(d.px(40.0f), rightEdge - x - d.px(theme::kSpace3));
    const bool twoLine = !item.detail.empty() && r.h >= d.line_height(text_style::kBody) * 2.0f + d.px(2.0f);
    Color lc = dim ? theme::kTextDisabled : theme::kText;
    if (t > 0.01f && !dim) {
        lc = mix(lc, Color{255, 255, 255}, t * 0.35f);
    }
    if (twoLine) {
        const float ly = r.y + r.h * 0.5f - d.line_height(text_style::kBody) - d.px(1.0f);
        d.text(d.elide(item.label, labelWidth, text_style::kHeading), x, ly, lc, text_style::kHeading);
        d.text(d.elide(item.detail, labelWidth, text_style::kBody), x,
               ly + d.line_height(text_style::kBody) + d.px(2.0f),
               dim ? theme::kTextDisabled : theme::kTextMuted, text_style::kBody);
    } else {
        d.text(d.elide(item.label, labelWidth, text_style::kBody), x,
               r.y + (r.h - d.line_height(text_style::kBody)) * 0.5f, lc, text_style::kBody);
    }
}

MenuResult Ui::menu_screen(const Rect& area, const MenuScreen& screen, int& selected, float& scroll,
                           bool confirmPressed) {
    Draw& d = *mDraw;
    const float unit = std::max(1.0f, std::round(d.scale()));

    // A screen that draws its own title lockup (the main menu) passes neither a
    // title nor a subtitle; it must not be charged for an empty header block.
    const bool hasHeader = !screen.title.empty() || !screen.subtitle.empty();
    const float headerH = hasHeader ? d.line_height(text_style::kTitle) + d.line_height(text_style::kBody) +
                                          d.px(theme::kSpace5)
                                    : 0.0f;
    const Rect header{area.x, area.y, area.w, headerH};
    if (hasHeader) {
        screen_header(header, screen.title, screen.subtitle, screen.titleIcon);
    }

    const float hintH = d.line_height(text_style::kCaption) + d.px(theme::kSpace5);
    const Rect hint{area.x, area.y + area.h - hintH, area.w, hintH};
    hint_bar(hint, screen.hint);

    const float bodyTop = header.y + header.h + (hasHeader ? d.px(theme::kSpace3) : 0.0f);
    Rect body{area.x, bodyTop, area.w, hint.y - bodyTop - d.px(theme::kSpace3)};

    const bool useInspector = screen.inspector && body.w > d.px(560.0f);
    Rect listRect = body;
    if (useInspector) {
        const float inspectorW = std::min(body.w * 0.40f, d.px(320.0f));
        listRect.w = body.w - inspectorW - d.px(theme::kSpace4);
    }
    const Rect view{listRect.x + unit * 2.0f, listRect.y + unit * 2.0f, listRect.w - unit * 4.0f,
                    listRect.h - unit * 4.0f};

    // One focus model: pointer-over-or-focus-index, commit on click or on
    // confirm. The whole decision pass lives in FocusModel and the painter
    // draws what it returns, so interaction and visuals cannot drift apart.
    const focus::Decisions decisions =
        focus::decide(screen, mPointer, selected, scroll, confirmPressed,
                      focus::Geometry{view, d.px(theme::kRowHeight), d.scale()});

    if (useInspector) {
        const Rect inspectorRect{body.x + listRect.w + d.px(theme::kSpace4), body.y,
                                 body.w - listRect.w - d.px(theme::kSpace4), body.h};
        d.panel(inspectorRect, Surface::Raised);
        const Rect inner{inspectorRect.x + d.px(theme::kSpace4), inspectorRect.y + d.px(theme::kSpace4),
                         inspectorRect.w - d.px(theme::kSpace4) * 2.0f,
                         inspectorRect.h - d.px(theme::kSpace4) * 2.0f};
        // The inspector answers for the row the focus model landed on this
        // frame; without a pointer the decision leaves `selected` where the
        // last frame left it, exactly as before.
        if (selected >= 0 && selected < static_cast<int>(screen.items.size())) {
            inspector(inner, screen.items[static_cast<usize>(selected)]);
        }
    }

    d.panel(listRect, Surface::Sunken);

    for (const auto& row : decisions.rows) {
        const auto& item = screen.items[static_cast<usize>(row.index)];
        const u64 key = hash_combine(fnv1a64(screen.title), static_cast<u64>(row.index) + 1);
        const float t = anim(key, row.hovered, row.focused, row.pressed);
        list_row(row.rect, item, row.hovered, clamp(t, 0.0f, 1.0f));
    }

    // Empty state. The builder sets this only when there is genuinely nothing to
    // list, so an empty screen explains itself instead of reading as a blank panel.
    if (!screen.emptyText.empty()) {
        const float w = view.w * 0.7f;
        const float x = view.x + (view.w - w) * 0.5f;
        const float top = view.y + view.h * 0.34f;
        d.icon(Icon::Codex, std::round(view.x + view.w * 0.5f - d.px(9.0f)), top - d.px(30.0f), d.px(18.0f),
               theme::kTextDisabled);
        wrap_text(screen.emptyText, Rect{x, top, w, 0.0f}, theme::kTextFaint, text_style::kBody);
    }

    // Scrollbar, only when it means something.
    if (decisions.max_scroll > 0.5f) {
        const float contentH = view.h + decisions.max_scroll; // from FocusModel's own arithmetic
        const float trackW = d.px(3.0f);
        const float trackX = listRect.x + listRect.w - trackW - unit * 2.0f;
        d.rect(trackX, view.y, trackW, view.h, theme::kWellDeep);
        const float thumbH = std::max(d.px(14.0f), view.h * (view.h / contentH));
        const float thumbY = view.y + (view.h - thumbH) * (scroll / decisions.max_scroll);
        d.rect(trackX, thumbY, trackW, thumbH, theme::kAmberDeep);
        d.rect(trackX, thumbY, trackW, std::max(1.0f, thumbH * 0.25f), theme::kAmber);
    }
    return decisions.result;
}

// ---------------------------------------------------------------------------
// Inspector
// ---------------------------------------------------------------------------

float Ui::wrap_text(std::string_view text, const Rect& box, Color c, const TextStyle& style) {
    Draw& d = *mDraw;
    float y = box.y;
    std::string line;
    std::string word;
    auto flush = [&]() {
        if (!line.empty()) {
            d.text(line, box.x, y, c, style);
            y += d.line_height(style) + d.px(2.0f);
            line.clear();
        }
    };
    for (usize i = 0; i <= text.size(); ++i) {
        const char ch = i < text.size() ? text[i] : ' ';
        if (ch != ' ' && ch != '\n') {
            word.push_back(ch);
            continue;
        }
        if (!word.empty()) {
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (d.measure(candidate, style) > box.w) {
                flush();
                line = word;
            } else {
                line = candidate;
            }
            word.clear();
        }
        if (ch == '\n') {
            flush();
        }
    }
    flush();
    return y;
}

void Ui::inspector(const Rect& r, const MenuItem& item) {
    Draw& d = *mDraw;
    float y = r.y;

    const float iconSize = d.px(20.0f);
    if (item.icon != Icon::None) {
        const Rect badge{r.x, y, iconSize + d.px(theme::kSpace4), iconSize + d.px(theme::kSpace4)};
        d.panel(badge, Surface::Sunken, false);
        d.icon(item.icon, badge.x + d.px(theme::kSpace2), badge.y + d.px(theme::kSpace2), iconSize,
               item.enabled ? item.accent : theme::kTextDisabled);
        d.text(d.elide(item.label, r.w - badge.w - d.px(theme::kSpace3), text_style::kHeading),
               badge.x + badge.w + d.px(theme::kSpace3), y + d.px(theme::kSpace2), theme::kText,
               text_style::kHeading);
        if (!item.status.empty()) {
            d.text(item.status, badge.x + badge.w + d.px(theme::kSpace3),
                   y + d.px(theme::kSpace2) + d.line_height(text_style::kHeading) + d.px(2.0f),
                   item.statusColor, text_style::kCaption);
        }
        y += badge.h + d.px(theme::kSpace4);
    } else {
        d.text(d.elide(item.label, r.w, text_style::kHeading), r.x, y, theme::kText, text_style::kHeading);
        y += d.line_height(text_style::kHeading) + d.px(theme::kSpace4);
    }

    d.hline(r.x, y, r.w, theme::kRuleSoft, std::max(1.0f, d.scale()));
    y += d.px(theme::kSpace4);

    // Body copy, wrapped against the measured width.
    if (!item.body.empty()) {
        y = wrap_text(item.body, Rect{r.x, y, r.w, 0.0f}, theme::kTextMuted, text_style::kBody);
        y += d.px(theme::kSpace3);
    }

    // Before/after bars: the clearest way to answer "what does this change?".
    for (const auto& delta : item.deltas) {
        const float rowH = d.line_height(text_style::kCaption) + d.px(9.0f);
        d.text(delta.label, r.x, y, theme::kTextFaint, text_style::kCaption);
        const bool better = delta.lowerIsBetter ? delta.after < delta.before : delta.after > delta.before;
        const bool same = std::fabs(delta.after - delta.before) < 0.0005f;
        const Color c = same ? theme::kTextMuted : (better ? theme::kGood : theme::kDanger);
        const std::string right = same ? format_number(delta.after, delta.decimals)
                                       : std::format("{} > {}", format_number(delta.before, delta.decimals),
                                                     format_number(delta.after, delta.decimals));
        d.text(right, r.x + r.w, y, c, text_style::kCaptionNum, TextAlign::Right);
        y += d.line_height(text_style::kCaption) + d.px(2.0f);

        // Paired bar: current in muted, the delta stacked on in the verdict colour.
        const float barH = d.px(3.0f);
        const float maxV = std::max({delta.before, delta.after, 0.0001f});
        const float b = clamp(delta.before / maxV, 0.0f, 1.0f);
        const float a = clamp(delta.after / maxV, 0.0f, 1.0f);
        d.rect(r.x, y, r.w, barH, theme::kWellDeep);
        d.rect(r.x, y, r.w * std::min(a, b), barH, theme::kTextFaint);
        if (a > b) {
            d.rect(r.x + r.w * b, y, r.w * (a - b), barH, c);
        } else if (b > a) {
            d.rect(r.x + r.w * a, y, r.w * (b - a), barH, c.with_alpha(120));
        }
        y += rowH - d.line_height(text_style::kCaption) + d.px(theme::kSpace2);
    }

    // Key/value facts. A short value sits on the label's line, right-aligned; a
    // long one wraps underneath it rather than being cut off mid-word.
    for (const auto& [key, value] : item.facts) {
        const float keyW = d.measure(key, text_style::kCaption);
        const float room = r.w - keyW - d.px(theme::kSpace4);
        d.text(key, r.x, y + d.px(2.0f), theme::kTextFaint, text_style::kCaption);
        if (d.measure(value, text_style::kBody) <= room) {
            d.text(value, r.x + r.w, y, theme::kText, text_style::kBody, TextAlign::Right);
            y += d.line_height(text_style::kBody) + d.px(theme::kSpace2);
        } else {
            y += d.line_height(text_style::kCaption) + d.px(2.0f);
            y = wrap_text(value, Rect{r.x + d.px(theme::kSpace3), y, r.w - d.px(theme::kSpace3), 0.0f},
                          theme::kText, text_style::kBody);
            y += d.px(theme::kSpace2);
        }
    }

    if (item.progress >= 0.0f) {
        y += d.px(theme::kSpace2);
        d.progress(Rect{r.x, y, r.w, d.px(5.0f)}, item.progress, theme::kCyan, theme::kWellDeep);
        y += d.px(theme::kSpace5);
    }

    // Blocking reason last, called out rather than buried.
    if (!item.note.empty()) {
        const float h = d.line_height(text_style::kCaption) + d.px(theme::kSpace4);
        const Rect box{r.x, r.y + r.h - h, r.w, h};
        d.rect(box, theme::kDangerDeep.with_alpha(110));
        d.rect(box.x, box.y, d.px(2.0f), box.h, theme::kWarn);
        const float ic = d.px(9.0f);
        d.icon(Icon::Warning, box.x + d.px(theme::kSpace3), box.y + (box.h - ic) * 0.5f, ic, theme::kWarn);
        d.text(d.elide(item.note, box.w - d.px(theme::kSpace3) * 2.0f - ic, text_style::kCaption),
               box.x + d.px(theme::kSpace3) + ic + d.px(theme::kSpace2),
               box.y + (box.h - d.line_height(text_style::kCaption)) * 0.5f, theme::kWarn,
               text_style::kCaption);
    }
}

// ---------------------------------------------------------------------------
// Instruments
// ---------------------------------------------------------------------------

void Ui::meter(const Rect& r, Icon icon, std::string_view label, float value, float max, Color fill,
               float danger) {
    Draw& d = *mDraw;
    const float fraction = max > 0.0f ? clamp(value / max, 0.0f, 1.0f) : 0.0f;
    const float iconSize = d.px(10.0f);
    const bool critical = fraction < 0.18f && (icon == Icon::Fuel || icon == Icon::Hull);
    const bool hot = danger >= 0.0f && fraction >= danger;
    const Color accent = (critical || hot) ? theme::kDanger : fill;

    d.icon(icon, r.x, r.y + (r.h - iconSize) * 0.5f, iconSize, accent);
    const float labelX = r.x + iconSize + d.px(theme::kSpace2);
    d.text(label, labelX, r.y, theme::kTextFaint, text_style::kCaption);

    const float readout = d.measure("0000/0000", text_style::kCaptionNum);
    const float barX = labelX;
    const float barY = r.y + d.line_height(text_style::kCaption) + d.px(1.0f);
    const float barW = r.w - (barX - r.x) - readout - d.px(theme::kSpace3);
    d.gauge(Rect{barX, barY, barW, r.h - (barY - r.y)}, fraction, accent, theme::kWellDeep, 16, danger);

    // Icon plus label plus number: never colour alone (Section 12.2).
    d.text(std::format("{:.0f}/{:.0f}", value, max), r.x + r.w, barY,
           critical || hot ? theme::kDanger : theme::kText, text_style::kCaptionNum, TextAlign::Right);
}

void Ui::depth_tape(const Rect& r, i64 depth, i64 maxDepth, const std::vector<std::pair<i64, Color>>& bands,
                    std::string_view biome) {
    Draw& d = *mDraw;
    d.panel(r, Surface::Sunken, false);
    const float unit = std::max(1.0f, std::round(d.scale()));
    const Rect inner{r.x + unit * 2.0f, r.y + unit * 2.0f, r.w - unit * 4.0f, r.h - unit * 4.0f};

    const float span = static_cast<float>(std::max<i64>(1, maxDepth));
    for (usize i = 0; i < bands.size(); ++i) {
        const float top = static_cast<float>(bands[i].first) / span;
        const float bottom = (i + 1 < bands.size()) ? static_cast<float>(bands[i + 1].first) / span : 1.0f;
        const Rect band{inner.x, inner.y + inner.h * top, inner.w, inner.h * (bottom - top)};
        d.rect(band, bands[i].second.with_alpha(70));
        d.hline(band.x, band.y, band.w, bands[i].second.with_alpha(150), unit);
    }

    // The rig's position, with a bright caret and the depth beside it.
    const float t = clamp(static_cast<float>(depth) / span, 0.0f, 1.0f);
    const float y = inner.y + inner.h * t;
    d.rect(inner.x, y - unit, inner.w, unit * 2.0f, theme::kAmber);
    d.rect(inner.x - unit * 2.0f, y - unit * 2.0f, unit * 2.0f, unit * 4.0f, theme::kAmberBright);
    d.text(std::format("{}m", depth), r.x + r.w * 0.5f, y + unit * 3.0f, theme::kAmberBright,
           text_style::kCaptionNum, TextAlign::Center);
    d.text(biome, r.x + r.w * 0.5f, r.y - d.line_height(text_style::kCaption) - d.px(theme::kSpace2),
           theme::kCyan, text_style::kCaption, TextAlign::Center);
}

Rect Ui::slider_track(const Rect& row) const {
    // The track geometry lives in the focus model so the painter and the drag
    // decision can never disagree about where the knob is.
    return focus::slider_track(row, mDraw->scale());
}

} // namespace df
