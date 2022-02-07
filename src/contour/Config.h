/**
 * This file is part of the "libterminal" project
 *   Copyright (c) 2019-2020 Christian Parpart <christian@parpart.family>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <contour/Actions.h>
#include <contour/opengl/ShaderConfig.h>

#include <terminal/Color.h>
#include <terminal/ColorPalette.h>
#include <terminal/InputBinding.h>
#include <terminal/Process.h>
#include <terminal/Sequencer.h> // CursorDisplay

#include <terminal_renderer/Decorator.h>
#include <terminal_renderer/FontDescriptions.h>

#include <text_shaper/font.h>
#include <text_shaper/mock_font_locator.h>

#include <crispy/size.h>
#include <crispy/stdfs.h>

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <unordered_map>
#include <variant>

namespace contour::config
{

// {{{ Gamepad input binding

enum class GamepadButton
{
    ButtonA,
    ButtonB,
    ButtonX,
    ButtonY,
    L1,
    R1,
    L2,
    R2,
    ButtonSelect,
    ButtonStart,
    L3,
    R3,
    Up,
    Down,
    Left,
    Right,
    Center,
    Guide,
};

struct GamepadButtonState
{
    int deviceId;
    GamepadButton button;
    bool pressed;
};

constexpr bool operator==(GamepadButtonState lhs, GamepadButtonState rhs) noexcept
{
    return lhs.deviceId == rhs.deviceId && lhs.button == rhs.button && lhs.pressed == rhs.pressed;
}

constexpr bool operator!=(GamepadButtonState lhs, GamepadButtonState rhs) noexcept
{
    return !(lhs == rhs);
}

enum class GamepadAxis
{
    LeftX,  // X-axis of left stick
    LeftY,  // Y-axis of left stick
    RightX, // X-axis of right stick
    RightY, // Y-axis of right stick
    L2,
    R2,
};

struct GamepadAxisState
{
    int deviceId;
    GamepadAxis axis;
    double value;
};

constexpr bool operator==(GamepadAxisState left, GamepadAxisState right) noexcept
{
    // clang-format off
    return left.deviceId == right.deviceId
        && left.axis == right.axis
        && left.value == right.value;
    // clang-format on
}

constexpr bool operator!=(GamepadAxisState left, GamepadAxisState right) noexcept
{
    return !(left == right);
}

struct GamepadAxisRange
{
    int deviceId;
    GamepadAxis axis;
    double begin;
    double end;               // excluding
    int repeatsPerSecond = 0; // 0 means disabled.
};

constexpr bool operator==(GamepadAxisRange left, GamepadAxisRange right) noexcept
{
    // clang-format off
    return left.deviceId == right.deviceId
        && left.axis == right.axis
        && left.begin == right.begin
        && left.end == right.end;
    // clang-format on
}

constexpr bool operator==(GamepadAxisState const& state, GamepadAxisRange const& range) noexcept
{
    // clang-format off
    return state.deviceId == range.deviceId
        && state.axis == range.axis
        && range.begin <= state.value
        && state.value < range.end;
    // clang-format on
}
// }}}

enum class ScrollBarPosition
{
    Hidden,
    Left,
    Right
};

enum class Permission
{
    Deny,
    Allow,
    Ask
};

enum class SelectionAction
{
    Nothing,
    CopyToSelectionClipboard,
    CopyToClipboard,
};

using ActionList = std::vector<actions::Action>;
using KeyInputMapping = terminal::InputBinding<terminal::Key, ActionList>;
using CharInputMapping = terminal::InputBinding<char32_t, ActionList>;
using MouseInputMapping = terminal::InputBinding<terminal::MouseButton, ActionList>;
using GamepadAxisInputMapping = terminal::InputBinding<GamepadAxisRange, ActionList>;
using GamepadButtonInputMapping = terminal::InputBinding<GamepadButtonState, ActionList>;

struct InputMappings
{
    std::vector<KeyInputMapping> keyMappings;
    std::vector<CharInputMapping> charMappings;
    std::vector<MouseInputMapping> mouseMappings;
    std::vector<GamepadAxisInputMapping> gamepadAxisMappings;
    std::vector<GamepadButtonInputMapping> gamepadButtonMappings;
};

namespace helper
{
    inline bool testMatchMode(uint8_t _actualModeFlags,
                              terminal::MatchModes _expected,
                              terminal::MatchModes::Flag _testFlag)
    {
        using MatchModes = terminal::MatchModes;
        switch (_expected.status(_testFlag))
        {
        case MatchModes::Status::Enabled:
            if (!(_actualModeFlags & _testFlag))
                return false;
            break;
        case MatchModes::Status::Disabled:
            if ((_actualModeFlags & _testFlag))
                return false;
        case MatchModes::Status::Any: break;
        }
        return true;
    }

    inline bool testMatchMode(uint8_t _actualModeFlags, terminal::MatchModes _expected)
    {
        using Flag = terminal::MatchModes::Flag;
        return testMatchMode(_actualModeFlags, _expected, Flag::AlternateScreen)
               && testMatchMode(_actualModeFlags, _expected, Flag::AppCursor)
               && testMatchMode(_actualModeFlags, _expected, Flag::AppKeypad)
               && testMatchMode(_actualModeFlags, _expected, Flag::Select);
    }
} // namespace helper

template <typename Matcher, typename Input>
std::vector<actions::Action> const* apply(
    std::vector<terminal::InputBinding<Matcher, ActionList>> const& _mappings,
    Input _input,
    terminal::Modifier _modifier,
    uint8_t _actualModeFlags)
{
    for (terminal::InputBinding<Matcher, ActionList> const& mapping: _mappings)
    {
        if (mapping.modifier == _modifier && _input == mapping.input
            && helper::testMatchMode(_actualModeFlags, mapping.modes))
        {
            return &mapping.binding;
        }
    }
    return nullptr;
}

struct TerminalProfile
{
    terminal::Process::ExecInfo shell;
    bool maximized = false;
    bool fullscreen = false;
    double refreshRate = 0.0; // 0=auto
    terminal::LineOffset copyLastMarkRangeOffset = terminal::LineOffset(0);

    std::string wmClass;

    terminal::PageSize terminalSize = { terminal::LineCount(10), terminal::ColumnCount(40) };
    terminal::VTType terminalId = terminal::VTType::VT525;

    terminal::LineCount maxHistoryLineCount;
    terminal::LineCount historyScrollMultiplier;
    ScrollBarPosition scrollbarPosition = ScrollBarPosition::Right;
    bool hideScrollbarInAltScreen = true;

    bool autoScrollOnUpdate;

    terminal::renderer::FontDescriptions fonts;

    struct
    {
        Permission captureBuffer = Permission::Ask;
        Permission changeFont = Permission::Ask;
    } permissions;

    terminal::ColorPalette colors {};

    terminal::CursorShape cursorShape;
    terminal::CursorDisplay cursorDisplay;
    std::chrono::milliseconds cursorBlinkInterval;

    terminal::Opacity backgroundOpacity; // value between 0 (fully transparent) and 0xFF (fully visible).
    bool backgroundBlur;                 // On Windows 10, this will enable Acrylic Backdrop.

    struct
    {
        terminal::renderer::Decorator normal = terminal::renderer::Decorator::DottedUnderline;
        terminal::renderer::Decorator hover = terminal::renderer::Decorator::Underline;
    } hyperlinkDecoration;
};

using opengl::ShaderClass;
using opengl::ShaderConfig;

enum class RenderingBackend
{
    Default,
    Software,
    OpenGL,
};

// NB: All strings in here must be UTF8-encoded.
struct Config
{
    FileSystem::path backingFilePath;

    /// Qt platform plugin to be loaded.
    /// This is equivalent to QT_QPA_PLATFORM.
    std::string platformPlugin;

    RenderingBackend renderingBackend = RenderingBackend::Default;

    /// Enables/disables support for direct mapped texture atlas tiles (e.g. glyphs).
    bool textureAtlasDirectMapping = true;

    // Number of hashtable slots to map to the texture tiles.
    // Larger values may increase performance, but too large may also decrease.
    // This value is rounted up to a value equal to the power of two.
    //
    // Default: 4096
    crispy::StrongHashtableSize textureAtlasHashtableSlots = crispy::StrongHashtableSize { 4096 };

    /// Number of tiles that must fit at lest into the texture atlas,
    /// excluding US-ASCII glyphs, cursor shapes and decorations.
    ///
    /// Value must be at least as large as grid cells available in the current view.
    /// This value is automatically adjusted if too small.
    crispy::LRUCapacity textureAtlasTileCount = crispy::LRUCapacity { 4000 };

    // Configures the size of the PTY read buffer.
    // Changing this value may result in better or worse throughput performance.
    //
    // This value must be integer-devisable by 16.
    int ptyReadBufferSize = 16384;

    bool reflowOnResize = true;

    std::unordered_map<std::string, terminal::ColorPalette> colorschemes;
    std::unordered_map<std::string, TerminalProfile> profiles;
    std::string defaultProfileName;

    TerminalProfile* profile(std::string const& _name)
    {
        if (auto i = profiles.find(_name); i != profiles.end())
            return &i->second;
        return nullptr;
    }

    TerminalProfile const* profile(std::string const& _name) const
    {
        if (auto i = profiles.find(_name); i != profiles.end())
            return &i->second;
        return nullptr;
    }

    TerminalProfile& profile() noexcept { return *profile(defaultProfileName); }
    TerminalProfile const& profile() const noexcept { return *profile(defaultProfileName); }

    // selection
    std::string wordDelimiters;
    terminal::Modifier bypassMouseProtocolModifier = terminal::Modifier::Shift;
    SelectionAction onMouseSelection = SelectionAction::CopyToSelectionClipboard;
    terminal::Modifier mouseBlockSelectionModifier = terminal::Modifier::Control;

    // input mapping
    InputMappings inputMappings;

    static std::optional<ShaderConfig> loadShaderConfig(ShaderClass _shaderClass);

    ShaderConfig backgroundShader = opengl::defaultShaderConfig(ShaderClass::Background);
    ShaderConfig textShader = opengl::defaultShaderConfig(ShaderClass::Text);

    bool spawnNewProcess = false;

    bool sixelScrolling = true;
    bool sixelCursorConformance = true;
    terminal::ImageSize maxImageSize = {}; // default to runtime system screen size.
    int maxImageColorRegisters = 4096;

    std::set<std::string> experimentalFeatures;
};

FileSystem::path configHome(std::string const& _programName);

std::optional<std::string> readConfigFile(std::string const& _filename);

void loadConfigFromFile(Config& _config, FileSystem::path const& _fileName);
Config loadConfigFromFile(FileSystem::path const& _fileName);
Config loadConfig();

std::string createDefaultConfig();
std::error_code createDefaultConfig(FileSystem::path const& _path);
std::string defaultConfigFilePath();

} // namespace contour::config

namespace fmt // {{{
{
template <>
struct formatter<contour::config::Permission>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(contour::config::Permission const& _perm, FormatContext& ctx)
    {
        switch (_perm)
        {
        case contour::config::Permission::Allow: return format_to(ctx.out(), "allow");
        case contour::config::Permission::Deny: return format_to(ctx.out(), "deny");
        case contour::config::Permission::Ask: return format_to(ctx.out(), "ask");
        }
        return format_to(ctx.out(), "({})", unsigned(_perm));
    }
};

template <>
struct formatter<contour::config::GamepadAxis>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(contour::config::GamepadAxis value, FormatContext& ctx)
    {
        using contour::config::GamepadAxis;
        switch (value)
        {
        case GamepadAxis::LeftX: return format_to(ctx.out(), "LeftX");
        case GamepadAxis::LeftY: return format_to(ctx.out(), "LeftY");
        case GamepadAxis::RightX: return format_to(ctx.out(), "RightX");
        case GamepadAxis::RightY: return format_to(ctx.out(), "RightY");
        case GamepadAxis::L2: return format_to(ctx.out(), "L2");
        case GamepadAxis::R2: return format_to(ctx.out(), "R2");
        }
        return format_to(ctx.out(), "{}", (unsigned) value);
    }
};

template <>
struct formatter<contour::config::GamepadAxisState>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(contour::config::GamepadAxisState value, FormatContext& ctx)
    {
        return format_to(ctx.out(), "Device-Id {} {} {}", value.deviceId, value.axis, value.value);
    }
};

template <>
struct formatter<contour::config::GamepadAxisRange>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(contour::config::GamepadAxisRange value, FormatContext& ctx)
    {
        return format_to(
            ctx.out(), "Device-Id {} {} {}..{}", value.deviceId, value.axis, value.begin, value.end);
    }
};

template <>
struct formatter<contour::config::GamepadButton>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(contour::config::GamepadButton value, FormatContext& ctx)
    {
        using contour::config::GamepadButton;
        switch (value)
        {
        case GamepadButton::ButtonA: return format_to(ctx.out(), "ButtonA");
        case GamepadButton::ButtonB: return format_to(ctx.out(), "ButtonB");
        case GamepadButton::ButtonX: return format_to(ctx.out(), "ButtonX");
        case GamepadButton::ButtonY: return format_to(ctx.out(), "ButtonY");
        case GamepadButton::L1: return format_to(ctx.out(), "L1");
        case GamepadButton::R1: return format_to(ctx.out(), "R1");
        case GamepadButton::L2: return format_to(ctx.out(), "L2");
        case GamepadButton::R2: return format_to(ctx.out(), "R2");
        case GamepadButton::ButtonSelect: return format_to(ctx.out(), "ButtonSelect");
        case GamepadButton::ButtonStart: return format_to(ctx.out(), "ButtonStart");
        case GamepadButton::L3: return format_to(ctx.out(), "L3");
        case GamepadButton::R3: return format_to(ctx.out(), "R3");
        case GamepadButton::Up: return format_to(ctx.out(), "Up");
        case GamepadButton::Down: return format_to(ctx.out(), "Down");
        case GamepadButton::Left: return format_to(ctx.out(), "Left");
        case GamepadButton::Right: return format_to(ctx.out(), "Right");
        case GamepadButton::Center: return format_to(ctx.out(), "Center");
        case GamepadButton::Guide: return format_to(ctx.out(), "Guide");
        }
        return format_to(ctx.out(), "{}", (unsigned) value);
    }
};

template <>
struct formatter<contour::config::GamepadButtonState>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }
    using SelectionAction = contour::config::SelectionAction;
    template <typename FormatContext>
    auto format(contour::config::GamepadButtonState value, FormatContext& ctx)
    {
        return format_to(ctx.out(),
                         "Gamepad {} button {} {}",
                         value.deviceId,
                         value.button,
                         value.pressed ? "pressed" : "released");
    }
};

template <>
struct formatter<contour::config::SelectionAction>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }
    using SelectionAction = contour::config::SelectionAction;
    template <typename FormatContext>
    auto format(SelectionAction _value, FormatContext& ctx)
    {
        switch (_value)
        {
        case SelectionAction::CopyToClipboard: return format_to(ctx.out(), "CopyToClipboard");
        case SelectionAction::CopyToSelectionClipboard:
            return format_to(ctx.out(), "CopyToSelectionClipboard");
        case SelectionAction::Nothing: return format_to(ctx.out(), "Waiting");
        }
        return format_to(ctx.out(), "{}", static_cast<unsigned>(_value));
    }
};

template <>
struct formatter<contour::config::ScrollBarPosition>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }
    using ScrollBarPosition = contour::config::ScrollBarPosition;
    template <typename FormatContext>
    auto format(ScrollBarPosition _value, FormatContext& ctx)
    {
        switch (_value)
        {
        case ScrollBarPosition::Hidden: return format_to(ctx.out(), "Hidden");
        case ScrollBarPosition::Left: return format_to(ctx.out(), "Left");
        case ScrollBarPosition::Right: return format_to(ctx.out(), "Right");
        }
        return format_to(ctx.out(), "{}", static_cast<unsigned>(_value));
    }
};

} // namespace fmt
