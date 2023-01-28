#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Device;
    class Context;

    class Fence
    {
    public:
        BENZIN_DEBUG_NAME_D3D12_OBJECT(m_D3D12Fence, "Fence")

    public:
        explicit Fence(Device& device);
        ~Fence();

    public:
        uint64_t GetValue() const { return m_Value; }

    public:
        ID3D12Fence* GetD3D12Fence() const { return m_D3D12Fence; }
        uint64_t GetCompletedValue() const { return m_D3D12Fence->GetCompletedValue(); }

        void Increment();
        void WaitForGPU() const;

    private:
        ID3D12Fence* m_D3D12Fence{ nullptr };
        HANDLE m_WaitEvent{ nullptr };

        uint64_t m_Value{ 0 };
    };

} // namespace benzin
