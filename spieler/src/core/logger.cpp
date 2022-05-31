#include "spieler/config/bootstrap.hpp"

#include "spieler/core/logger.hpp"

namespace spieler
{

    namespace _internal
    {

        uint8_t ConvertToWinConsoleTextAttribute(uint8_t textColor, uint8_t backgroundColor)
        {
            return textColor + backgroundColor * 16;
        }

    } // namespace _internal

    Logger& Logger::GetInstance()
    {
        static Logger instance;
        return instance;
    }

    Logger& Logger::Reset(Logger& logger)
    {
        return WhiteOnBlack(logger);
    }

    Logger& Logger::Flush(Logger& logger)
    {
        logger.m_OutputStream.flush();
        return logger;
    }

    Logger& Logger::EndLine(Logger& logger)
    {
        logger.m_OutputStream << std::endl;
        return logger;
    }

    Logger::Logger()
        : m_ConsoleHandle{ ::GetStdHandle(STD_OUTPUT_HANDLE) }
        , m_OutputStream{ std::cout }
        , m_COutputStream{ stdout }
    {}

    Logger& Logger::SetAttribute(Color textColor, Color backgroundColor)
    {
        ::SetConsoleTextAttribute(m_ConsoleHandle, _internal::ConvertToWinConsoleTextAttribute(static_cast<uint8_t>(textColor), static_cast<uint8_t>(backgroundColor)));
        return *this;
    }

    Logger& Logger::operator<< (const std::string& string)
    {
        m_OutputStream << string;
        return *this;
    }

    Logger& Logger::operator<< (const Manipulator& manipulator)
    {
        return manipulator(*this);
    }

} // namespace spieler