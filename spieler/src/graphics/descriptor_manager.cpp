#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/descriptor_manager.hpp"

#include "spieler/core/common.hpp"

#include "spieler/graphics/device.hpp"
#include "spieler/graphics/context.hpp"

#include "platform/dx12/dx12_common.hpp"

namespace spieler
{
    
    //////////////////////////////////////////////////////////////////////////
    /// DescriptorHeap
    //////////////////////////////////////////////////////////////////////////
    DescriptorHeap::DescriptorHeap(Device& device, Type type, std::uint32_t descriptorCount)
    {
        SPIELER_ASSERT(Init(device, type, descriptorCount));
    }

    DescriptorHeap::DescriptorHeap(DescriptorHeap&& other) noexcept
        : m_DX12DescriptorHeap{ std::exchange(other.m_DX12DescriptorHeap, nullptr) }
        , m_DescriptorSize{ std::exchange(other.m_DescriptorSize, 0) }
        , m_DescriptorCount{ std::exchange(other.m_DescriptorCount, 0) }
        , m_Marker{ std::exchange(other.m_Marker, 0) }
    {}

    uint32_t DescriptorHeap::AllocateIndex()
    {
        SPIELER_ASSERT(m_Marker + 1 <= m_DescriptorCount);

        return m_Marker++;
    }

    CPUDescriptorHandle DescriptorHeap::AllocateCPU(uint32_t index)
    {
        return m_DX12DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<uint64_t>(index) * m_DescriptorSize;
    }

    GPUDescriptorHandle DescriptorHeap::AllocateGPU(uint32_t index)
    {
        return m_DX12DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + static_cast<uint64_t>(index) * m_DescriptorSize;
    }

    bool DescriptorHeap::Init(Device& device, Type type, uint32_t descriptorCount)
    {
        const D3D12_DESCRIPTOR_HEAP_DESC desc
        {
            .Type{ dx12::Convert(type) },
            .NumDescriptors{ descriptorCount },
            .Flags{ type == Type::Sampler || type == Type::CBV_SRV_UAV ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE },
            .NodeMask{ 0 }
        };

        SPIELER_RETURN_IF_FAILED(device.GetDX12Device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DX12DescriptorHeap)));

        m_DescriptorCount = descriptorCount;
        m_DescriptorSize = device.GetDX12Device()->GetDescriptorHandleIncrementSize(desc.Type);

        return true;
    }

    DescriptorHeap& DescriptorHeap::operator=(DescriptorHeap&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_DX12DescriptorHeap = std::exchange(other.m_DX12DescriptorHeap, nullptr);
        m_DescriptorSize = std::exchange(other.m_DescriptorSize, 0);
        m_DescriptorCount = std::exchange(other.m_DescriptorCount, 0);
        m_Marker = std::exchange(other.m_Marker, 0);

        return *this;
    }

    //////////////////////////////////////////////////////////////////////////
    /// DescriptorManager
    //////////////////////////////////////////////////////////////////////////
    DescriptorManager::DescriptorManager(Device& device, const Config& config)
    {
        m_DescriptorHeaps[DescriptorHeap::Type::CBV_SRV_UAV] = DescriptorHeap{ device, DescriptorHeap::Type::CBV_SRV_UAV, config.CBV_SRV_UAVDescriptorCount };
        m_DescriptorHeaps[DescriptorHeap::Type::Sampler] = DescriptorHeap{ device, DescriptorHeap::Type::Sampler, config.SamplerDescriptorCount };
        m_DescriptorHeaps[DescriptorHeap::Type::RTV] = DescriptorHeap{ device, DescriptorHeap::Type::RTV, config.RTVDescriptorCount };
        m_DescriptorHeaps[DescriptorHeap::Type::DSV] = DescriptorHeap{ device, DescriptorHeap::Type::DSV, config.DSVDescriptorCount };
    }

    DescriptorManager::DescriptorManager(DescriptorManager&& other) noexcept
        : m_DescriptorHeaps{ std::exchange(other.m_DescriptorHeaps, {}) }
    {}

    RTVDescriptor DescriptorManager::AllocateRTV()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeap::Type::RTV] };

        return RTVDescriptor
        {
            .CPU{ heap.AllocateCPU(heap.AllocateIndex()) }
        };
    }

    DSVDescriptor DescriptorManager::AllocateDSV()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeap::Type::DSV] };

        return DSVDescriptor
        {
            .CPU{ heap.AllocateCPU(heap.AllocateIndex()) }
        };
    }

    SamplerDescriptor DescriptorManager::AllocateSampler()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeap::Type::Sampler] };

        const uint32_t heapIndex{ heap.AllocateIndex() };

        return SamplerDescriptor
        {
            .HeapIndex{ heapIndex },
            .CPU{ heap.AllocateCPU(heapIndex) },
            .GPU{ heap.AllocateGPU(heapIndex) }
        };
    }

    CBVDescriptor DescriptorManager::AllocateCBV()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeap::Type::CBV] };

        const uint32_t heapIndex{ heap.AllocateIndex() };

        return CBVDescriptor
        {
            .HeapIndex{ heapIndex },
            .CPU{ heap.AllocateCPU(heapIndex) },
            .GPU{ heap.AllocateGPU(heapIndex) }
        };
    }

    SRVDescriptor DescriptorManager::AllocateSRV()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeap::Type::SRV] };

        const uint32_t heapIndex{ heap.AllocateIndex() };

        return SRVDescriptor
        {
            .HeapIndex{ heapIndex },
            .CPU{ heap.AllocateCPU(heapIndex) },
            .GPU{ heap.AllocateGPU(heapIndex) }
        };
    }

    UAVDescriptor DescriptorManager::AllocateUAV()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeap::Type::UAV] };

        const uint32_t heapIndex{ heap.AllocateIndex() };

        return UAVDescriptor
        {
            .HeapIndex{ heapIndex },
            .CPU{ heap.AllocateCPU(heapIndex) },
            .GPU{ heap.AllocateGPU(heapIndex) }
        };
    }

    DescriptorManager& DescriptorManager::operator=(DescriptorManager&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_DescriptorHeaps = std::exchange(other.m_DescriptorHeaps, {});

        return *this;
    }

} // namespace spieler
