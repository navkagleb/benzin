#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/descriptor_manager.hpp"

#include "benzin/graphics/device.hpp"

namespace benzin
{
 
    namespace
    {

        D3D12_DESCRIPTOR_HEAP_TYPE GetDescriptorHeapType(DescriptorType descripitorType)
        {
            switch (descripitorType)
            {
                using enum DescriptorType;

                case RenderTargetView: return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                case DepthStencilView: return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
                case ConstantBufferView:
                case ShaderResourceView:
                case UnorderedAccessView: return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                case Sampler: return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
                default: std::unreachable();
            }
        }

    } // anonymous namespace

    // DescriptorManager::DescriptorHeap
    DescriptorManager::DescriptorHeap::DescriptorHeap(Device& device, D3D12_DESCRIPTOR_HEAP_TYPE d3d12DescriptorHeapType, uint32_t descriptorCount)
        : m_IsAccessableByShader{ d3d12DescriptorHeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || d3d12DescriptorHeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER }
        , m_DescriptorCount{ descriptorCount }
    {
        const D3D12_DESCRIPTOR_HEAP_DESC d3d12DescriptorHeapDesc
        {
            .Type = d3d12DescriptorHeapType,
            .NumDescriptors = descriptorCount,
            .Flags = m_IsAccessableByShader ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0,
        };

        BenzinAssert(device.GetD3D12Device()->CreateDescriptorHeap(&d3d12DescriptorHeapDesc, IID_PPV_ARGS(&m_D3D12DescriptorHeap)));

        m_DescriptorSize = device.GetD3D12Device()->GetDescriptorHandleIncrementSize(d3d12DescriptorHeapDesc.Type);

        SetD3D12ObjectDebugName(m_D3D12DescriptorHeap, magic_enum::enum_name(d3d12DescriptorHeapType));
    }

    DescriptorManager::DescriptorHeap::~DescriptorHeap()
    {
#if BENZIN_IS_DEBUG_BUILD
        BenzinAssert(m_AllocatedIndexCount == 0);
#endif

        SafeUnknownRelease(m_D3D12DescriptorHeap);
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
        static const uint32_t invalidIndex = std::numeric_limits<uint32_t>::max();

#if BENZIN_IS_DEBUG_BUILD
        m_AllocatedIndexCount += 1;
#endif

        uint32_t index = invalidIndex;

        if (!m_FreeIndices.empty())
        {
            index = m_FreeIndices.back();
            m_FreeIndices.pop_back();
        }
        else
        {
            index = m_Marker++;
        }

        BenzinAssert(index != invalidIndex);
        return index;
    }

    void DescriptorManager::DescriptorHeap::DeallocateIndex(uint32_t index)
    {
        BenzinAssert(std::find(m_FreeIndices.begin(), m_FreeIndices.end(), index) == m_FreeIndices.end());
        m_FreeIndices.push_back(index);

#if BENZIN_IS_DEBUG_BUILD
        m_AllocatedIndexCount--;
#endif
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

    // DescriptorManager
    DescriptorManager::DescriptorManager(Device& device)
    {
        const auto& createDescriptorHeap = [&](D3D12_DESCRIPTOR_HEAP_TYPE d3d12DescriptorHeapType, uint32_t descriptorCount)
        {
            m_DescriptorHeaps[d3d12DescriptorHeapType] = std::make_unique<DescriptorHeap>(device, d3d12DescriptorHeapType, descriptorCount);
        };

        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, config::g_MaxRenderTargetViewDescriptorCount);
        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, config::g_MaxDepthStencilViewDescriptorCount);
        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, config::g_MaxResourceDescriptorCount);
        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, config::g_MaxSamplerDescriptorCount);
    }

    Descriptor DescriptorManager::AllocateDescriptor(DescriptorType descriptorType)
    {
        DescriptorHeap& descriptorHeap = *m_DescriptorHeaps[GetDescriptorHeapType(descriptorType)];
        return descriptorHeap.AllocateDescriptor();
    }

    void DescriptorManager::DeallocateDescriptor(DescriptorType descriptorType, const Descriptor& descriptor)
    {
        DescriptorHeap& descriptorHeap = *m_DescriptorHeaps[GetDescriptorHeapType(descriptorType)];
        descriptorHeap.DeallocateDescriptor(descriptor);
    }

} // namespace benzin
