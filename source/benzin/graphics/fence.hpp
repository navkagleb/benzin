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
        virtual ~Fence();

    public:
        ID3D12Fence* GetD3D12Fence() const { return m_D3D12Fence; }

    protected:
        ID3D12Fence* m_D3D12Fence = nullptr;

        HANDLE m_WaitEvent = nullptr;
    };

    class StalledFence : public Fence
    {
    public:
        explicit StalledFence(Device& device, std::string_view debugName);

    public:
        uint64_t GetCompletedValue() const;

        void StallCurrentThreadUntilGPUCompletion(uint64_t value) const;
    };

} // namespace benzin
