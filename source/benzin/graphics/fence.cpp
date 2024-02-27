#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/fence.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin
{

    // Fence

    Fence::Fence(Device& device, std::string_view debugName)
    {
        BenzinEnsure(device.GetD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_D3D12Fence)));
        SetD3D12ObjectDebugName(m_D3D12Fence, debugName);

        m_WaitEvent = ::CreateEvent(nullptr, false, false, nullptr);
        BenzinEnsure(m_WaitEvent != INVALID_HANDLE_VALUE);
    }

    Fence::~Fence()
    {
        ::CloseHandle(m_WaitEvent);

        SafeUnknownRelease(m_D3D12Fence);
    }

    // StalledFence

    StalledFence::StalledFence(Device& device, std::string_view debugName)
        : Fence{ device, debugName }
    {}

    uint64_t StalledFence::GetCompletedValue() const
    {
        BenzinAssert(m_D3D12Fence);

        return m_D3D12Fence->GetCompletedValue();
    }

    void StalledFence::StallCurrentThreadUntilGPUCompletion(uint64_t value) const
    {
        m_D3D12Fence->SetEventOnCompletion(value, m_WaitEvent);
        BenzinEnsure(::WaitForSingleObject(m_WaitEvent, INFINITE) == WAIT_OBJECT_0);
    }

} // namespace benzin
