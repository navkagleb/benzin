#pragma once

namespace benzin
{

    namespace HrAsserter
    {
        using DeviceRemovedCallback = std::function<HRESULT()>;

        void SetDeviceRemovedCallback(DeviceRemovedCallback&& callback);

        HRESULT ValidateHr(HRESULT hr);
        std::string GetHrMessage(HRESULT hr);
    };

}

#define BenzinHrEnsure(hrExpression, ...) \
    do \
    { \
        const HRESULT _hr = HrAsserter::ValidateHr(hrExpression); \
        if (FAILED(_hr)) \
        { \
            const std::string message = benzin::ArgsToFormatString(__VA_ARGS__); \
            const std::string hrMessage = benzin::HrAsserter::GetHrMessage(_hr); \
            benzin::Assert(#hrExpression, std::source_location::current(), message, hrMessage); \
            BenzinDebugBreak(); \
        } \
    } while (0)
