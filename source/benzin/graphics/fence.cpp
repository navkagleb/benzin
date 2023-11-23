#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/fence.hpp"

#include "benzin/graphics/device.hpp"

namespace benzin
{

    Fence::Fence(Device& device)
    {
        BenzinAssert(device.GetD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_D3D12Fence)));

        m_WaitEvent = CreateEvent(nullptr, false, false, nullptr);
        BenzinAssert(m_WaitEvent);
    }

    Fence::~Fence()
    {
        if (m_DeviceRemovedWaitEvent)
        {
            ::UnregisterWait(m_DeviceRemovedWaitEvent);
        }

        ::CloseHandle(m_WaitEvent);

        SafeUnknownRelease(m_D3D12Fence);
    }

    void Fence::WaitForGPU(uint64_t value) const
    {
        m_D3D12Fence->SetEventOnCompletion(value, m_WaitEvent);
        BenzinAssert(::WaitForSingleObject(m_WaitEvent, INFINITE) == WAIT_OBJECT_0);
    }

    void Fence::WaitForDeviceRemoving()
    {
        m_D3D12Fence->SetEventOnCompletion(std::numeric_limits<uint64_t>::max(), m_WaitEvent);

        ComPtr<ID3D12Device> d3d12Device;
        BenzinAssert(m_D3D12Fence->GetDevice(IID_PPV_ARGS(&d3d12Device)));

        BenzinAssert(::RegisterWaitForSingleObject(&m_DeviceRemovedWaitEvent, m_WaitEvent, OnD3D12DeviceRemoved, d3d12Device.Get(), INFINITE, 0) != 0);
    }

} // namespace benzin
