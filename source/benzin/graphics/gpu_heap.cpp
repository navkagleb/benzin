#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/gpu_heap.hpp>

#include <benzin/core/cmd_line_args.hpp>
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
        BenzinAssert(IsGoodEnum(creation.Type));
        BenzinAssert(creation.SizeInBytes != 0);

        const D3D12_HEAP_DESC d3d12HeapDesc
        {
            .SizeInBytes = creation.SizeInBytes,
            .Properties = GetD3D12HeapProperties(ToD3D12HeapType(m_Device, creation.Type)),
            .Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
            .Flags = D3D12_HEAP_FLAG_NONE,
        };

        BenzinD3D12Call(device.GetD3D12Device()->CreateHeap(&d3d12HeapDesc, IID_PPV_ARGS(&m_D3D12Heap)));
        BenzinEnsure(m_D3D12Heap != nullptr);

        SetD3DObjectDebugName(m_D3D12Heap, creation.DebugName);

        m_Type = creation.Type;
        m_SizeInBytes = creation.SizeInBytes;
    }

    GpuHeap::~GpuHeap()
    {
        m_Device.DeferredRelease(m_D3D12Heap);
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

        BenzinAssert(!IsGoodEnum(bufferCreation.HeapType));

        return AllocateBuffer(bufferCreation);
    }

    std::unique_ptr<Buffer> GpuHeapLinearBufferAllocator::AllocateStructuredBuffer(std::string_view debugName, uint32_t elementCount, uint32_t elementSizeInBytes)
    {
        return AllocateBuffer(BufferCreation
        {
            .DebugName = debugName,
            .Type = BufferType::Structured,
            .ElementSizeInBytes = elementSizeInBytes,
            .ElementCount = elementCount,
        });
    }

    std::unique_ptr<Buffer> GpuHeapLinearBufferAllocator::AllocateFormatBuffer(std::string_view debugName, uint32_t elementCount, GraphicsFormat format)
    {
        BenzinAssert(format != GraphicsFormat::Unknown);

        return AllocateBuffer(BufferCreation
        {
            .DebugName = debugName,
            .Type = BufferType::Format,
            .Format = format,
            .ElementSizeInBytes = GetFormatSizeInBytes(format),
            .ElementCount = elementCount,
        });
    }

    void GpuHeapLinearBufferAllocator::Reset()
    {
        m_OffsetInBytes = 0;
    }

    std::unique_ptr<Buffer> GpuHeapLinearBufferAllocator::AllocateBuffer(const BufferCreation& bufferCreation)
    {
        const uint64_t alignedOffsetInBytes = AlignUp(m_OffsetInBytes, (uint64_t)D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
        const uint64_t bufferSizeInBytes = (uint64_t)bufferCreation.ElementCount * bufferCreation.ElementSizeInBytes;

        const uint64_t neededSizeInBytes = alignedOffsetInBytes + bufferSizeInBytes;
        BenzinEnsure(
            neededSizeInBytes <= m_GpuHeap.GetSizeInBytes(),
            "GpuHeap is full. Needed size: {:.2f}, Actual size: {:.2f}",
            ToMb(neededSizeInBytes),
            ToMb(m_GpuHeap.GetSizeInBytes())
        );

        m_OffsetInBytes = alignedOffsetInBytes + bufferSizeInBytes;

        if (bufferCreation.DebugName.empty())
        {
            const_cast<BufferCreation&>(bufferCreation).DebugName = "GpuHeapLinearBufferAllocator";
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
            .SizeInBytes = bufferSizeInBytesPerFrame * GraphicsConfig::g_FrameInFlightCount,
        });

        for (uint32_t i = 0; i < GraphicsConfig::g_FrameInFlightCount; ++i)
        {
            const uint64_t gpuHeapOffsetInBytes = bufferSizeInBytesPerFrame * i;
            MakeUniquePtr(m_FrameBuffers[i], *m_GpuHeap, gpuHeapOffsetInBytes, BufferCreation
            {
                .DebugName = std::format("FrameConstBuffer_{}", i),
                .Type = BufferType::Byte,
                .ElementSizeInBytes = sizeof(std::byte),
                .ElementCount = bufferSizeInBytesPerFrame,
            });
        }
    }

    void ConstBufferLinearAllocator::ResetFrameBuffer()
    {
        m_FrameBuffer = m_FrameBuffers[m_Device.GetActiveFrameIndex()].get();
        m_FrameBufferWriter.ResetTargetBuffer(ByteBuffer{ m_FrameBuffer->GetCpuMappedData(), m_FrameBuffer->GetSizeInBytes() });
    }

    uint64_t ConstBufferLinearAllocator::Allocate(std::span<const std::byte> data)
    {
        const uint64_t offsetInBytes = m_FrameBufferWriter.GetPositionInBytes();
        BenzinAssert((offsetInBytes % GraphicsConfig::g_ConstBufferAlignmentInBytes) == 0);

        m_FrameBufferWriter.WriteData(data);
        m_FrameBufferWriter.SetPositionInBytes(AlignUp(m_FrameBufferWriter.GetPositionInBytes(), GraphicsConfig::g_ConstBufferAlignmentInBytes));

        return m_FrameBuffer->GetGpuVirtualAddress() + offsetInBytes;
    }

}
