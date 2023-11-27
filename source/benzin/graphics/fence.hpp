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
        explicit Fence(Device& device);
        ~Fence();

    public:
        ID3D12Fence* GetD3D12Fence() const { return m_D3D12Fence; }

        uint64_t GetCompletedValue() const;

        void StallCurrentThreadUntilGPUCompletion(uint64_t value) const;
        void SubsribeForDeviceRemoving();

    private:
        ID3D12Fence* m_D3D12Fence = nullptr;

        HANDLE m_WaitEvent = nullptr;
        HANDLE m_DeviceRemovedWaitEvent = nullptr;
    };

} // namespace benzin
