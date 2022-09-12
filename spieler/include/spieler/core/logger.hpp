#pragma once

#include <third_party/fmt/format.h>

namespace spieler
{

#define _SPIELER_LOGGER_TEXT_COLOR(textColor) static Logger& textColor(Logger& logger) { return logger.SetAttribute(Color::textColor, Color::Black); }
#define _SPIELER_LOGGER_BACKGROUND_COLOR(backgroundColor) static Logger& On##backgroundColor(Logger& logger) { return logger.SetAttribute(Color::White, Color::backgroundColor); }
#define _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(TextColor, BackgroundColor) static Logger& TextColor##On##BackgroundColor(Logger& logger) { return logger.SetAttribute(Color::TextColor, Color::BackgroundColor); }

#define _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(Background)                       \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(Black, Background)                            \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(Blue, Background)                             \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(Green, Background)                            \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(Aqua, Background)                             \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(Red, Background)                              \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(Purple, Background)                           \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(Yellow, Background)                           \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(White, Background)                            \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(Grey, Background)                             \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(LightBlue, Background)                        \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(LightGreen, Background)                       \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(LightAqua, Background)                        \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(LightRed, Background)                         \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(LightPurple, Background)                      \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(LightYellow, Background)                      \
    _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR(BrightWhite, Background)

    class Logger
    {
    private:
        enum class Color : uint8_t
        {
            Black = 0,
            Blue,
            Green,
            Aqua,
            Red,
            Purple,
            Yellow,
            White,
            Grey,
            LightBlue,
            LightGreen,
            LightAqua,
            LightRed,
            LightPurple,
            LightYellow,
            BrightWhite
        };

        using Manipulator = std::function<Logger&(Logger&)>;

    public:
        static Logger& GetInstance();

        static Logger& Reset(Logger& logger);
        static Logger& Flush(Logger& logger);
        static Logger& EndLine(Logger& logger);

        _SPIELER_LOGGER_TEXT_COLOR(Black)
        _SPIELER_LOGGER_TEXT_COLOR(Blue)
        _SPIELER_LOGGER_TEXT_COLOR(Green)
        _SPIELER_LOGGER_TEXT_COLOR(Aqua)
        _SPIELER_LOGGER_TEXT_COLOR(Red)
        _SPIELER_LOGGER_TEXT_COLOR(Purple)
        _SPIELER_LOGGER_TEXT_COLOR(Yellow)
        _SPIELER_LOGGER_TEXT_COLOR(White)
        _SPIELER_LOGGER_TEXT_COLOR(Grey)
        _SPIELER_LOGGER_TEXT_COLOR(LightBlue)
        _SPIELER_LOGGER_TEXT_COLOR(LightGreen)
        _SPIELER_LOGGER_TEXT_COLOR(LightAqua)
        _SPIELER_LOGGER_TEXT_COLOR(LightRed)
        _SPIELER_LOGGER_TEXT_COLOR(LightPurple)
        _SPIELER_LOGGER_TEXT_COLOR(LightYellow)
        _SPIELER_LOGGER_TEXT_COLOR(BrightWhite)

        _SPIELER_LOGGER_BACKGROUND_COLOR(Black)
        _SPIELER_LOGGER_BACKGROUND_COLOR(Blue)
        _SPIELER_LOGGER_BACKGROUND_COLOR(Green)
        _SPIELER_LOGGER_BACKGROUND_COLOR(Aqua)
        _SPIELER_LOGGER_BACKGROUND_COLOR(Red)
        _SPIELER_LOGGER_BACKGROUND_COLOR(Purple)
        _SPIELER_LOGGER_BACKGROUND_COLOR(Yellow)
        _SPIELER_LOGGER_BACKGROUND_COLOR(White)
        _SPIELER_LOGGER_BACKGROUND_COLOR(Grey)
        _SPIELER_LOGGER_BACKGROUND_COLOR(LightBlue)
        _SPIELER_LOGGER_BACKGROUND_COLOR(LightGreen)
        _SPIELER_LOGGER_BACKGROUND_COLOR(LightAqua)
        _SPIELER_LOGGER_BACKGROUND_COLOR(LightRed)
        _SPIELER_LOGGER_BACKGROUND_COLOR(LightPurple)
        _SPIELER_LOGGER_BACKGROUND_COLOR(LightYellow)
        _SPIELER_LOGGER_BACKGROUND_COLOR(BrightWhite)

        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(Black)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(Blue)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(Green)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(Aqua)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(Red)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(Purple)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(Yellow)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(White)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(Grey)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(LightBlue)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(LightGreen)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(LightAqua)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(LightRed)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(LightPurple)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(LightYellow)
        _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION(BrightWhite)

    private:
        Logger();

    private:
        Logger& SetAttribute(Color textColor, Color backgroundColor);

    public:
        Logger& operator<< (const std::string& value);

        template <std::integral T>
        Logger& operator<< (T integral);

        template <std::floating_point T>
        Logger& operator<< (T floatingPoint);

        Logger& operator<< (const Manipulator& manipulator);

    private:
        ::HANDLE m_ConsoleHandle{ INVALID_HANDLE_VALUE };
        std::ostream& m_OutputStream;
        FILE* m_COutputStream;
    };

#undef _SPIELER_LOGGER_TEXT_COLOR
#undef _SPIELER_LOGGER_BACKGROUND_COLOR
#undef _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR
#undef _SPIELER_LOGGER_TEXT_BACKGROUND_COLOR_SECTION

#define _SPIELER_LOGGER_LOG(type, Color, formatString, ...)                                                                                                     \
{                                                                                                                                                               \
    using namespace ::spieler;                                                                                                                                  \
    Logger::GetInstance() << Logger::Color << "[" << type << "] " << Logger::Reset << fmt::format(FMT_STRING(formatString), __VA_ARGS__) << Logger::EndLine;    \
}

#define SPIELER_INFO(format, ...) _SPIELER_LOGGER_LOG("INFO", Green, format, __VA_ARGS__)
#define SPIELER_WARNING(format, ...) _SPIELER_LOGGER_LOG("WARNING", Yellow, format, __VA_ARGS__)
#define SPIELER_CRITICAL(format, ...) _SPIELER_LOGGER_LOG("CRITICAL", Red, format, __VA_ARGS__)

} // namespace spieler

#include "logger.inl"