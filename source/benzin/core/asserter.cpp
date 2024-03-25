#include "benzin/config/bootstrap.hpp"
#include "benzin/core/asserter.hpp"

#include "benzin/core/logger.hpp"
#include "benzin/graphics/d3d12_utils.hpp"

namespace benzin
{

    static inline void DebugBreak()
    {
        if (::IsDebuggerPresent())
        {
            ::DebugBreak();
        }
        else
        {
            *((volatile int*)1) = 2;
        }
    }

    static void FormatToBuffer(const std::source_location& sourceLocation, std::string_view conditionString, std::string_view message, std::string& outBuffer)
    {
        if (outBuffer.empty())
        {
            outBuffer.reserve(KbToBytes(1));
        }

        std::format_to(
            std::back_inserter(outBuffer),
            "\n"
            "-- Assertion failed !!!\n"
            "-- {}:{}\n"
            "-- Function: {}\n"
            "-- Condition: {}\n",
            sourceLocation.file_name(), sourceLocation.line(),
            sourceLocation.function_name(),
            conditionString
        );

        if (!message.empty())
        {
            std::format_to(std::back_inserter(outBuffer), "-- Message: {}\n", message);
        }
    }

    static void FormatToBuffer(HRESULT hr, std::string& outBuffer)
    {
        _com_error comError{ hr };
        const std::string_view comErrorMessage = comError.ErrorMessage();

        std::format_to(
            std::back_inserter(outBuffer),
            "-- HRESULT: ({:#0x}) {}\n"
            "-- ComErrorMessage: {}\n",
            (uint32_t)hr, DxgiErrorToString(hr),
            comErrorMessage
        );
    }

    static void LogAndBreak(std::string_view buffer)
    {
        BenzinError("{}", buffer);
        DebugBreak();
    }

    static Asserter::DeviceRemovedCallback g_DeviceRemovedCallback;

    //

    void Asserter::SetDeviceRemovedCallback(DeviceRemovedCallback&& callback)
    {
        g_DeviceRemovedCallback = std::move(callback);
    }

    void Asserter::AssertImpl(bool isPassed, std::string_view conditionString, const std::source_location& sourceLocation, std::string_view message)
    {
        if (isPassed)
        {
            return;
        }

        std::string buffer;
        FormatToBuffer(sourceLocation, conditionString, message, buffer);

        LogAndBreak(buffer);
    }

    void Asserter::AssertImpl(HRESULT hr, std::string_view conditionString, const std::source_location& sourceLocation, std::string_view message)
    {
        if (SUCCEEDED(hr))
        {
            return;
        }

        if (g_DeviceRemovedCallback && (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET))
        {
            BenzinError("DeviceRemoved is triggered!");
            hr = g_DeviceRemovedCallback();
        }

        std::string buffer;
        FormatToBuffer(sourceLocation, conditionString, message, buffer);
        FormatToBuffer(hr, buffer);
        
        LogAndBreak(buffer);
    }

} // namespace benzin
