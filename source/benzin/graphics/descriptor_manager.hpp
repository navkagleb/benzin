#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Device;

    enum class DescriptorType : uint8_t
    {
        RenderTargetView,
        DepthStencilView,
        ConstantBufferView,
        ShaderResourceView,
        UnorderedAccessView,
        Sampler,
    };

    class Descriptor
    {
    public:
        Descriptor() = default;
        Descriptor(uint32_t heapIndex, uint64_t cpuHandle, uint64_t gpuHandle = 0)
            : m_HeapIndex{ heapIndex }
            , m_CpuHandle{ cpuHandle }
            , m_GpuHandle{ gpuHandle }
        {}

    public:
        uint32_t GetHeapIndex() const;
        uint64_t GetCpuHandle() const;
        uint64_t GetGpuHandle() const;

        bool IsValid() const { return m_CpuHandle != 0; }

    private:
        uint32_t m_HeapIndex = 0;
        uint64_t m_CpuHandle = 0;
        uint64_t m_GpuHandle = 0;
    };

    class DescriptorManager
    {
    private:
        class DescriptorHeap
        {
        public:
            BenzinDefineNonCopyable(DescriptorHeap);
            BenzinDefineNonMoveable(DescriptorHeap);

        public:
            DescriptorHeap(Device& device, D3D12_DESCRIPTOR_HEAP_TYPE d3d12DescriptorHeapType, uint32_t descriptorCount);
            ~DescriptorHeap();

        public:
            ID3D12DescriptorHeap* GetD3D12DescriptorHeap() const { return m_D3D12DescriptorHeap; }

        public:
            Descriptor AllocateDescriptor();
            void DeallocateDescriptor(const Descriptor& descriptor);

        private:
            uint32_t AllocateIndex();
            void DeallocateIndex(uint32_t index);

            Descriptor GetDescriptor(uint32_t index) const;
            uint64_t GetCpuHandle(uint32_t index) const;
            uint64_t GetGpuHandle(uint32_t index) const;

        private:
            ID3D12DescriptorHeap* m_D3D12DescriptorHeap = nullptr;

            bool m_IsAccessableByShader = false;
            uint32_t m_DescriptorSize = 0;
            uint32_t m_DescriptorCount = 0;

            uint32_t m_Marker = 0;
            std::vector<uint32_t> m_FreeIndices;

#if BENZIN_IS_DEBUG_BUILD
            uint32_t m_AllocatedIndexCount = 0;
#endif
        };

    public:
        BenzinDefineNonCopyable(DescriptorManager);
        BenzinDefineNonMoveable(DescriptorManager);

    public:
        explicit DescriptorManager(Device& device);

    public:
        ID3D12DescriptorHeap* GetD3D12ResourceDescriptorHeap() const { return m_DescriptorHeaps.at(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetD3D12DescriptorHeap(); }
        ID3D12DescriptorHeap* GetD3D12SamplerDescriptorHeap() const { return m_DescriptorHeaps.at(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)->GetD3D12DescriptorHeap(); }

        Descriptor AllocateDescriptor(DescriptorType descriptorType);
        void DeallocateDescriptor(DescriptorType descriptorType, const Descriptor& descriptor);

    private:
        std::array<std::unique_ptr<DescriptorHeap>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_DescriptorHeaps;
    };

} // namespace benzin
