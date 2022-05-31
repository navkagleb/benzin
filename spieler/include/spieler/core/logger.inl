namespace spieler
{

    template <typename... Args>
    std::string Logger::Format(const std::string& format, Args&&... args)
    {
        return std::format(format, std::forward<Args>(args)...);
    }

    template <std::integral T>
    Logger& Logger::operator<< (T integral)
    {
        m_OutputStream << integral;
        return *this;
    }

    template <std::floating_point T>
    Logger& Logger::operator<< (T floatingPoint)
    {
        m_OutputStream << floatingPoint;
        return *this;
    }

} // namespace spieler
