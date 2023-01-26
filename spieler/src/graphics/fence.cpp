#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/fence.hpp"

#include "spieler/core/common.hpp"

#include "spieler/graphics/device.hpp"

namespace spieler
{

    Fence::Fence(Device& device)
    {
        SPIELER_D3D12_ASSERT(device.GetD3D12Device()->CreateFence(m_Value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_D3D12Fence)));

        m_WaitEvent = CreateEvent(nullptr, false, false, nullptr);
        SPIELER_ASSERT(m_WaitEvent);
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

} // namespace spieler
