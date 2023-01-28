#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/descriptor_manager.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include "benzin/graphics/device.hpp"

namespace benzin
{
 
    namespace
    {

        constexpr DescriptorHeap::Type GetDescriptorHeapTypeFromDescriptorType(Descriptor::Type descripitorType)
        {
            switch (descripitorType)
            {
                case Descriptor::Type::RenderTargetView: return DescriptorHeap::Type::RenderTargetView;
                case Descriptor::Type::DepthStencilView: return DescriptorHeap::Type::DepthStencilView;
                case Descriptor::Type::ConstantBufferView:
                case Descriptor::Type::ShaderResourceView:
                case Descriptor::Type::UnorderedAccessView: return DescriptorHeap::Type::Resource;
                case Descriptor::Type::Sampler: return DescriptorHeap::Type::Sampler;
                default: break;
            }

            BENZIN_ASSERT(false);
        }

    } // anonymous namespace

    //////////////////////////////////////////////////////////////////////////
    /// Descriptor
    //////////////////////////////////////////////////////////////////////////
    Descriptor::Descriptor(uint32_t heapIndex, uint64_t cpuHandle, uint64_t gpuHandle)
        : m_HeapIndex{ heapIndex }
        , m_CPUHandle{ cpuHandle }
        , m_GPUHandle{ gpuHandle }
    {}

    //////////////////////////////////////////////////////////////////////////
    /// DescriptorHeap
    //////////////////////////////////////////////////////////////////////////
    DescriptorHeap::DescriptorHeap(Device& device, Type type, std::uint32_t descriptorCount)
    {
        m_IsAccessableByShader = type == Type::Sampler || type == Type::Resource;

        const D3D12_DESCRIPTOR_HEAP_DESC d3d12DescriptorHeapDesc
        {
            .Type{ static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type) },
            .NumDescriptors{ descriptorCount },
            .Flags{ m_IsAccessableByShader ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE },
            .NodeMask{ 0 }
        };

        BENZIN_D3D12_ASSERT(device.GetD3D12Device()->CreateDescriptorHeap(&d3d12DescriptorHeapDesc, IID_PPV_ARGS(&m_D3D12DescriptorHeap)));

        m_DescriptorCount = descriptorCount;
        m_DescriptorSize = device.GetD3D12Device()->GetDescriptorHandleIncrementSize(d3d12DescriptorHeapDesc.Type);

        SetDebugName(std::string{ magic_enum::enum_name(type) });

        BENZIN_INFO("{} created", GetDebugName());
    }

    DescriptorHeap::~DescriptorHeap()
    {
        BENZIN_INFO("{} destroyed", GetDebugName());

        SafeReleaseD3D12Object(m_D3D12DescriptorHeap);

    }

    Descriptor DescriptorHeap::AllocateDescriptor()
    {
        const uint32_t index = AllocateIndices(1);

        return GetDescriptor(index);
    }

    std::vector<Descriptor> DescriptorHeap::AllocateDescriptors(uint32_t count)
    {
        const uint32_t beginIndex = AllocateIndices(count);

        std::vector<Descriptor> result;
        result.reserve(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            result.push_back(GetDescriptor(beginIndex + i));
        }

        return result;
    }

    void DescriptorHeap::DeallocateDescriptor(const Descriptor& descriptor)
    {
        DeallocateIndex(descriptor.GetHeapIndex());
    }

    uint32_t DescriptorHeap::AllocateIndices(uint32_t count)
    {
        uint32_t beginIndex = 0;

        if (m_FreeIndices.size() >= count)
        {
            beginIndex = m_FreeIndices.front();

            for (size_t i = 0; i < count - 1; ++i)
            {
                if (i - beginIndex == count)
                {
                    m_FreeIndices.erase(m_FreeIndices.begin() + beginIndex, m_FreeIndices.begin() + i);

                    return beginIndex;
                }

                if (m_FreeIndices[i] + 1 != m_FreeIndices[i + 1])
                {
                    beginIndex = m_FreeIndices[i + 1];
                    continue;
                }
            }
        }

        beginIndex = m_Marker;
        m_Marker += count;

        return beginIndex;
    }

    void DescriptorHeap::DeallocateIndex(uint32_t index)
    {
        if (std::find(m_FreeIndices.begin(), m_FreeIndices.end(), index) == m_FreeIndices.end())
        {
            m_FreeIndices.push_back(index);
            std::sort(m_FreeIndices.begin(), m_FreeIndices.end());
        }
    }

    Descriptor DescriptorHeap::GetDescriptor(uint32_t index) const
    {
        return Descriptor
        {
            index,
            GetCPUHandle(index),
            m_IsAccessableByShader ? GetGPUHandle(index) : 0
        };
    }

    uint64_t DescriptorHeap::GetCPUHandle(uint32_t index) const
    {
        return m_D3D12DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<uint64_t>(index) * m_DescriptorSize;
    }

    uint64_t DescriptorHeap::GetGPUHandle(uint32_t index) const
    {
        return m_D3D12DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + static_cast<uint64_t>(index) * m_DescriptorSize;
    }

    //////////////////////////////////////////////////////////////////////////
    /// DescriptorManager
    //////////////////////////////////////////////////////////////////////////
    DescriptorManager::DescriptorManager(Device& device, const Config& config)
    {
        const auto& createDescriptorHeap = [this, &device](DescriptorHeap::Type type, uint32_t descriptorCount)
        {
            m_DescriptorHeaps[type] = std::make_unique<DescriptorHeap>(device, type, descriptorCount);
        };

        createDescriptorHeap(DescriptorHeap::Type::RenderTargetView, config.RenderTargetViewDescriptorCount);
        createDescriptorHeap(DescriptorHeap::Type::DepthStencilView, config.DepthStencilViewDescriptorCount);
        createDescriptorHeap(DescriptorHeap::Type::Resource, config.ResourceDescriptorCount);
        createDescriptorHeap(DescriptorHeap::Type::Sampler, config.SamplerDescriptorCount);
    }

    Descriptor DescriptorManager::AllocateDescriptor(Descriptor::Type descriptorType)
    {
        DescriptorHeap& descriptorHeap = *m_DescriptorHeaps[GetDescriptorHeapTypeFromDescriptorType(descriptorType)];
        
        return descriptorHeap.AllocateDescriptor();
    }

    std::vector<Descriptor> DescriptorManager::AllocateDescriptors(Descriptor::Type descriptorType, uint32_t count)
    {
        DescriptorHeap& descriptorHeap = *m_DescriptorHeaps[GetDescriptorHeapTypeFromDescriptorType(descriptorType)];

        return descriptorHeap.AllocateDescriptors(count);
    }

    void DescriptorManager::DeallocateDescriptor(Descriptor::Type descriptorType, const Descriptor& descriptor)
    {
        DescriptorHeap& descriptorHeap = *m_DescriptorHeaps[GetDescriptorHeapTypeFromDescriptorType(descriptorType)];
        descriptorHeap.DeallocateDescriptor(descriptor);
    }

} // namespace benzin
