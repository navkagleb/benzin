#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/fence.hpp"

#include "benzin/core/common.hpp"

#include "benzin/graphics/device.hpp"

namespace benzin
{

    Fence::Fence(Device& device)
    {
        BENZIN_D3D12_ASSERT(device.GetD3D12Device()->CreateFence(m_Value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_D3D12Fence)));

        m_WaitEvent = CreateEvent(nullptr, false, false, nullptr);
        BENZIN_ASSERT(m_WaitEvent);
    }

    Fence::~Fence()
    {
        ::CloseHandle(m_WaitEvent);

        SafeReleaseD3D12Object(m_D3D12Fence);
    }

    void Fence::Increment()
    {
        ++m_Value;
    }

    void Fence::WaitForGPU() const
    {
        if (m_D3D12Fence->GetCompletedValue() < m_Value)
        {
            m_D3D12Fence->SetEventOnCompletion(m_Value, m_WaitEvent);
            ::WaitForSingleObject(m_WaitEvent, INFINITE);
        }
    }

} // namespace benzin
