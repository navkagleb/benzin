#pragma once

#include "benzin/graphics/resource.hpp"

namespace benzin
{

    enum class BufferFlag : uint8_t
    {
        Upload,
        ConstantBuffer,
        AllowUnorderedAccess,
    };
    using BufferFlags = magic_enum::containers::bitset<BufferFlag>;
    static_assert(sizeof(BufferFlags) <= sizeof(BufferFlag));

    struct BufferCreation
    {
        DebugName DebugName;

        uint32_t ElementSize = sizeof(std::byte);
        uint32_t ElementCount = 0;

        BufferFlags Flags{};

        ResourceState InitialState = ResourceState::Present;
        std::span<const std::byte> InitialData;

        bool IsNeedShaderResourceView = false;
        bool IsNeedUnorderedAccessView = false;
        bool IsNeedConstantBufferView = false;
    };

    class Buffer : public Resource
    {
    public:
        Buffer(Device& device, const BufferCreation& creation);

    public:
        uint32_t GetElementSize() const { return m_ElementSize; }
        uint32_t GetElementCount() const { return m_ElementCount; }
        uint32_t GetSizeInBytes() const override { return m_AlignedSizeInBytes; }

    public:
        bool HasConstantBufferView(uint32_t index = 0) const { return HasResourceView(DescriptorType::ConstantBufferView, index); }

        const Descriptor& GetConstantBufferView(uint32_t index = 0) const { return GetResourceView(DescriptorType::ConstantBufferView, index); }

        uint32_t PushShaderResourceView();
        uint32_t PushUnorderedAccessView();
        uint32_t PushConstantBufferView();

    private:
        uint32_t m_ElementSize = 0;
        uint32_t m_ElementCount = 0;
        uint32_t m_AlignedSizeInBytes = 0;
    };

} // namespace benzin
