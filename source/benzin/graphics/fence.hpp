#pragma once

namespace benzin
{

    class Device;

    struct FenceCreation
    {
        std::string_view DebugName;
        uint64_t InitialValue = g_MaxU64;
    };

    class Fence
    {
    public:
        Fence(Device& device, const FenceCreation& creation);
        ~Fence();

        BenzinDefineNonCopyable(Fence);
        BenzinDefineNonMoveable(Fence);

    public:
        auto* GetD3D12Fence() const { return m_D3D12Fence; }

        uint64_t GetCompletedValue() const;
        void StopCurrentThreadBeforeGpuFinish(uint64_t value) const;

    private:
        ID3D12Fence* m_D3D12Fence = nullptr;
        HANDLE m_WaitEvent = nullptr;
    };

}
