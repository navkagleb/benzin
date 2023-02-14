#pragma once

#include "benzin/graphics/api/common.hpp"

namespace benzin
{

    class Descriptor
    {
    public:
        friend class GraphicsCommandList;

    public:
        enum Type : uint8_t
        {
            Unknown,

            RenderTargetView,
            DepthStencilView,
            ConstantBufferView,
            ShaderResourceView,
            UnorderedAccessView,
            Sampler
        };

    public:
        Descriptor() = default;
        Descriptor(uint32_t heapIndex, uint64_t cpuHandle, uint64_t gpuHandle = 0);

    public:
        uint32_t GetHeapIndex() const { return m_HeapIndex; }
        uint64_t GetCPUHandle() const { return m_CPUHandle; }
        uint64_t GetGPUHandle() const { BENZIN_ASSERT(m_GPUHandle != 0);  return m_GPUHandle; }

    public:
        bool IsValid() const { return m_CPUHandle != 0; }

    private:
        uint32_t m_HeapIndex{ 0 };
        uint64_t m_CPUHandle{ 0 };
        uint64_t m_GPUHandle{ 0 };
    };

    class DescriptorHeap
    {
    public:
        BENZIN_NON_COPYABLE(DescriptorHeap)
        BENZIN_NON_MOVEABLE(DescriptorHeap)
        BENZIN_DEBUG_NAME_D3D12_OBJECT(m_D3D12DescriptorHeap, "DescriptorHeap")

    public:
        enum class Type : uint8_t
        {
            RenderTargetView = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            DepthStencilView = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            Resource = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            Sampler = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
        };

    public:
        DescriptorHeap() = default;
        DescriptorHeap(Device& device, Type type, uint32_t descriptorCount);
        ~DescriptorHeap();

    public:
        ID3D12DescriptorHeap* GetD3D12DescriptorHeap() const { return m_D3D12DescriptorHeap; }

    public:
        Descriptor AllocateDescriptor();
        std::vector<Descriptor> AllocateDescriptors(uint32_t count);
        void DeallocateDescriptor(const Descriptor& descriptor);

    private:
        uint32_t AllocateIndices(uint32_t count);
        void DeallocateIndex(uint32_t index);

        Descriptor GetDescriptor(uint32_t index) const;
        uint64_t GetCPUHandle(uint32_t index) const;
        uint64_t GetGPUHandle(uint32_t index) const;

    private:
        ID3D12DescriptorHeap* m_D3D12DescriptorHeap{ nullptr };

        bool m_IsAccessableByShader{ false };
        uint32_t m_DescriptorSize{ 0 };
        uint32_t m_DescriptorCount{ 0 };

        uint32_t m_Marker{ 0 };
        std::vector<uint32_t> m_FreeIndices;
    };

    class DescriptorManager
    {
    public:
        struct Config
        {
            uint32_t RenderTargetViewDescriptorCount{ 0 };
            uint32_t DepthStencilViewDescriptorCount{ 0 };
            uint32_t ResourceDescriptorCount{ 0 };
            uint32_t SamplerDescriptorCount{ 0 };
        };

    public:
        BENZIN_NON_COPYABLE(DescriptorManager)
        BENZIN_NON_MOVEABLE(DescriptorManager)

    public:
        DescriptorManager(Device& device, const Config& config);

    public:
        ID3D12DescriptorHeap* GetD3D12ResourceDescriptorHeap() const { return m_DescriptorHeaps.at(DescriptorHeap::Type::Resource)->GetD3D12DescriptorHeap(); }
        ID3D12DescriptorHeap* GetD3D12SamplerDescriptorHeap() const { return m_DescriptorHeaps.at(DescriptorHeap::Type::Sampler)->GetD3D12DescriptorHeap(); }

        Descriptor AllocateDescriptor(Descriptor::Type descriptorType);
        std::vector<Descriptor> AllocateDescriptors(Descriptor::Type descriptorType, uint32_t count);

        void DeallocateDescriptor(Descriptor::Type descriptorType, const Descriptor& descriptor);

    private:
        std::unordered_map<DescriptorHeap::Type, std::unique_ptr<DescriptorHeap>> m_DescriptorHeaps;
    };

} // namespace benzin
