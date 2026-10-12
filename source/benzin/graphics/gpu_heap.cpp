#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/gpu_heap.hpp>

#include <benzin/core/math.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>

namespace benzin
{

    extern D3D12_RESOURCE_DESC ToD3D12ResourceDesc(const BufferCreation& creation);
    extern D3D12_RESOURCE_DESC ToD3D12ResourceDesc(const TextureCreation& creation);

    // GpuHeap

    GpuHeap::GpuHeap(Device& device, const GpuHeapCreation& creation)
        : m_Device{ device }
    {
        BenzinAssert(!IsMaxEnum(creation.m_Type));
        BenzinAssert(creation.m_SizeInBytes != 0);

        D3D12_HEAP_DESC d3d12HeapDesc = {};
        d3d12HeapDesc.SizeInBytes = creation.m_SizeInBytes;
        d3d12HeapDesc.Properties = GetD3D12HeapProperties(ToD3D12HeapType(m_Device, creation.m_Type));
        d3d12HeapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        d3d12HeapDesc.Flags = D3D12_HEAP_FLAG_NONE;

        BenzinD3D12Call(device.GetD3D12Device()->CreateHeap(&d3d12HeapDesc, IID_PPV_ARGS(&m_D3D12Heap)));
        BenzinEnsure(m_D3D12Heap != nullptr);

        SetD3DObjectDebugName(m_D3D12Heap, creation.m_DebugName);

        m_Type = creation.m_Type;
        m_SizeInBytes = creation.m_SizeInBytes;
    }

    GpuHeap::~GpuHeap()
    {
        m_Device.DeferredRelease(m_D3D12Heap);
        m_D3D12Heap = nullptr;
    }

    // GpuHeapLinearAllocator

    GpuHeapLinearAllocator::GpuHeapLinearAllocator(GpuHeap& gpuHeap)
        : m_GpuHeap{ gpuHeap }
    {
        BenzinAssert(gpuHeap.GetD3D12Heap() != nullptr);
    }

    void GpuHeapLinearAllocator::ResetOffset()
    {
        m_OffsetInBytes = 0;
    }

    std::unique_ptr<Buffer> GpuHeapLinearAllocator::AllocateBuffer(BufferConfigurator configurator)
    {
        BufferCreation creation;
        configurator(creation);

        BenzinAssert(IsMaxEnum(creation.m_HeapType));

        return Allocate<Buffer>(creation);
    }

    std::unique_ptr<Buffer> GpuHeapLinearAllocator::AllocateStructuredBuffer(std::string_view debugName, uint32_t elementCount, uint32_t elementSizeInBytes)
    {
        return AllocateBuffer([&](BufferCreation& creation)
        {
            creation.m_DebugName = debugName;
            creation.m_Type = BufferType::Structured;
            creation.m_ElementSizeInBytes = elementSizeInBytes;
            creation.m_ElementCount = elementCount;
        });
    }

    std::unique_ptr<Buffer> GpuHeapLinearAllocator::AllocateFormatBuffer(std::string_view debugName, uint32_t elementCount, DXGI_FORMAT dxgiFormat)
    {
        BenzinAssert(dxgiFormat != DXGI_FORMAT_UNKNOWN);

        return AllocateBuffer([&](BufferCreation& creation)
        {
            creation.m_DebugName = debugName;
            creation.m_Type = BufferType::Format;
            creation.m_DxgiFormat = dxgiFormat;
            creation.m_ElementSizeInBytes = GetDxgiFormatSizeInBytes(dxgiFormat);
            creation.m_ElementCount = elementCount;
        });
    }

    std::unique_ptr<Texture> GpuHeapLinearAllocator::AllocateTexture(TextureConfigurator configurator)
    {
        BenzinAssert(m_GpuHeap.GetType() == GpuHeapType::Default);
        
        TextureCreation creation;
        configurator(creation);

        return Allocate<Texture>(creation);
    }

    template <typename ResourceT, typename CreationT>
    std::unique_ptr<ResourceT> GpuHeapLinearAllocator::Allocate(CreationT& creation)
    {
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ToD3D12ResourceDesc(creation);
        const D3D12_RESOURCE_ALLOCATION_INFO d3d12AllocationInfo = m_GpuHeap.m_Device.GetD3D12Device()->GetResourceAllocationInfo(0, 1, &d3d12ResourceDesc);
        BenzinAssert(d3d12AllocationInfo.Alignment == D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

        const uint64_t alignedOffsetInBytes = AlignUp(m_OffsetInBytes, d3d12AllocationInfo.Alignment);
        const uint64_t sizeInBytes = d3d12AllocationInfo.SizeInBytes;

        const uint64_t neededSizeInBytes = alignedOffsetInBytes + sizeInBytes;
        BenzinEnsure(
            neededSizeInBytes <= m_GpuHeap.GetSizeInBytes(),
            "GpuHeap is full. Needed size: {:.2f} mb, Actual size: {:.2f} mb",
            ToMb(neededSizeInBytes),
            ToMb(m_GpuHeap.GetSizeInBytes()));

        m_OffsetInBytes = alignedOffsetInBytes + sizeInBytes;

        if (creation.m_DebugName.empty())
        {
            creation.m_DebugName = "GpuHeapLinearAllocator::Resource";
        }

        return std::make_unique<ResourceT>(m_GpuHeap, alignedOffsetInBytes, creation);
    }

    // ConstBufferLinearAllocator

    ConstBufferLinearAllocator::ConstBufferLinearAllocator(Device& device, uint32_t sizeInBytesPerFrame)
        : m_Device{ device }
    {
        MakeUniquePtr(m_GpuHeap, m_Device, GpuHeapCreation
        {
            .m_DebugName = "ConstBufferHeap",
            .m_Type = GpuHeapType::GpuUpload,
            .m_SizeInBytes = sizeInBytesPerFrame * BENZIN_FRAME_COUNT,
        });

        for (uint32_t i = 0; i < BENZIN_FRAME_COUNT; ++i)
        {
            const uint64_t gpuHeapOffsetInBytes = sizeInBytesPerFrame * i;
            MakeUniquePtr(m_FrameBuffers[i], *m_GpuHeap, gpuHeapOffsetInBytes, BufferCreation
            {
                .m_DebugName = std::format("FrameConstBuffer_{}", i),
                .m_Type = BufferType::Byte,
                .m_ElementSizeInBytes = sizeof(std::byte),
                .m_ElementCount = sizeInBytesPerFrame,
            });
        }
    }

    void ConstBufferLinearAllocator::ResetFrameBuffer()
    {
        m_FrameBuffer = m_FrameBuffers[m_Device.GetActiveFrameIndex()].get();
        m_FrameBufferWriter.ResetTargetBuffer(m_FrameBuffer->GetCpuMappedData(), m_FrameBuffer->GetSizeInBytes());
    }

    uint64_t ConstBufferLinearAllocator::Allocate(std::span<const std::byte> data)
    {
        const uint64_t offsetInBytes = m_FrameBufferWriter.GetPositionInBytes();
        BenzinAssert(offsetInBytes % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0);

        m_FrameBufferWriter.WriteData(data);
        m_FrameBufferWriter.SetPositionInBytes(AlignUp(m_FrameBufferWriter.GetPositionInBytes(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));

        return m_FrameBuffer->GetGpuVirtualAddress() + offsetInBytes;
    }

}
