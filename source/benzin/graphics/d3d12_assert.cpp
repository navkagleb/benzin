#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/d3d12_assert.hpp>

#include <benzin/graphics/d3d12_utils.hpp>

namespace benzin
{

    extern std::string_view DxgiErrorToString(HRESULT hr);

    static D3D12Asserter::DeviceRemovedCallback g_DeviceRemovedCallback;

    void D3D12Asserter::SetDeviceRemovedCallback(DeviceRemovedCallback&& callback)
    {
        g_DeviceRemovedCallback = std::move(callback);
    }

    HRESULT D3D12Asserter::ValidateHr(HRESULT hr)
    {
        if (g_DeviceRemovedCallback && (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET))
        {
            return g_DeviceRemovedCallback();
        }

        return hr;
    }

    std::string D3D12Asserter::GetHrMessage(HRESULT hr)
    {
        const _com_error comError{ hr };
        const std::string_view comErrorMessage = comError.ErrorMessage();

        return std::format("HRESULT: ({:#0x}) {}, ComErrorMessage: {}", (uint32_t)hr, DxgiErrorToString(hr), comErrorMessage);
    }

}
