#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/gpu_heap.hpp>

#include <benzin/core/math.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>

namespace benzin
{

    // GpuHeap

    GpuHeap::GpuHeap(Device& device, const GpuHeapCreation& creation)
        : m_Device{ device }
    {
        BenzinAssert(!IsMaxEnum(creation.Type));
        BenzinAssert(creation.SizeInBytes != 0);

        D3D12_HEAP_DESC d3d12HeapDesc = {};
        d3d12HeapDesc.SizeInBytes = creation.SizeInBytes;
        d3d12HeapDesc.Properties = GetD3D12HeapProperties(ToD3D12HeapType(m_Device, creation.Type));
        d3d12HeapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        d3d12HeapDesc.Flags = D3D12_HEAP_FLAG_NONE;

        BenzinD3D12Call(device.GetD3D12Device()->CreateHeap(&d3d12HeapDesc, IID_PPV_ARGS(&m_D3D12Heap)));
        BenzinEnsure(m_D3D12Heap != nullptr);

        SetD3DObjectDebugName(m_D3D12Heap, creation.DebugName);

        m_Type = creation.Type;
        m_SizeInBytes = creation.SizeInBytes;
    }

    GpuHeap::~GpuHeap()
    {
        m_Device.DeferredRelease(m_D3D12Heap);
        m_D3D12Heap = nullptr;
    }

    // GpuHeapLinearBufferAllocator

    GpuHeapLinearBufferAllocator::GpuHeapLinearBufferAllocator(GpuHeap& gpuHeap)
        : m_GpuHeap{ gpuHeap }
    {
        BenzinAssert(gpuHeap.GetD3D12Heap() != nullptr);
    }

    std::unique_ptr<Buffer> GpuHeapLinearBufferAllocator::AllocateBuffer(const BufferConfigurator& configurator)
    {
        BenzinAssert(configurator);

        BufferCreation bufferCreation;
        configurator(bufferCreation);

        BenzinAssert(IsMaxEnum(bufferCreation.m_HeapType));

        return AllocateBuffer(bufferCreation);
    }

    std::unique_ptr<Buffer> GpuHeapLinearBufferAllocator::AllocateStructuredBuffer(std::string_view debugName, uint32_t elementCount, uint32_t elementSizeInBytes)
    {
        return AllocateBuffer(BufferCreation
        {
            .m_DebugName = debugName,
            .m_Type = BufferType::Structured,
            .m_ElementSizeInBytes = elementSizeInBytes,
            .m_ElementCount = elementCount,
        });
    }

    std::unique_ptr<Buffer> GpuHeapLinearBufferAllocator::AllocateFormatBuffer(std::string_view debugName, uint32_t elementCount, DXGI_FORMAT dxgiFormat)
    {
        BenzinAssert(dxgiFormat != DXGI_FORMAT_UNKNOWN);

        return AllocateBuffer(BufferCreation
        {
            .m_DebugName = debugName,
            .m_Type = BufferType::Format,
            .m_DxgiFormat = dxgiFormat,
            .m_ElementSizeInBytes = GetDxgiFormatSizeInBytes(dxgiFormat),
            .m_ElementCount = elementCount,
        });
    }

    void GpuHeapLinearBufferAllocator::Reset()
    {
        m_OffsetInBytes = 0;
    }

    std::unique_ptr<Buffer> GpuHeapLinearBufferAllocator::AllocateBuffer(const BufferCreation& bufferCreation)
    {
        const uint64_t alignedOffsetInBytes = AlignUp(m_OffsetInBytes, (uint64_t)D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
        const uint64_t bufferSizeInBytes = (uint64_t)bufferCreation.m_ElementCount * bufferCreation.m_ElementSizeInBytes;

        const uint64_t neededSizeInBytes = alignedOffsetInBytes + bufferSizeInBytes;
        BenzinEnsure(
            neededSizeInBytes <= m_GpuHeap.GetSizeInBytes(),
            "GpuHeap is full. Needed size: {:.2f}, Actual size: {:.2f}",
            ToMb(neededSizeInBytes),
            ToMb(m_GpuHeap.GetSizeInBytes()));

        m_OffsetInBytes = alignedOffsetInBytes + bufferSizeInBytes;

        if (bufferCreation.m_DebugName.empty())
        {
            const_cast<BufferCreation&>(bufferCreation).m_DebugName = "GpuHeapLinearBufferAllocator";
        }

        return std::make_unique<Buffer>(m_GpuHeap, alignedOffsetInBytes, bufferCreation);
    }

    // ConstBufferLinearAllocator

    ConstBufferLinearAllocator::ConstBufferLinearAllocator(Device& device)
        : m_Device{ device }
    {
        constexpr uint64_t bufferSizeInBytesPerFrame = 2_mb;

        MakeUniquePtr(m_GpuHeap, m_Device, GpuHeapCreation
        {
            .DebugName = "ConstBufferHeap",
            .Type = GpuHeapType::GpuUpload,
            .SizeInBytes = bufferSizeInBytesPerFrame * BENZIN_FRAME_COUNT,
        });

        for (uint32_t i = 0; i < BENZIN_FRAME_COUNT; ++i)
        {
            const uint64_t gpuHeapOffsetInBytes = bufferSizeInBytesPerFrame * i;
            MakeUniquePtr(m_FrameBuffers[i], *m_GpuHeap, gpuHeapOffsetInBytes, BufferCreation
            {
                .m_DebugName = std::format("FrameConstBuffer_{}", i),
                .m_Type = BufferType::Byte,
                .m_ElementSizeInBytes = sizeof(std::byte),
                .m_ElementCount = bufferSizeInBytesPerFrame,
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
