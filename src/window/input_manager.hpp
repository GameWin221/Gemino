#ifndef GEMINO_INPUT_MANAGER_HPP
#define GEMINO_INPUT_MANAGER_HPP

#include <window/window.hpp>
#include <array>

enum struct Key : i32 {
    FIRST_KEY = 32,
    Space = 32,
    Apostrophe = 39,
    Comma = 44,
    Minus = 45,
    Period = 46,
    Slash = 47,
    Key0 = 48,
    Key1 = 49,
    Key2 = 50,
    Key3 = 51,
    Key4 = 52,
    Key5 = 53,
    Key6 = 54,
    Key7 = 55,
    Key8 = 56,
    Key9 = 57,
    Semicolon = 59,
    Equal = 61,
    A = 65,
    B = 66,
    C = 67,
    D = 68,
    E = 69,
    F = 70,
    G = 71,
    H = 72,
    I = 73,
    J = 74,
    K = 75,
    L = 76,
    M = 77,
    N = 78,
    O = 79,
    P = 80,
    Q = 81,
    R = 82,
    S = 83,
    T = 84,
    U = 85,
    V = 86,
    W = 87,
    X = 88,
    Y = 89,
    Z = 90,
    LeftBracket = 91,
    Backslash = 92,
    RightBracket = 93,
    GraveAccent = 96,
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace  = 259,
    Insert = 260,
    Delete = 261,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,
    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,
    F1 = 290,
    F2 = 291,
    F3 = 292,
    F4 = 293,
    F5 = 294,
    F6 = 295,
    F7 = 296,
    F8 = 297,
    F9 = 298,
    F10 = 299,
    F11 = 300,
    F12 = 301,
    Keypad0 = 320,
    Keypad1 = 321,
    Keypad2 = 322,
    Keypad3 = 323,
    Keypad4 = 324,
    Keypad5 = 325,
    Keypad6 = 326,
    Keypad7 = 327,
    Keypad8 = 328,
    Keypad9 = 329,
    KeypadDecimal = 330,
    KeypadDivide = 331,
    KeypadMultiply = 332,
    KeypadSubtract = 333,
    KeypadAdd = 334,
    KeypadEnter = 335,
    KeypadEqual = 336,
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
    Menu = 348,
    LAST_KEY = 348
};

enum struct Button : i32 {
    FIRST_BUTTON = 0,
    Button1 = 0,
    Button2 = 1,
    Button3 = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7,
    LAST_BUTTON = 7,

    Left = Button1,
    Right = Button2,
    Middle = Button3,
};

enum struct CursorMode : u32 {
    Free = 0x00034001,
    Hidden = 0x00034002,
    Locked = 0x00034003
};

enum struct InputState : u32 {
    Pressed = 0U,
    Down,
    Released
};

class InputManager {
public:
    explicit InputManager(Window& polled_window);
    ~InputManager();

    InputManager& operator=(const InputManager& other) = delete;
    InputManager& operator=(InputManager&& other) noexcept = delete;

    void poll_input();

    void set_cursor_mode(CursorMode mode);
    CursorMode get_cursor_mode() const { return cursor_mode; }

    bool get_key(Key key, InputState state) const;
    bool get_button(Button button, InputState state) const;

    Key get_last_key() const { return last_key; };
    Button get_last_button() const { return last_button; };

    glm::vec2 get_scroll() const { return scroll_delta; };

    glm::vec2 get_mouse_position() const { return current_mouse_pos; };
    glm::vec2 get_mouse_velocity() const { return mouse_delta; };

private:
    struct GLFWwindow* polled_window_handle{};

    CursorMode cursor_mode = CursorMode::Free;

    Key last_key{};
    Button last_button{};

    glm::vec2 scroll_delta{};

    glm::vec2 current_mouse_pos{};
    glm::vec2 last_mouse_pos{};
    glm::vec2 mouse_delta{};

    std::array<bool, static_cast<usize>(Key::LAST_KEY) - static_cast<usize>(Key::FIRST_KEY) + 1> current_key_presses{};
    std::array<bool, static_cast<usize>(Key::LAST_KEY) - static_cast<usize>(Key::FIRST_KEY) + 1> last_key_presses{};

    std::array<bool, static_cast<usize>(Button::LAST_BUTTON) - static_cast<usize>(Button::FIRST_BUTTON) + 1> current_button_presses{};
    std::array<bool, static_cast<usize>(Button::LAST_BUTTON) - static_cast<usize>(Button::FIRST_BUTTON) + 1> last_button_presses{};

    static void scroll_callback(struct GLFWwindow* window, f64 x_offset, f64 y_offset);
    static void key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods);
    static void mouse_button_callback(GLFWwindow* window, i32 button, i32 action, i32 mods);
};

#endif
