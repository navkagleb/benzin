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
        ::CloseHandle(m_WaitEvent);

        SafeUnknownRelease(m_D3D12Fence);
    }

    void Fence::WaitForGPU(uint64_t value) const
    {
        m_D3D12Fence->SetEventOnCompletion(value, m_WaitEvent);
        ::WaitForSingleObject(m_WaitEvent, INFINITE);
    }

} // namespace benzin
