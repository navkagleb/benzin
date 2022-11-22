#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/fence.hpp"

#include "spieler/core/common.hpp"

#include "spieler/graphics/device.hpp"

namespace spieler
{

    Fence::Fence(Device& device)
    {
        SPIELER_ASSERT(SUCCEEDED(device.GetDX12Device()->CreateFence(m_Value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_DX12Fence))));
    }

    void Fence::Increment()
    {
        ++m_Value;
    }

    void Fence::WaitForGPU() const
    {
        if (m_DX12Fence->GetCompletedValue() < m_Value)
        {
            // ::Handle is void*
            const ::HANDLE eventHandle{ CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS) };

            SPIELER_ASSERT(eventHandle);

            m_DX12Fence->SetEventOnCompletion(m_Value, eventHandle);

            ::WaitForSingleObject(eventHandle, INFINITE);
            ::CloseHandle(eventHandle);
        }
    }

} // namespace spieler
