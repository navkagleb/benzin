#pragma once

#include "benzin/graphics/resource.hpp"

namespace benzin
{

    enum class BufferType : uint8_t
    {
        Vertex,
        Index,

        Byte, // ByteAddress
        Format,
        Structured,
        Const,
        RayTracing_AccelerationStructure,
    };

    struct BufferCreation
    {
        std::string_view DebugName;

        ResourceMemoryType MemoryType = ResourceMemoryType::Default;
        BufferType Type = BufferType::Byte;
        GraphicsFormat Format = GraphicsFormat::Unknown; // Optional. Uses for BufferType::Format

        uint32_t ElementSizeInBytes = sizeof(std::byte);
        uint64_t ElementCount = 0;

        bool IsUnorderedAccessAllowed = false;
    };

    class Buffer : public Resource
    {
    public:
        using MapReadbackCallback = std::function<void(const std::byte* mappedData)>;

        explicit Buffer(Device& device, const BufferCreation& creation);
        ~Buffer() override;

    public:
        auto GetMemoryType() const { return m_MemoryType; }
        auto GetType() const { return m_Type; }
        auto GetFormat() const { return m_Format; }

        auto GetElementSizeInBytes() const { return m_ElementSizeInBytes; }
        auto GetAlignedElementSizeInBytes() const { return m_AlignedElementSizeInBytes; }
        auto GetElementCount() const { return m_ElementCount; }

        uint64_t GetNotAlignedSizeInBytes() const { return m_ElementSizeInBytes * m_ElementCount; }
        uint64_t GetSizeInBytes() const override { return m_AlignedElementSizeInBytes * m_ElementCount; }

        auto* GetCpuMappedData() const { return m_CpuMappedData; }

        uint64_t GetGpuVirtualAddress(uint32_t elementIndex = 0) const;

        const Descriptor& GetSrv(const SubRange64& elementRange = {}) const;
        const Descriptor& GetUav() const;
        const Descriptor& GetCbv(uint32_t elementIndex = 0) const;

        Descriptor CreateDetachedSrv(const SubRange64& elementRange = {}, bool isValidationEnabled = true) const;
        Descriptor CreateDetachedUav() const;
        Descriptor CreateDetachedCbv(uint32_t elementIndex) const;

        void MapReadbackData(uint64_t offsetInBytes, uint32_t dataSizeInBytes, const MapReadbackCallback& callback) const;

    private:
        ResourceMemoryType m_MemoryType = ResourceMemoryType::Default;
        BufferType m_Type = BufferType::Byte;
        GraphicsFormat m_Format = GraphicsFormat::Unknown;

        uint32_t m_ElementSizeInBytes = 0;
        uint32_t m_AlignedElementSizeInBytes = 0; // For ConstantBufferView
        uint64_t m_ElementCount = 0;

        bool m_IsUnorderedAccessAllowed = false;

        std::byte* m_CpuMappedData = nullptr;
    };

}
