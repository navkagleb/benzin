#include "benzin/config/bootstrap.hpp"
#include "benzin/core/assert.hpp"

namespace benzin
{

    namespace
    {

        void DebugBreak()
        {
            if (::IsDebuggerPresent())
            {
                ::DebugBreak();
            }
        }

        void AssertLog(const std::source_location& sourceLocation, std::string_view message, std::string_view comErrorMessage = "")
        {
            std::string buffer;
            buffer.reserve(512);

            std::format_to(
                std::back_inserter(buffer),
                "{}\n"
                "Assert failed !!!\n"
                "{}:{}\n"
                "Function: {}\n",
                Logger::s_LineSeparator,
                sourceLocation.file_name(), sourceLocation.line(),
                sourceLocation.function_name()
            );

            if (!message.empty())
            {
                std::format_to(std::back_inserter(buffer), "Message: {}\n", message);
            }

            if (!comErrorMessage.empty())
            {
                std::format_to(std::back_inserter(buffer), "ComError message: {}\n", comErrorMessage);
            }

            std::format_to(std::back_inserter(buffer), "{}", Logger::s_LineSeparator);

            BenzinError("\n{}", buffer);
        }

    } // anonymous namespace

    void AssertFormat(const std::source_location& sourceLocation, std::string_view message)
    {
        AssertLog(sourceLocation, message);
        DebugBreak();
    }

    void AssertFormat(HRESULT hr, const std::source_location& sourceLocation, std::string_view message)
    {
        _com_error comError{ hr };
        AssertLog(sourceLocation, message, comError.ErrorMessage());
        
        switch (hr)
        {
            case DXGI_ERROR_DEVICE_HUNG:
            case DXGI_ERROR_DEVICE_REMOVED:
            case DXGI_ERROR_DEVICE_RESET:
            case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
            case DXGI_ERROR_INVALID_CALL:
            {
                // DebugBreak in 'OnD3D12DeviceRemoved'
                return;
            }
        }

        DebugBreak();
    }

} // namespace benzin
