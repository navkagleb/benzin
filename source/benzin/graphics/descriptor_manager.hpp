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
        Sampler
    };

    class Descriptor
    {
    public:
        Descriptor() = default;
        Descriptor(uint32_t heapIndex, uint64_t cpuHandle, uint64_t gpuHandle = 0)
            : m_HeapIndex{ heapIndex }
            , m_CPUHandle{ cpuHandle }
            , m_GPUHandle{ gpuHandle }
        {}

    public:
        uint32_t GetHeapIndex() const { Assert{ IsValid() }; return m_HeapIndex; }
        uint64_t GetCPUHandle() const { Assert{ IsValid() };  return m_CPUHandle; }
        uint64_t GetGPUHandle() const { Assert{ IsValid() && m_GPUHandle != 0 }; return m_GPUHandle; }

    public:
        bool IsValid() const { return m_CPUHandle != 0; }

    private:
        uint32_t m_HeapIndex = 0;
        uint64_t m_CPUHandle = 0;
        uint64_t m_GPUHandle = 0;
    };

    class DescriptorManager
    {
    private:
        class DescriptorHeap
        {
        public:
            BENZIN_NON_COPYABLE_IMPL(DescriptorHeap)
            BENZIN_NON_MOVEABLE_IMPL(DescriptorHeap)

        public:
            DescriptorHeap() = default;
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
            uint64_t GetCPUHandle(uint32_t index) const;
            uint64_t GetGPUHandle(uint32_t index) const;

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
        BENZIN_NON_COPYABLE_IMPL(DescriptorManager)
        BENZIN_NON_MOVEABLE_IMPL(DescriptorManager)

    public:
        explicit DescriptorManager(Device& device);

    public:
        ID3D12DescriptorHeap* GetD3D12ResourceDescriptorHeap() const { return m_DescriptorHeaps.at(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetD3D12DescriptorHeap(); }
        ID3D12DescriptorHeap* GetD3D12SamplerDescriptorHeap() const { return m_DescriptorHeaps.at(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)->GetD3D12DescriptorHeap(); }

        Descriptor AllocateDescriptor(DescriptorType descriptorType);
        void DeallocateDescriptor(DescriptorType descriptorType, const Descriptor& descriptor);

    private:
        magic_enum::containers::array<D3D12_DESCRIPTOR_HEAP_TYPE, std::unique_ptr<DescriptorHeap>> m_DescriptorHeaps;
    };

} // namespace benzin
