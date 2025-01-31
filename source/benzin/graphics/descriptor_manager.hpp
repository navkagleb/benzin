#pragma once

namespace benzin
{

    class Device;
    class DescriptorHeap;

    enum class DescriptorType : uint8_t
    {
        Rtv,
        Dsv,
        Cbv,
        Srv,
        Uav,
        Sampler, // For future use
    };

    class Descriptor
    {
    public:
        friend class DescriptorManager;

        Descriptor() = default;

        Descriptor(DescriptorType type, uint32_t gpuHeapIndex, uint64_t cpuHandle, uint64_t gpuHandle = 0)
            : m_Type{ type }
            , m_GpuHeapIndex{ gpuHeapIndex }
            , m_CpuHandle{ cpuHandle }
            , m_GpuHandle{ gpuHandle }
        {}
        
        auto GetType() const { return m_Type; }
        auto GetGpuHeapIndex() const { return m_GpuHeapIndex; }
        auto GetCpuHandle() const { return m_CpuHandle; }
        auto GetGpuHandle() const { return m_GpuHandle; }

        bool IsCpuValid() const { return m_CpuHandle != 0; }
        bool IsGpuValid() const { return IsCpuValid() && m_GpuHandle != 0; }

    private:
        DescriptorType m_Type = g_InvalidEnum<DescriptorType>;
        uint32_t m_GpuHeapIndex = 0;
        uint64_t m_CpuHandle = 0;
        uint64_t m_GpuHandle = 0;
    };

    class DescriptorManager
    {
    public:
        explicit DescriptorManager(Device& device);
        ~DescriptorManager();

        BenzinDefineNonCopyable(DescriptorManager);
        BenzinDefineNonMoveable(DescriptorManager);

    public:
        using DescriptorInitCallback = std::function<void(uint64_t cpuHandle)>;

        ID3D12DescriptorHeap* GetD3D12GpuResourceDescriptorHeap() const;

        Descriptor AllocateDescriptor(DescriptorType descriptorType, const DescriptorInitCallback& initCallback = {});
        void FreeDescriptor(const Descriptor& descriptor);

        void CopyToGpuResourceHeap(Descriptor& descriptor);

    private:
        DescriptorHeap& GetCpuHeap(DescriptorType descriptorType);

    private:
        Device& m_Device;

        std::unique_ptr<DescriptorHeap> m_CpuRtvHeap;
        std::unique_ptr<DescriptorHeap> m_CpuDsvHeap;
        std::unique_ptr<DescriptorHeap> m_CpuResourceHeap;
        std::unique_ptr<DescriptorHeap> m_GpuResourceHeap;
    };

}
