namespace spieler
{

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
