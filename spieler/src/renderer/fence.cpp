#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/fence.hpp"

#include "spieler/core/assert.hpp"
#include "spieler/core/common.hpp"

#include "spieler/renderer/device.hpp"
#include "spieler/renderer/context.hpp"

namespace spieler::renderer
{

    Fence::Fence(Device& device)
    {
        SPIELER_ASSERT(Init(device));
    }

    void Fence::Increment()
    {
        ++m_Value;
    }

    bool Fence::Signal(Context& context) const
    {
        SPIELER_RETURN_IF_FAILED(context.GetNativeCommandQueue()->Signal(m_Fence.Get(), m_Value));

        return true;
    }

    bool Fence::WaitForGPU() const
    {
        if (m_Fence->GetCompletedValue() < m_Value)
        {
            // ::Handle is void*
            const ::HANDLE eventHandle{ CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS) };

            if (!eventHandle)
            {
                return false;
            }

            m_Fence->SetEventOnCompletion(m_Value, eventHandle);

            ::WaitForSingleObject(eventHandle, INFINITE);
            ::CloseHandle(eventHandle);
        }

        return true;
    }

    bool Fence::Init(Device& device)
    {
        SPIELER_RETURN_IF_FAILED(device.GetNativeDevice()->CreateFence(m_Value, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &m_Fence));

        return true;
    }

} // namespace spieler::renderer