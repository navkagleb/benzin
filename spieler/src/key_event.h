#pragma once

#include "event.h"

namespace Spieler
{

    enum KeyCode : std::int32_t
    {
        KeyCode_Unknown         = -1,

        KeyCode_Backspace       = VK_BACK,
        KeyCode_Tab             = VK_TAB,
        KeyCode_Enter           = VK_RETURN,
        KeyCode_Shift           = VK_SHIFT,
        KeyCode_Control         = VK_CONTROL,
        KeyCode_Alt             = VK_MENU,
        KeyCode_CapLock         = VK_CAPITAL,
        KeyCode_Escape          = VK_ESCAPE,
        KeyCode_Space           = VK_SPACE,
        KeyCode_PageUp          = VK_PRIOR,
        KeyCode_PageDown        = VK_NEXT,
        KeyCode_End             = VK_END,
        KeyCode_Home            = VK_HOME,
        KeyCode_LeftArrow       = VK_LEFT,
        KeyCode_UpArrow         = VK_UP,
        KeyCode_RightArrow      = VK_RIGHT,
        KeyCode_DownArrow       = VK_DOWN,
        KeyCode_Select          = VK_SELECT,
        KeyCode_Print           = VK_PRINT,
        KeyCode_Execute         = VK_EXECUTE,
        KeyCode_Snapshot        = VK_SNAPSHOT,
        KeyCode_Insert          = VK_INSERT,
        KeyCode_Delete          = VK_DELETE,
        KeyCode_Help            = VK_HELP,

        KeyCode_D0              = 0x30,
        KeyCode_D1              = 0x31,
        KeyCode_D2              = 0x32,
        KeyCode_D3              = 0x33,
        KeyCode_D4              = 0x34,
        KeyCode_D5              = 0x35,
        KeyCode_D6              = 0x36,
        KeyCode_D7              = 0x37,
        KeyCode_D8              = 0x38,
        KeyCode_D9              = 0x39,

        KeyCode_A               = 0x41,
        KeyCode_B               = 0x42,
        KeyCode_C               = 0x43,
        KeyCode_D               = 0x44,
        KeyCode_E               = 0x45,
        KeyCode_F               = 0x46,
        KeyCode_G               = 0x47,
        KeyCode_H               = 0x48,
        KeyCode_I               = 0x49,
        KeyCode_J               = 0x4A,
        KeyCode_K               = 0x4B,
        KeyCode_L               = 0x4C,
        KeyCode_M               = 0x4D,
        KeyCode_N               = 0x4E,
        KeyCode_O               = 0x4F,
        KeyCode_P               = 0x50,
        KeyCode_Q               = 0x51,
        KeyCode_R               = 0x52,
        KeyCode_S               = 0x53,
        KeyCode_T               = 0x54,
        KeyCode_U               = 0x55,
        KeyCode_V               = 0x56,
        KeyCode_W               = 0x57,
        KeyCode_X               = 0x58,
        KeyCode_Y               = 0x59,
        KeyCode_Z               = 0x5A,

        KeyCode_LeftWin         = VK_LWIN,
        KeyCode_RightWin        = VK_RWIN,
        KeyCode_Apps            = VK_APPS,
        KeyCode_Sleep           = VK_SLEEP,
        KeyCode_Numpad0         = VK_NUMPAD0,
        KeyCode_Numpad1         = VK_NUMPAD1,
        KeyCode_Numpad2         = VK_NUMPAD2,
        KeyCode_Numpad3         = VK_NUMPAD3,
        KeyCode_Numpad4         = VK_NUMPAD4,
        KeyCode_Numpad5         = VK_NUMPAD5,
        KeyCode_Numpad6         = VK_NUMPAD6,
        KeyCode_Numpad7         = VK_NUMPAD7,
        KeyCode_Numpad8         = VK_NUMPAD8,
        KeyCode_Numpad9         = VK_NUMPAD9,
        KeyCode_Multiply        = VK_MULTIPLY,
        KeyCode_Add             = VK_ADD,
        KeyCode_Separator       = VK_SEPARATOR,
        KeyCode_Subtract        = VK_SUBTRACT,
        KeyCode_Decimal         = VK_DECIMAL,
        KeyCode_Divide          = VK_DIVIDE,

        KeyCode_F1              = VK_F1,
        KeyCode_F2              = VK_F2,
        KeyCode_F3              = VK_F3,
        KeyCode_F4              = VK_F4,
        KeyCode_F5              = VK_F5,
        KeyCode_F6              = VK_F6,
        KeyCode_F7              = VK_F7,
        KeyCode_F8              = VK_F1,
        KeyCode_F9              = VK_F9,
        KeyCode_F10             = VK_F10,
        KeyCode_F11             = VK_F11,
        KeyCode_F12             = VK_F12,
        KeyCode_F13             = VK_F13,
        KeyCode_F14             = VK_F14,
        KeyCode_F15             = VK_F15,
        KeyCode_F16             = VK_F16,
        KeyCode_F17             = VK_F17,
        KeyCode_F18             = VK_F18,
        KeyCode_F19             = VK_F19,
        KeyCode_F20             = VK_F20,
        KeyCode_F21             = VK_F21,
        KeyCode_F22             = VK_F22,
        KeyCode_F23             = VK_F23,
        KeyCode_F24             = VK_F24,

        KeyCode_NumLock         = VK_NUMLOCK,
        KeyCode_Scroll          = VK_SCROLL,

        KeyCode_LeftShift       = VK_LSHIFT,
        KeyCode_RightShift      = VK_RSHIFT,
        KeyCode_LeftControl     = VK_LCONTROL,
        KeyCode_RightControl    = VK_RCONTROL,
        KeyCode_LeftAlt         = VK_LMENU,
        KeyCode_RightAlt        = VK_RMENU,
        
        KeyCode_VolumeMute      = VK_VOLUME_MUTE,
        KeyCode_VolumeDown      = VK_VOLUME_DOWN,
        KeyCode_VolumeUp        = VK_VOLUME_UP,

        KeyCode_MediaNextTrack  = VK_MEDIA_NEXT_TRACK,
        KeyCode_MediaPrevTrack  = VK_MEDIA_PREV_TRACK,
        KeyCode_MediaStop       = VK_MEDIA_STOP,
        KeyCode_MediaPlayPause  = VK_MEDIA_PLAY_PAUSE
    };

    class KeyEvent : public Event
    {
        EVENT_CLASS_CATEGORY(EventCategory_Input | EventCategory_Keyboard)

    protected:
        explicit KeyEvent(KeyCode keyCode)
            : m_KeyCode(keyCode)
        {}

    public:
        KeyCode GetKeyCode() const { return m_KeyCode; }

    protected:
        KeyCode m_KeyCode;
    };

    class KeyPressedEvent : public KeyEvent
    {
        EVENT_CLASS_TYPE(KeyPressedEvent)

    public:
        KeyPressedEvent(KeyCode keyCode, bool isRepeated)
            : KeyEvent(keyCode)
            , m_IsRepeated(isRepeated)
        {}
       
    public:
        bool IsRepeated() const { return m_IsRepeated; }

        std::string ToString() const override
        {
            return GetName() + ": " + static_cast<char>(m_KeyCode) + "(" + std::to_string(m_KeyCode) + ", " + std::to_string(m_IsRepeated) + ")";
        }

    private:
        bool m_IsRepeated;
    };

    class KeyReleasedEvent : public KeyEvent
    {
        EVENT_CLASS_TYPE(KeyReleasedEvent)

    public:
        explicit KeyReleasedEvent(KeyCode keyCode)
            : KeyEvent(keyCode)
        {}
        
    public:
        std::string ToString() const override
        {
            return GetName() + ": " + static_cast<char>(m_KeyCode) + "(" + std::to_string(m_KeyCode) + ")";
        }
    };

    class KeyTypedEvent : public Event
    {
        EVENT_CLASS_TYPE(KeyTypedEvent);
        EVENT_CLASS_CATEGORY(EventCategory_Input | EventCategory_Keyboard)

    public:
        explicit KeyTypedEvent(char character)
            : m_Character(character)
        {}
       
        std::string ToString() const override
        {
            return GetName() + ": " + m_Character;
        }

    private:
        char m_Character;
    };
} // namespace Spieler