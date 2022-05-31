#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/descriptor_manager.hpp"

#include "spieler/core/assert.hpp"
#include "spieler/core/common.hpp"

#include "spieler/renderer/device.hpp"
#include "spieler/renderer/context.hpp"

namespace spieler::renderer
{

    DescriptorManager::DescriptorHeap::DescriptorHeap(Device& device, DescriptorHeapType type, std::uint32_t descriptorCount)
    {
        SPIELER_ASSERT(Init(device, type, descriptorCount));
    }

    CPUDescriptorHandle DescriptorManager::DescriptorHeap::AllocateCPU(uint32_t index)
    {
        return m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + index * m_DescriptorSize;
    }

    GPUDescriptorHandle DescriptorManager::DescriptorHeap::AllocateGPU(uint32_t index)
    {
        return m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + index * m_DescriptorSize;
    }

    void DescriptorManager::DescriptorHeap::Bind(Context& context) const
    {
        SPIELER_ASSERT(m_DescriptorHeap);

        context.GetNativeCommandList()->SetDescriptorHeaps(1, m_DescriptorHeap.GetAddressOf());
    }

    bool DescriptorManager::DescriptorHeap::Init(Device& device, DescriptorHeapType type, uint32_t descriptorCount)
    {
        ComPtr<ID3D12Device>& d3d12Device{ device.GetNativeDevice() };

        D3D12_DESCRIPTOR_HEAP_DESC desc
        {
            .Type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type),
            .NumDescriptors = descriptorCount,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0
        };

        if (desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
        {
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        }

        SPIELER_RETURN_IF_FAILED(d3d12Device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &m_DescriptorHeap));

        m_DescriptorCount = descriptorCount;
        m_DescriptorSize = d3d12Device->GetDescriptorHandleIncrementSize(desc.Type);

        return true;
    }

    uint32_t DescriptorManager::DescriptorHeap::AllocateIndex()
    {
        SPIELER_ASSERT(m_Marker + 1 <= m_DescriptorCount);

        return m_Marker++;
    }

    DescriptorManager::DescriptorManager(Device& device, const DescriptorManagerConfig& config)
    {
        m_DescriptorHeaps[DescriptorHeapType::RTV] = DescriptorHeap(device, DescriptorHeapType::RTV, config.RTVDescriptorCount);
        m_DescriptorHeaps[DescriptorHeapType::DSV] = DescriptorHeap(device, DescriptorHeapType::DSV, config.DSVDescriptorCount);
        m_DescriptorHeaps[DescriptorHeapType::Sampler] = DescriptorHeap(device, DescriptorHeapType::Sampler, config.SamplerDescriptorCount);
        m_DescriptorHeaps[DescriptorHeapType::CBV] = DescriptorHeap(device, DescriptorHeapType::CBV, config.CBVDescriptorCount);
        m_DescriptorHeaps[DescriptorHeapType::SRV] = DescriptorHeap(device, DescriptorHeapType::SRV, config.SRVDescriptorCount);
        m_DescriptorHeaps[DescriptorHeapType::UAV] = DescriptorHeap(device, DescriptorHeapType::UAV, config.UAVDescriptorCount);
    }

    void DescriptorManager::Bind(Context& context, DescriptorHeapType type) const
    {
        m_DescriptorHeaps.at(type).Bind(context);
    }

    RTVDescriptor DescriptorManager::AllocateRTV()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeapType::RTV] };

        return RTVDescriptor
        {
            .CPU = heap.AllocateCPU(heap.AllocateIndex())
        };
    }

    DSVDescriptor DescriptorManager::AllocateDSV()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeapType::DSV] };

        DSVDescriptor descriptor;
        descriptor.CPU = heap.AllocateCPU(heap.AllocateIndex());

        return descriptor;
    }

    SamplerDescriptor DescriptorManager::AllocateSampler()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeapType::Sampler] };

        SamplerDescriptor descriptor;
        descriptor.HeapIndex = heap.AllocateIndex();
        descriptor.CPU = heap.AllocateCPU(descriptor.HeapIndex);
        descriptor.GPU = heap.AllocateGPU(descriptor.HeapIndex);

        return descriptor;
    }

    CBVDescriptor DescriptorManager::AllocateCBV()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeapType::CBV] };

        CBVDescriptor descriptor;
        descriptor.HeapIndex = heap.AllocateIndex();
        descriptor.CPU = heap.AllocateCPU(descriptor.HeapIndex);
        descriptor.GPU = heap.AllocateGPU(descriptor.HeapIndex);

        return descriptor;
    }

    SRVDescriptor DescriptorManager::AllocateSRV()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeapType::SRV] };

        SRVDescriptor descriptor;
        descriptor.HeapIndex = heap.AllocateIndex();
        descriptor.CPU = heap.AllocateCPU(descriptor.HeapIndex);
        descriptor.GPU = heap.AllocateGPU(descriptor.HeapIndex);

        return descriptor;
    }

    UAVDescriptor DescriptorManager::AllocateUAV()
    {
        DescriptorHeap& heap{ m_DescriptorHeaps[DescriptorHeapType::UAV] };

        UAVDescriptor descriptor;
        descriptor.HeapIndex = heap.AllocateIndex();
        descriptor.CPU = heap.AllocateCPU(descriptor.HeapIndex);
        descriptor.GPU = heap.AllocateGPU(descriptor.HeapIndex);

        return descriptor;
    }

} // namespace spieler::renderer