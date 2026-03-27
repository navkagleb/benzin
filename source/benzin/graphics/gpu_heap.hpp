#pragma once

#include <benzin/core/buffer_writer.hpp>
#include <benzin/graphics/common.hpp>

namespace benzin
{

    class Device;
    class Texture;
    struct BufferCreation;
    struct TextureCreation;

    enum class GpuHeapType : uint8_t
    {
        Default, // Local VRAM
        Upload, // Host VRAM. CPU can write
        Readback, // Host VRAM. CPU can read
        GpuUpload, // Local VRAM. CPU can write
    };

    struct GpuHeapCreation
    {
        std::string m_DebugName;

        GpuHeapType m_Type = g_MaxEnum<GpuHeapType>;
        uint64_t m_SizeInBytes = 0;
    };

    class GpuHeap
    {
    public:
        friend class Buffer;
        friend class GpuHeapLinearAllocator;
        friend class Texture;

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

        GpuHeapType m_Type = g_MaxEnum<GpuHeapType>;
        uint64_t m_SizeInBytes = 0;
    };

    class GpuHeapLinearAllocator
    {
    public:
        using BufferConfigurator = std::move_only_function<void(BufferCreation& creation)>;
        using TextureConfigurator = std::move_only_function<void(TextureCreation& creation)>;

        GpuHeapLinearAllocator(GpuHeap& gpuHeap);

        const auto& GetGpuHeap() const { return m_GpuHeap; }
        auto GetOffsetInBytes() const { return m_OffsetInBytes; }

        void ResetOffset();

        template <typename T>
        std::unique_ptr<Buffer> AllocateBuffer(std::string_view debugName, std::span<const T> elements, DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN)
        {
            if (dxgiFormat != DXGI_FORMAT_UNKNOWN)
            {
                BenzinAssert(GetDxgiFormatSizeInBytes(dxgiFormat) == sizeof(T));
                return AllocateFormatBuffer(debugName, (uint32_t)elements.size(), dxgiFormat);
            }

            return AllocateStructuredBuffer(debugName, (uint32_t)elements.size(), sizeof(T));
        }

        template <typename T>
        std::unique_ptr<Buffer> AllocateAndWriteBuffer(std::string_view debugName, std::span<const T> elements, DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN)
        {
            auto buffer = AllocateBuffer(debugName, elements, dxgiFormat);

            BenzinAssert(buffer->GetCpuMappedData() != nullptr);

            BufferWriter writer = MakeBufferWriter(*buffer);
            writer.WriteArray(elements);

            return buffer;
        }

        std::unique_ptr<Buffer> AllocateBuffer(BufferConfigurator configurator);
        std::unique_ptr<Buffer> AllocateStructuredBuffer(std::string_view debugName, uint32_t elementCount, uint32_t elementSizeInBytes);
        std::unique_ptr<Buffer> AllocateFormatBuffer(std::string_view debugName, uint32_t elementCount, DXGI_FORMAT dxgiFormat);

        std::unique_ptr<Texture> AllocateTexture(TextureConfigurator configurator);

    private:
        template <typename ResourceT, typename CreationT>
        std::unique_ptr<ResourceT> Allocate(CreationT& creation);

        GpuHeap& m_GpuHeap;
        uint64_t m_OffsetInBytes = 0;
    };

    class ConstBufferLinearAllocator
    {
    public:
        ConstBufferLinearAllocator(Device& device, uint32_t sizeInBytesPerFrame);

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
        std::unique_ptr<Buffer> m_FrameBuffers[BENZIN_FRAME_COUNT];

        Buffer* m_FrameBuffer = nullptr;
        BufferWriter m_FrameBufferWriter;
    };

}
