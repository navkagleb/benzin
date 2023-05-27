#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/descriptor_manager.hpp"

#include "benzin/graphics/api/device.hpp"

namespace benzin
{
 
    namespace
    {

        constexpr uint32_t g_InvalidIndex = std::numeric_limits<uint32_t>::max();

        constexpr D3D12_DESCRIPTOR_HEAP_TYPE GetDescriptorHeapType(Descriptor::Type descripitorType)
        {
            switch (descripitorType)
            {
                using enum Descriptor::Type;

                case RenderTargetView: return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                case DepthStencilView: return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
                case ConstantBufferView:
                case ShaderResourceView:
                case UnorderedAccessView: return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                case Sampler: return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
                default: break;
            }

            BENZIN_ASSERT(false);
            return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        }

    } // anonymous namespace

    //////////////////////////////////////////////////////////////////////////
    /// Descriptor
    //////////////////////////////////////////////////////////////////////////
    Descriptor::Descriptor(uint32_t heapIndex, size_t cpuHandle, uint64_t gpuHandle)
        : m_HeapIndex{ heapIndex }
        , m_CPUHandle{ cpuHandle }
        , m_GPUHandle{ gpuHandle }
    {}

    //////////////////////////////////////////////////////////////////////////
    /// DescriptorManager::DescriptorHeap
    //////////////////////////////////////////////////////////////////////////
    DescriptorManager::DescriptorHeap::DescriptorHeap(Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, std::uint32_t descriptorCount)
    {
        m_IsAccessableByShader = type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

        const D3D12_DESCRIPTOR_HEAP_DESC d3d12DescriptorHeapDesc
        {
            .Type{ type },
            .NumDescriptors{ descriptorCount },
            .Flags{ m_IsAccessableByShader ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE },
            .NodeMask{ 0 }
        };

        BENZIN_HR_ASSERT(device.GetD3D12Device()->CreateDescriptorHeap(&d3d12DescriptorHeapDesc, IID_PPV_ARGS(&m_D3D12DescriptorHeap)));

        m_DescriptorCount = descriptorCount;
        m_DescriptorSize = device.GetD3D12Device()->GetDescriptorHandleIncrementSize(d3d12DescriptorHeapDesc.Type);

        SetDebugName(magic_enum::enum_name(type));
    }

    DescriptorManager::DescriptorHeap::~DescriptorHeap()
    {
        BENZIN_ASSERT(m_AllocatedIndexCount == 0);

        dx::SafeRelease(m_D3D12DescriptorHeap);
    }

    Descriptor DescriptorManager::DescriptorHeap::AllocateDescriptor()
    {
        return GetDescriptor(AllocateIndex());
    }

    void DescriptorManager::DescriptorHeap::DeallocateDescriptor(const Descriptor& descriptor)
    {
        DeallocateIndex(descriptor.GetHeapIndex());
    }

    uint32_t DescriptorManager::DescriptorHeap::AllocateIndex()
    {
        m_AllocatedIndexCount += 1;

        uint32_t index = g_InvalidIndex;

        if (!m_FreeIndices.empty())
        {
            index = m_FreeIndices.back();
            m_FreeIndices.pop_back();
        }
        else
        {
            index = m_Marker++;
        }

        BENZIN_ASSERT(index != g_InvalidIndex);
        return index;
    }

    void DescriptorManager::DescriptorHeap::DeallocateIndex(uint32_t index)
    {
        BENZIN_ASSERT(std::find(m_FreeIndices.begin(), m_FreeIndices.end(), index) == m_FreeIndices.end());

        m_FreeIndices.push_back(index);
        m_AllocatedIndexCount--;
    }

    Descriptor DescriptorManager::DescriptorHeap::GetDescriptor(uint32_t index) const
    {
        return Descriptor
        {
            index,
            GetCPUHandle(index),
            m_IsAccessableByShader ? GetGPUHandle(index) : 0
        };
    }

    uint64_t DescriptorManager::DescriptorHeap::GetCPUHandle(uint32_t index) const
    {
        return m_D3D12DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<uint64_t>(index) * m_DescriptorSize;
    }

    uint64_t DescriptorManager::DescriptorHeap::GetGPUHandle(uint32_t index) const
    {
        return m_D3D12DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + static_cast<uint64_t>(index) * m_DescriptorSize;
    }

    //////////////////////////////////////////////////////////////////////////
    /// DescriptorManager
    //////////////////////////////////////////////////////////////////////////
    DescriptorManager::DescriptorManager(Device& device)
    {
        static const auto& createDescriptorHeap = [this, &device](D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptorCount)
        {
            m_DescriptorHeaps[type] = std::make_unique<DescriptorHeap>(device, type, descriptorCount);
        };

        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, config::GetMaxRenderTargetViewDescriptorCount());
        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, config::GetMaxDepthStencilViewDescriptorCount());
        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, config::GetMaxResourceDescriptorCount());
        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, config::GetMaxSamplerDescriptorCount());
    }

    Descriptor DescriptorManager::AllocateDescriptor(Descriptor::Type descriptorType)
    {
        DescriptorHeap& descriptorHeap = *m_DescriptorHeaps[GetDescriptorHeapType(descriptorType)];
        
        return descriptorHeap.AllocateDescriptor();
    }

    void DescriptorManager::DeallocateDescriptor(Descriptor::Type descriptorType, const Descriptor& descriptor)
    {
        DescriptorHeap& descriptorHeap = *m_DescriptorHeaps[GetDescriptorHeapType(descriptorType)];
        descriptorHeap.DeallocateDescriptor(descriptor);
    }

} // namespace benzin
