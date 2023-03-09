namespace benzin
{

    template <typename... Args>
    void Logger::Log(Severity severity, std::string_view filepath, uint32_t line, std::string_view format, Args&&... args)
    {
        LogImpl(severity, filepath, line, fmt::format(format.data(), std::forward<Args>(args)...));
    }

} // namespace benzin
