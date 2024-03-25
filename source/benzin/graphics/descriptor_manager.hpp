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
        Sampler,
    };

    class Descriptor
    {
    public:
        Descriptor() = default;

        Descriptor(DescriptorType type, uint32_t heapIndex, uint64_t cpuHandle, uint64_t gpuHandle = 0)
            : m_Type{ type }
            , m_HeapIndex{ heapIndex }
            , m_CpuHandle{ cpuHandle }
            , m_GpuHandle{ gpuHandle }
        {}
        
        auto GetType() const { return m_Type; }
        auto GetHeapIndex() const { return m_HeapIndex; }
        auto GetCpuHandle() const { return m_CpuHandle; }
        auto GetGpuHandle() const { return m_GpuHandle; }

        bool IsCpuValid() const { return m_CpuHandle != 0; }
        bool IsGpuValid() const { return IsCpuValid() && m_GpuHandle != 0; }

    private:
        DescriptorType m_Type = g_InvalidEnumValue<DescriptorType>;
        uint32_t m_HeapIndex = 0;
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
        ID3D12DescriptorHeap* GetD3D12GpuResourceDescriptorHeap() const;
        ID3D12DescriptorHeap* GetD3D12SamplerDescriptorHeap() const;

        Descriptor AllocateDescriptor(DescriptorType descriptorType);
        void FreeDescriptor(const Descriptor& descriptor);

    private:
        std::array<std::unique_ptr<DescriptorHeap>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_DescriptorHeaps;
    };

} // namespace benzin
