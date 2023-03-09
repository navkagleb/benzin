#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/fence.hpp"

#include "benzin/graphics/api/device.hpp"

namespace benzin
{

    static constexpr uint64_t g_DefaultFenceValue = 0;

    Fence::Fence(Device& device, std::string_view debugName)
    {
        BENZIN_D3D12_ASSERT(device.GetD3D12Device()->CreateFence(g_DefaultFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_D3D12Fence)));
        SetDebugName(debugName, true);

        m_WaitEvent = CreateEvent(nullptr, false, false, nullptr);
        BENZIN_ASSERT(m_WaitEvent);
    }

    Fence::~Fence()
    {
        ::CloseHandle(m_WaitEvent);

        SafeReleaseD3D12Object(m_D3D12Fence);
    }

    void Fence::WaitForGPU(uint64_t value) const
    {
        m_D3D12Fence->SetEventOnCompletion(value, m_WaitEvent);
        ::WaitForSingleObject(m_WaitEvent, INFINITE);
    }

} // namespace benzin
