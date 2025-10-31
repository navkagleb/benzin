#pragma once

#include <benzin/graphics/resource.hpp>

namespace benzin
{

    class GpuHeap;

    enum class GpuHeapType : uint8_t;

    enum class BufferType : uint8_t
    {
        Byte, // ByteAddress
        Format,
        Structured,
        Const,
        RayTracing_AccelerationStructure,
    };

    struct BufferCreation
    {
        std::string_view m_DebugName;

        GpuHeapType m_HeapType = g_MaxEnum<GpuHeapType>; // For committed resource
        BufferType m_Type = BufferType::Byte;
        DXGI_FORMAT m_DxgiFormat = DXGI_FORMAT_UNKNOWN; // Uses for BufferType::Format

        uint32_t m_ElementSizeInBytes = sizeof(std::byte);
        uint64_t m_ElementCount = 0;

        bool m_IsUnorderedAccessAllowed = false;
    };

    class Buffer : public Resource
    {
    public:
        using MapReadbackCallback = std::function<void(const std::byte* mappedData)>;

        Buffer(Device& device, const BufferCreation& creation);
        Buffer(GpuHeap& gpuHeap, uint64_t gpuHeapOffsetInBytes, const BufferCreation& creation);
        ~Buffer() override;

    public:
        auto GetHeapType() const { return m_HeapType; }
        auto GetType() const { return m_Type; }
        auto GetDxgiFormat() const { return m_DxgiFormat; }
        auto GetElementSizeInBytes() const { return m_ElementSizeInBytes; }
        auto GetElementCount() const { return m_ElementCount; }

        auto* GetCpuMappedData() const { return m_CpuMappedData; }

        uint64_t GetSizeInBytes() const override;
        uint64_t GetGpuVirtualAddress(uint32_t elementIndex = 0) const;

        const Descriptor& GetSrv() const;
        const Descriptor& GetUav() const;

        Descriptor CreateDetachedSrv() const;
        Descriptor CreateDetachedUav() const;

        void MapReadbackData(uint64_t offsetInBytes, uint64_t dataSizeInBytes, const MapReadbackCallback& callback) const;

    private:
        void SetupCreation(const BufferCreation& creation, const GpuHeap* gpuHeap = nullptr);

    private:
        GpuHeapType m_HeapType = g_MaxEnum<GpuHeapType>;
        BufferType m_Type = BufferType::Byte;
        DXGI_FORMAT m_DxgiFormat = DXGI_FORMAT_UNKNOWN;
        uint32_t m_ElementSizeInBytes = 0;
        uint64_t m_ElementCount = 0;
        bool m_IsUnorderedAccessAllowed = false;

        std::byte* m_CpuMappedData = nullptr;
    };

}
