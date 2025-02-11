#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/fence.hpp"

#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/hr_assert.hpp"

namespace benzin
{

    Fence::Fence(Device& device, const FenceCreation& creation)
    {
        BenzinHrEnsure(device.GetD3D12Device()->CreateFence(
            creation.InitialValue,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&m_D3D12Fence)
        ));
        SetDxObjectDebugName(m_D3D12Fence, creation.DebugName);

        m_WaitEvent = ::CreateEvent(nullptr, false, false, nullptr);
        BenzinEnsure(m_WaitEvent != INVALID_HANDLE_VALUE);
    }

    Fence::~Fence()
    {
        ::CloseHandle(m_WaitEvent);

        BenzinSafeDxObjectRelease(m_D3D12Fence);
    }

    uint64_t Fence::GetCompletedValue() const
    {
        return m_D3D12Fence->GetCompletedValue();
    }

    void Fence::StopCurrentThreadBeforeGpuFinish(uint64_t value) const
    {
        m_D3D12Fence->SetEventOnCompletion(value, m_WaitEvent);
        BenzinEnsure(::WaitForSingleObject(m_WaitEvent, INFINITE) == WAIT_OBJECT_0);
    }

} // namespace benzin
