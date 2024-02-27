#pragma once

#include "benzin/core/command_line_args.hpp"
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
    using BufferFlags = magic_enum::containers::bitset<BufferFlag>;
    static_assert(sizeof(BufferFlags) <= sizeof(BufferFlag));

    struct BufferCreation
    {
        std::string_view DebugName;

        GraphicsFormat Format = GraphicsFormat::Unknown;
        uint32_t ElementSize = sizeof(std::byte);
        uint32_t ElementCount = 0;

        BufferFlags Flags{};

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
        explicit Buffer(Device& device);
        Buffer(Device& device, const BufferCreation& creation);

    public:
        auto GetElementSize() const { return m_ElementSize; }
        auto GetElementCount() const { return m_ElementCount; }
        auto GetAlignedElementSize() const { return m_AlignedElementSize; }
        auto GetNotAlignedSizeInBytes() const { return m_ElementSize * m_ElementCount; }

        uint32_t GetSizeInBytes() const override { return m_AlignedElementSize * m_ElementCount; }

        uint64_t GetGPUVirtualAddress() const;
        uint64_t GetGPUVirtualAddress(uint32_t elementIndex) const;

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
            .Flags{ BufferFlag::UploadBuffer }, // #TODO: Add StructuredBuffer flag
        });

        for (auto i : std::views::iota(0u, frameInFlightCount))
        {
            BenzinAssert(buffer->PushStructureBufferView(StructuredBufferViewCreation{ .FirstElementIndex = i * elementCount, .ElementCount = elementCount }) == i);
        }

        return buffer;
    }

    template <typename T>
    std::unique_ptr<Buffer> CreateFrameDependentConstantBuffer(Device& device, std::string_view debugName)
    {
        const uint32_t frameInFlightCount = CommandLineArgs::GetFrameInFlightCount();

        auto buffer = std::make_unique<Buffer>(device, BufferCreation
        {
            .DebugName = debugName,
            .ElementSize = sizeof(T),
            .ElementCount = frameInFlightCount,
            .Flags = BufferFlag::ConstantBuffer,
        });

        for (auto i : std::views::iota(0u, frameInFlightCount))
        {
            BenzinAssert(buffer->PushConstantBufferView(ConstantBufferViewCreation{ .ElementIndex = i }) == i);
        }

        return buffer;
    }

} // namespace benzin
