#include "benzin/config/bootstrap.hpp"
#include "benzin/core/logger.hpp"

namespace benzin
{

    namespace
    {

        const auto g_StartTimePoint = std::chrono::system_clock::now().time_since_epoch();

        std::string GetTimePointFormat()
        {
            using namespace std::chrono;

            const auto logTimePoint = system_clock::now().time_since_epoch();
            const auto passTime = duration_cast<milliseconds>(logTimePoint - g_StartTimePoint);

            const uint64_t h = duration_cast<hours>(passTime).count();
            const uint64_t m = duration_cast<minutes>(passTime).count() - h * 60;
            const uint64_t s = duration_cast<seconds>(passTime).count() - h * 60 * 60 - m * 60;
            const uint64_t ms = duration_cast<milliseconds>(passTime).count() - h * 60 * 60 * 1000 - m * 60 * 1000 - s * 1000;

            return std::format("{}:{}:{:0>2}.{:0>3}", h, m, s, ms);
        }

        std::string GetFileNameFormat(const std::source_location& sourceLocation)
        {
            const std::string_view filePath = sourceLocation.file_name();

            return std::format("{}:{}", filePath.substr(filePath.find_last_of("\\") + 1), sourceLocation.line());
        }

        std::string GetOutput(LogSeverity severity, const std::source_location& sourceLocation, std::string_view message)
        {
            const auto time = GetTimePointFormat();
            const auto fileName = GetFileNameFormat(sourceLocation);
            const auto threadId = std::this_thread::get_id();

            return std::format("[{}][{}][{}][{}]: {}\n", time, threadId, magic_enum::enum_name(severity), fileName, message);
        }

    } // anonymous namespace

    void Logger::LogImpl(LogSeverity severity, const std::source_location& sourceLocation, std::string_view message)
    {
        const auto output = GetOutput(severity, sourceLocation, message);

        std::print("{}", output);
        OutputDebugStringA(output.c_str());
    }

} // namespace benzin
