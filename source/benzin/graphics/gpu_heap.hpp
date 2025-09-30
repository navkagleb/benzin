#pragma once

#include <benzin/core/buffer_writer.hpp>
#include <benzin/graphics/format.hpp>

namespace benzin
{

    class Device;

    struct BufferCreation;

    enum class GpuHeapType : uint8_t
    {
        Default, // Local VRAM
        Upload, // Host VRAM. CPU can write
        Readback, // Host VRAM. CPU can read
        GpuUpload, // Local VRAM. CPU can write
    };

    struct GpuHeapCreation
    {
        std::string_view DebugName;

        GpuHeapType Type = g_BadEnum<GpuHeapType>;
        uint64_t SizeInBytes = 0;
    };

    class GpuHeap
    {
    public:
        friend class Buffer;

        GpuHeap(Device& device, const GpuHeapCreation& creation);
        ~GpuHeap();

        BenzinDefineNonCopyable(GpuHeap);
        BenzinDefineNonMoveable(GpuHeap);

        auto* GetD3D12Heap() const { return m_D3D12Heap; }

        auto GetType() const { return m_Type; }
        auto GetSizeInBytes() const { return m_SizeInBytes; }

    private:
        Device& m_Device;

        ID3D12Heap* m_D3D12Heap = nullptr;

        GpuHeapType m_Type = g_BadEnum<GpuHeapType>;
        uint64_t m_SizeInBytes = 0;
    };

    class GpuHeapLinearBufferAllocator
    {
    public:
        using BufferConfigurator = std::function<void(BufferCreation& creation)>;

        GpuHeapLinearBufferAllocator(GpuHeap& gpuHeap);

        auto GetSizeInBytes() const { return m_GpuHeap.GetSizeInBytes(); }
        auto GetOffsetInBytes() const { return m_OffsetInBytes; }

        template <typename T>
        std::unique_ptr<Buffer> AllocateBuffer(std::string_view debugName, std::span<const T> elements, GraphicsFormat format = GraphicsFormat::Unknown)
        {
            if (format != benzin::GraphicsFormat::Unknown)
            {
                BenzinAssert(GetFormatSizeInBytes(format) == sizeof(T));
                return AllocateFormatBuffer(debugName, (uint32_t)elements.size(), format);
            }

            return AllocateStructuredBuffer(debugName, (uint32_t)elements.size(), sizeof(T));
        }

        template <typename T>
        std::unique_ptr<Buffer> AllocateAndWriteBuffer(std::string_view debugName, std::span<const T> elements, GraphicsFormat format = GraphicsFormat::Unknown)
        {
            auto buffer = AllocateBuffer(debugName, elements, format);

            BenzinAssert(buffer->GetCpuMappedData() != nullptr);

            BufferWriter writer{ buffer->GetCpuMappedData(), buffer->GetSizeInBytes() };
            writer.WriteArray(elements);

            return buffer;
        }

        std::unique_ptr<Buffer> AllocateBuffer(const BufferConfigurator& configurator);
        std::unique_ptr<Buffer> AllocateStructuredBuffer(std::string_view debugName, uint32_t elementCount, uint32_t elementSizeInBytes);
        std::unique_ptr<Buffer> AllocateFormatBuffer(std::string_view debugName, uint32_t elementCount, GraphicsFormat format);

        void Reset();

    private:
        std::unique_ptr<Buffer> AllocateBuffer(const BufferCreation& bufferCreation);

    private:
        GpuHeap& m_GpuHeap;

        uint64_t m_OffsetInBytes = 0;
    };

    class ConstBufferLinearAllocator
    {
    public:
        ConstBufferLinearAllocator(Device& device);

        void ResetFrameBuffer();

        uint64_t Allocate(std::span<const std::byte> data);

        template <typename T>
        uint64_t Allocate(const T& data)
        {
            return Allocate(ToSingleByteSpan(data));
        }

    private:
        Device& m_Device;

        std::unique_ptr<GpuHeap> m_GpuHeap;
        std::unique_ptr<Buffer> m_FrameBuffers[GraphicsConfig::g_FrameInFlightCount];

        Buffer* m_FrameBuffer = nullptr;
        BufferWriter m_FrameBufferWriter;
    };

}
