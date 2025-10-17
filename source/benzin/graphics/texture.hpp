#pragma once

#include "benzin/graphics/resource.hpp"
#include "benzin/graphics/format.hpp"

namespace benzin
{

    struct DepthStencilValue
    {
        float Depth = 0.0f;
        uint8_t Stencil = 0;
    };

    using ClearValueVariant = std::variant<std::monostate, DirectX::XMFLOAT4, DepthStencilValue>;

    enum class TextureAccessFlag : uint8_t
    {
        AllowRenderTarget,
        AllowDepthStencil,
        AllowUnorderedAccess,
    };
    BenzinEnableFlagsForEnum(TextureAccessFlag);

    struct TextureCreation
    {
        // For now only 2D textures supported

        std::string_view DebugName;

        bool IsCubeMap = false;
        GraphicsFormat Format = GraphicsFormat::Unknown;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint16_t Depth = 1; // ArraySize
        uint16_t MipCount = 0; // By default select all mip levels

        EnumFlags<TextureAccessFlag> AccessFlags;
        ClearValueVariant ClearValueVariant;
    };

    struct TextureSrv
    {
        bool IsCubeMap = false;
        GraphicsFormat Format = GraphicsFormat::Unknown;
        SubRange16 DepthRange; // By default select all slices
        SubRange16 MipRange; // By default select all mips
    };

    struct TextureUav
    {
        GraphicsFormat Format = GraphicsFormat::Unknown;
        uint16_t MipIndex = 0;
        SubRange16 DepthRange;
    };

    struct TextureRtv
    {
        GraphicsFormat Format = GraphicsFormat::Unknown;
        SubRange16 DepthRange;
    };

    class Texture : public Resource
    {
    public:
        Texture(Device& device, const TextureCreation& creation);
        Texture(Device& device, ID3D12Resource* d3d12Resource);

    public:
        auto IsCubeMap() const { return m_IsCubeMap; }
        auto GetFormat() const { return m_Format; }
        auto GetWidth() const { return m_Width; }
        auto GetHeight() const { return m_Height; }
        auto GetDepth() const { return m_Depth; }
        auto GetMipCount() const { return m_MipCount; }
        auto GetAccessFlags() const { return m_AccessFlags; }

        const DirectX::XMFLOAT4& GetClearColor() const;
        DepthStencilValue GetClearDepthStencil() const;

        uint64_t GetSizeInBytes() const override;
        uint32_t GetSubResourceCount() const;

        uint32_t GetMipWidth(uint16_t mipIndex) const;
        uint32_t GetMipHeight(uint16_t mipIndex) const;
        uint32_t CalcSubResourceIndex(uint16_t mipIndex, uint16_t depthIndex) const;

        const Descriptor& GetSrv(const TextureSrv& textureSrv = {}) const;
        const Descriptor& GetUav(const TextureUav& textureUav = {}) const;
        const Descriptor& GetRtv(const TextureRtv& textureRtv = {}) const;
        const Descriptor& GetDsv() const;

        Descriptor CreateDetachedSrv(const TextureSrv& textureSrv = {}, bool isValidationEnabled = true) const;
        Descriptor CreateDetachedUav(const TextureUav& textureUav = {}, bool isValidationEnabled = true) const;
        Descriptor CreateDetachedRtv(const TextureRtv& textureRtv = {}, bool isValidationEnabled = true) const;
        Descriptor CreateDetachedDsv(bool isValidationEnabled = true) const;

    private:
        void SetupCreation(const TextureCreation* creation = nullptr);

    private:
        bool m_IsCubeMap = false;
        GraphicsFormat m_Format = GraphicsFormat::Unknown;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint16_t m_Depth = 0;
        uint16_t m_MipCount = 0;

        EnumFlags<TextureAccessFlag> m_AccessFlags;
        ClearValueVariant m_ClearValueVariant;
    };

}
