#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/fence.hpp"

#include "benzin/graphics/api/device.hpp"

namespace benzin
{

    Fence::Fence(Device& device)
    {
        BENZIN_HR_ASSERT(device.GetD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_D3D12Fence)));

        m_WaitEvent = CreateEvent(nullptr, false, false, nullptr);
        BENZIN_ASSERT(m_WaitEvent);
    }

    Fence::~Fence()
    {
        ::CloseHandle(m_WaitEvent);

        dx::SafeRelease(m_D3D12Fence);
    }

    void Fence::WaitForGPU(uint64_t value) const
    {
        m_D3D12Fence->SetEventOnCompletion(value, m_WaitEvent);
        ::WaitForSingleObject(m_WaitEvent, INFINITE);
    }

} // namespace benzin
