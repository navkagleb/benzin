#pragma once

#include "benzin/graphics/resource.hpp"

namespace benzin
{

    enum class TextureType : std::underlying_type_t<D3D12_RESOURCE_DIMENSION>
    {
        Unknown = D3D12_RESOURCE_DIMENSION_UNKNOWN,
        Texture2D = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
    };

    enum class TextureFlag : uint8_t
    {
        AllowRenderTarget,
        AllowDepthStencil,
        AllowUnorderedAccess,
    };
    using TextureFlags = magic_enum::containers::bitset<TextureFlag>;
    static_assert(sizeof(TextureFlags) <= sizeof(TextureFlag));

    struct TextureCreation
    {
        std::string_view DebugName;

        TextureType Type = TextureType::Unknown;
        bool IsCubeMap = false;

        GraphicsFormat Format = GraphicsFormat::Unknown;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint16_t ArraySize = 1;
        uint16_t MipCount = 0; // 0 - all mip levels

        TextureFlags Flags{};

        ClearValueVariant ClearValue;

        ResourceState InitialState = ResourceState::Common;

        bool IsNeedShaderResourceView = false;
        bool IsNeedUnorderedAccessView = false;
        bool IsNeedRenderTargetView = false;
        bool IsNeedDepthStencilView = false;
    };

    struct TextureShaderResourceViewCreation
    {
        GraphicsFormat Format = GraphicsFormat::Unknown;
        bool IsCubeMap = false;
        uint32_t MostDetailedMipIndex = 0;
        uint32_t MipCount = 0xffffffff; // Default value to select all mips
        uint32_t FirstArrayIndex = 0;
        uint32_t ArraySize = 0;
    };

    struct TextureUnorderedAccessViewCreation
    {
        GraphicsFormat Format = GraphicsFormat::Unknown;
        uint32_t FirstArrayIndex = 0;
        uint32_t ArraySize = 0;
    };

    struct TextureRenderTargetViewCreation
    {
        GraphicsFormat Format = GraphicsFormat::Unknown;
        uint32_t FirstArrayIndex = 0;
        uint32_t ArraySize = 0;
    };

    class Texture : public Resource
    {
    public:
        friend class TextureLoader;

    public:
        Texture(Device& device, const TextureCreation& creation);
        Texture(Device& device, ID3D12Resource* d3d12Resource);

    public:
        auto GetType() const { return m_Type; }
        auto IsCubeMap() const { return m_IsCubeMap; }

        auto GetFormat() const { return m_Format; }
        auto GetWidth() const { return m_Width; }
        auto GetHeight() const { return m_Height; }
        auto GetArraySize() const { return m_ArraySize; }
        auto GetMipCount() const { return m_MipCount; }

        uint32_t GetSizeInBytes() const override;
        uint32_t GetSubResourceCount() const;

    public:
        bool HasRenderTargetView(uint32_t index = 0) const { return HasResourceView(DescriptorType::RenderTargetView, index); }
        bool HasDepthStencilView(uint32_t index = 0) const { return HasResourceView(DescriptorType::DepthStencilView, index); }

        const Descriptor& GetRenderTargetView(uint32_t index = 0) const { return GetResourceView(DescriptorType::RenderTargetView, index); }
        const Descriptor& GetDepthStencilView(uint32_t index = 0) const { return GetResourceView(DescriptorType::DepthStencilView, index); }

        uint32_t PushShaderResourceView(const TextureShaderResourceViewCreation& creation = {});
        uint32_t PushUnorderedAccessView(const TextureUnorderedAccessViewCreation& creation = {});
        uint32_t PushRenderTargetView(const TextureRenderTargetViewCreation& creation = {});
        uint32_t PushDepthStencilView();

    private:
        TextureType m_Type = TextureType::Unknown;
        bool m_IsCubeMap = false;

        GraphicsFormat m_Format = GraphicsFormat::Unknown;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint16_t m_ArraySize = 0;
        uint16_t m_MipCount = 0;
    };

} // namespace benzin
