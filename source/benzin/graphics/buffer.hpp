#pragma once

#include "benzin/core/asserter.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/core/enum_flags.hpp"
#include "benzin/core/memory_writer.hpp"
#include "benzin/graphics/resource.hpp"

namespace benzin
{

    enum class BufferFlag : uint8_t
    {
        UploadBuffer,
        ConstantBuffer,
        ReadbackBuffer,
        StructuredBuffer,
        AllowUnorderedAccess,
    };
    BenzinDefineFlagsForEnum(BufferFlag);

    struct BufferCreation
    {
        std::string_view DebugName;

        GraphicsFormat Format = GraphicsFormat::Unknown;
        uint32_t ElementSize = sizeof(std::byte);
        uint32_t ElementCount = 0;

        BufferFlags Flags;

        ResourceState InitialState = ResourceState::Common;
        std::span<const std::byte> InitialData;

        bool IsNeedFormatBufferView = false;
        bool IsNeedStructuredBufferView = false;
        bool IsNeedByteAddressBufferView = false;
        bool IsNeedRaytracingAccelerationStructureView = false;
        bool IsNeedUnorderedAccessView = false;
        bool IsNeedConstantBufferView = false;
    };

    struct FormatBufferViewCreation
    {
        GraphicsFormat Format = GraphicsFormat::Unknown;
    };

    struct StructuredBufferViewCreation
    {
        uint32_t FirstElementIndex = 0;
        uint32_t ElementCount = 0;
    };

    struct ConstantBufferViewCreation
    {
        uint32_t ElementIndex = g_InvalidIndex<uint32_t>;
    };

    class Buffer : public Resource
    {
    public:
        friend class RtAccelerationStructure;

        template <typename ConstantsT>
        friend class ConstantBuffer;

    public:
        explicit Buffer(Device& device);
        Buffer(Device& device, const BufferCreation& creation);
        ~Buffer() override;

    public:
        auto GetElementSize() const { return m_ElementSize; }
        auto GetElementCount() const { return m_ElementCount; }
        auto GetAlignedElementSize() const { return m_AlignedElementSize; }
        auto GetNotAlignedSizeInBytes() const { return m_ElementSize * m_ElementCount; }

        uint32_t GetSizeInBytes() const override { return m_AlignedElementSize * m_ElementCount; }

        uint64_t GetGpuVirtualAddress() const;
        uint64_t GetGpuVirtualAddress(uint32_t elementIndex) const;

        auto* GetMappedData() const { return m_MappedData; }

    public:
        void Create(const BufferCreation& creation);

        bool HasConstantBufferView(uint32_t index = 0) const { return HasResourceView(DescriptorType::ConstantBufferView, index); }

        const Descriptor& GetConstantBufferView(uint32_t index = 0) const { return GetResourceView(DescriptorType::ConstantBufferView, index); }

        uint32_t PushFormatBufferView(const FormatBufferViewCreation& creation);
        uint32_t PushStructureBufferView(const StructuredBufferViewCreation& creation = {});
        uint32_t PushByteAddressBufferView();
        uint32_t PushRaytracingAccelerationStructureView();
        uint32_t PushUnorderedAccessView();
        uint32_t PushConstantBufferView(const ConstantBufferViewCreation& creation = {});

    private:
        uint32_t PushShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC& d3d12ShaderResourceViewDesc, ID3D12Resource* d3d12Resource);

    private:
        uint32_t m_ElementSize = 0;
        uint32_t m_AlignedElementSize = 0; // For ConstantBufferView
        uint32_t m_ElementCount = 0;

        std::byte* m_MappedData = nullptr;
    };

    template <typename T>
    std::unique_ptr<Buffer> CreateFrameDependentUploadStructuredBuffer(Device& device, std::string_view debugName, uint32_t elementCount = 1)
    {
        const uint32_t frameInFlightCount = CommandLineArgs::GetFrameInFlightCount();

        auto buffer = std::make_unique<Buffer>(device, BufferCreation
        {
            .DebugName = debugName,
            .ElementSize = sizeof(T),
            .ElementCount = elementCount * frameInFlightCount,
            .Flags = BufferFlag::UploadBuffer, // #TODO: Add StructuredBuffer flag
        });

        for (auto i : std::views::iota(0u, frameInFlightCount))
        {
            BenzinAssert(buffer->PushStructureBufferView(StructuredBufferViewCreation{ .FirstElementIndex = i * elementCount, .ElementCount = elementCount }) == i);
        }

        return buffer;
    }

    template <typename ConstantsT>
    class ConstantBuffer
    {
    public:
        ConstantBuffer(Device& device, std::string_view debugName)
            : m_Buffer{ device }
        {
            m_Buffer.Create(BufferCreation
            {
                .DebugName = debugName,
                .ElementSize = sizeof(ConstantsT),
                .ElementCount = CommandLineArgs::GetFrameInFlightCount(),
                .Flags = BufferFlag::ConstantBuffer,
            });

            for (auto i : std::views::iota(0u, m_Buffer.GetElementCount()))
            {
                BenzinAssert(m_Buffer.PushConstantBufferView(ConstantBufferViewCreation{ .ElementIndex = i }) == i);
            }

            m_MappedDataWriter = MemoryWriter{ m_Buffer.GetMappedData(), m_Buffer.GetSizeInBytes() };
        }

        const Descriptor& GetActiveCbv() const
        {
            return m_Buffer.GetConstantBufferView(m_Buffer.m_Device.GetActiveFrameIndex());
        }

        void UpdateConstants(const ConstantsT& constants)
        {
            m_MappedDataWriter.WriteSized(constants, m_Buffer.GetAlignedElementSize(), m_Buffer.m_Device.GetActiveFrameIndex());
        }

    private:
        Buffer m_Buffer;
        MemoryWriter m_MappedDataWriter;
    };

} // namespace benzin
