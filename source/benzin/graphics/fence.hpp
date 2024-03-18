#pragma once

namespace benzin
{

    class Device;

    class Fence
    {
    public:
        BenzinDefineNonCopyable(Fence);
        BenzinDefineNonMoveable(Fence);

    public:
        explicit Fence(Device& device, std::string_view debugName);
        ~Fence();

    public:
        auto* GetD3D12Fence() const { return m_D3D12Fence; }

        uint64_t GetCompletedValue() const;
        void StopCurrentThreadBeforeGpuFinish(uint64_t value) const;

    private:
        ID3D12Fence* m_D3D12Fence = nullptr;
        HANDLE m_WaitEvent = nullptr;
    };

} // namespace benzin
