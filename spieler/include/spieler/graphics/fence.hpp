#pragma once

namespace spieler
{

    class Device;
    class Context;

    class Fence
    {
    public:
        explicit Fence(Device& device);

    public:
        uint64_t GetValue() const { return m_Value; }

    public:
        ID3D12Fence* GetDX12Fence() const { return m_DX12Fence.Get(); }

        void Increment();
        void WaitForGPU() const;

    private:
        ComPtr<ID3D12Fence> m_DX12Fence;

        uint64_t m_Value{ 0 };
    };

} // namespace spieler
