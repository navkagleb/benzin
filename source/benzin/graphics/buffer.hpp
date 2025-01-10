#pragma once

#include "benzin/core/command_line_args.hpp"
#include "benzin/core/enum_flags.hpp"
#include "benzin/core/memory_writer.hpp"
#include "benzin/graphics/resource.hpp"

namespace benzin
{

    enum class BufferType : uint8_t
    {
        Byte, // ByteAddress
        Format,
        Structured,
        Constant,
        RtAccelerationStructure,
    };

    struct BufferCreation
    {
        std::string_view DebugName;

        ResourceMemoryType MemoryType = ResourceMemoryType::Default;
        BufferType Type = BufferType::Byte;
        GraphicsFormat Format = GraphicsFormat::Unknown; // Optional. Uses for BufferType::Format

        Bytes32 ElementSize = sizeof(std::byte);
        uint32_t ElementCount = 0;

        bool IsUnorderedAccessAllowed = false;
    };

    class Buffer : public Resource
    {
    public:
        friend class RtAccelerationStructure;

        template <typename ConstantsT>
        friend class ConstantBuffer;

        explicit Buffer(Device& device);
        Buffer(Device& device, const BufferCreation& creation);
        ~Buffer() override;

    public:
        auto GetMemoryType() const { return m_MemoryType; }
        auto GetType() const { return m_Type; }
        auto GetFormat() const { return m_Format; }

        auto GetElementSize() const { return m_ElementSize; }
        auto GetElementCount() const { return m_ElementCount; }
        auto GetAlignedElementSize() const { return m_AlignedElementSize; }

        Bytes32 GetNotAlignedSize() const { return m_ElementSize * m_ElementCount; }
        Bytes32 GetSize() const override { return m_AlignedElementSize * m_ElementCount; }

        auto* GetCpuMappedData() const { return m_CpuMappedData; }

        uint64_t GetGpuVirtualAddress(uint32_t elementIndex = 0) const;

        void Create(const BufferCreation& creation);

        const Descriptor& GetSrv(IndexRange32 elementRange = {}) const;
        const Descriptor& GetUav() const;
        const Descriptor& GetCbv(uint32_t elementIndex = 0) const;

        Descriptor CreateDetachedSrv(IndexRange32 elementRange = {}, bool isValidationEnabled = true) const;
        Descriptor CreateDetachedUav() const;
        Descriptor CreateDetachedCbv(uint32_t elementIndex) const;

    private:
        ResourceMemoryType m_MemoryType = ResourceMemoryType::Default;
        BufferType m_Type = BufferType::Byte;
        GraphicsFormat m_Format = GraphicsFormat::Unknown;

        Bytes32 m_ElementSize = 0;
        Bytes32 m_AlignedElementSize = 0; // For ConstantBufferView
        uint32_t m_ElementCount = 0;

        bool m_IsUnorderedAccessAllowed = false;

        std::byte* m_CpuMappedData = nullptr;
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
                .MemoryType = ResourceMemoryType::Upload,
                .Type = BufferType::Constant,
                .ElementSize = sizeof(ConstantsT),
                .ElementCount = CommandLineArgs::GetU32("FrameInFlightCount"),
            });

            m_MappedDataWriter = MemoryWriter{ m_Buffer.GetCpuMappedData(), m_Buffer.GetSize() };
        }

        auto GetActiveGpuVirtualAddress() const
        {
            return m_Buffer.GetGpuVirtualAddress(m_Buffer.m_Device.GetActiveFrameIndex());
        }

        const auto& GetActiveCbv() const
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
