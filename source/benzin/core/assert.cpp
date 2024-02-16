#include "benzin/config/bootstrap.hpp"
#include "benzin/core/assert.hpp"

namespace benzin
{

    namespace
    {

        inline void DebugBreak()
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
                "--------------------------------------------------\n"
                "Assert failed !!!\n"
                "{}:{}\n"
                "Function: {}\n",
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

            std::format_to(std::back_inserter(buffer), "--------------------------------------------------");

            BenzinError("\n{}", buffer);
        }

    } // anonymous namespace

    void Asserter::AssertImpl(bool isPassed, const std::source_location& sourceLocation, std::string_view message) const
    {
        if (isPassed)
        {
            return;
        }

        AssertLog(sourceLocation, message);
        DebugBreak();
    }

    void Asserter::AssertImpl(HRESULT hr, const std::source_location& sourceLocation, std::string_view message) const
    {
        if (SUCCEEDED(hr))
        {
            return;
        }

        _com_error comError{ hr };
        AssertLog(sourceLocation, message, comError.ErrorMessage());

#if 0
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
#endif

        DebugBreak();
    }

} // namespace benzin
