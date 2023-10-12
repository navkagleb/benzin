#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Device;

    class Fence
    {
    public:
        BENZIN_NON_COPYABLE_IMPL(Fence)
        BENZIN_NON_MOVEABLE_IMPL(Fence)

    public:
        explicit Fence(Device& device);
        ~Fence();

    public:
        ID3D12Fence* GetD3D12Fence() const { return m_D3D12Fence; }
        uint64_t GetCompletedValue() const { return m_D3D12Fence->GetCompletedValue(); }

        void WaitForGPU(uint64_t value) const;

    private:
        ID3D12Fence* m_D3D12Fence = nullptr;
        HANDLE m_WaitEvent = nullptr;
    };

} // namespace benzin
