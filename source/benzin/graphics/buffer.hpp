#pragma once

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
    BenzinEnableFlagsForEnum(BufferFlag);

    struct BufferCreation
    {
        std::string_view DebugName;

        GraphicsFormat Format = GraphicsFormat::Unknown;
        uint32_t ElementSize = sizeof(std::byte);
        uint32_t ElementCount = 0;

        BufferFlags Flags;

        ResourceState InitialState = ResourceState::Common;
        std::span<const std::byte> InitialData;
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

        auto* GetMappedData() const { return m_MappedData; }

        uint64_t GetGpuVirtualAddress(uint32_t elementIndex = 0) const;

        void Create(const BufferCreation& creation);

        const Descriptor& GetFormatSrv(GraphicsFormat format = GraphicsFormat::Unknown) const;
        const Descriptor& GetStructuredSrv(IndexRangeU32 elementRange = {}) const;
        const Descriptor& GetByteAddressSrv() const;
        const Descriptor& GetRtAsSrv() const;
        const Descriptor& GetUav() const;
        const Descriptor& GetCbv(uint32_t elementIndex = 0) const;

    private:
        Descriptor CreateSrv(const D3D12_SHADER_RESOURCE_VIEW_DESC& d3d12SrvDesc, ID3D12Resource* d3d12Resource) const;

    private:
        uint32_t m_ElementSize = 0;
        uint32_t m_AlignedElementSize = 0; // For ConstantBufferView
        uint32_t m_ElementCount = 0;

        std::byte* m_MappedData = nullptr;
    };

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

            m_MappedDataWriter = MemoryWriter{ m_Buffer.GetMappedData(), m_Buffer.GetSizeInBytes() };
        }

        const Descriptor& GetActiveCbv() const
        {
            return m_Buffer.GetCbv(m_Buffer.m_Device.GetActiveFrameIndex());
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
