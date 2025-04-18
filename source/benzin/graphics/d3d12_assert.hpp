#pragma once

namespace benzin
{

    namespace D3D12Asserter
    {
        using DeviceRemovedCallback = std::function<HRESULT()>;

        void SetDeviceRemovedCallback(DeviceRemovedCallback&& callback);

        HRESULT ValidateHr(HRESULT hr);
        std::string GetHrMessage(HRESULT hr);
    };

}

#define BenzinD3D12Call(hrExpression, ...) \
    do \
    { \
        const HRESULT _hr = benzin::D3D12Asserter::ValidateHr(hrExpression); \
        if (FAILED(_hr)) \
        { \
            const auto messages = std::to_array<std::string>({ benzin::ArgsToFormatString(__VA_ARGS__), benzin::D3D12Asserter::GetHrMessage(_hr) }); \
            benzin::Assert(#hrExpression, std::source_location::current(), messages); \
            BenzinDebugBreak(); \
        } \
    } while (0)
