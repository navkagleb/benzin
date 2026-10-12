#pragma once

namespace benzin
{

    namespace D3D12Debug
    {
        using ValidateReturnCodeCallback = std::function<HRESULT(HRESULT hr)>;
        using DeviceRemovedCallback = std::function<void(HRESULT code)>;

        void SetValidateReturnCodeCallback(ValidateReturnCodeCallback&& callback);
        void SetDeviceRemovedCallback(DeviceRemovedCallback&& callback);

        HRESULT ValidateReturnCode(HRESULT code);
        void TriggerRemoveDevice(HRESULT code);

        std::string GetMessageFromReturnCode(HRESULT code);
        std::string_view DxgiErrorToString(HRESULT code);

        void EnableD3D12DebugLayer();
        void EnableD3D12DebugMessages(ID3D12Device* d3d12Device);
        void EnableDred();
        void ReportLiveD3D12Objects(ID3D12Device* d3d12Device);

        std::string ProcessDredMessages(ID3D12Device* d3d12Device);
    };

}

#define BenzinD3D12Call(hrExpression, ...) \
    do \
    { \
        HRESULT code = hrExpression; \
        code = benzin::D3D12Debug::ValidateReturnCode(code); \
        if (SUCCEEDED(code)) \
            break; \
        \
        benzin::D3D12Debug::TriggerRemoveDevice(code); \
        \
        const auto messages = std::to_array<std::string>( \
        { \
            benzin::D3D12Debug::GetMessageFromReturnCode(code), \
            benzin::ArgsToFormatString(__VA_ARGS__), \
        }); \
        \
        if (benzin::Ensure(#hrExpression, messages)) \
        { \
            __debugbreak(); \
        } \
    } while (0)
