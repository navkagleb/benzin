#pragma once

#include <benzin/graphics/resource.hpp>
#include <benzin/graphics/format.hpp>

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
        bool m_IsCubeMap = false;
        GraphicsFormat m_Format = GraphicsFormat::Unknown;
        uint32_t m_DepthOffset = 0;
        uint32_t m_DepthCount = g_MaxU32; // By default select all slices
        uint32_t m_MipOffset = 0;
        uint32_t m_MipCount = g_MaxU32; // By default select all mips
    };

    struct TextureUav
    {
        uint32_t m_MipIndex = 0;
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
        const Descriptor& GetRtv() const;
        const Descriptor& GetDsv() const;

    private:
        Descriptor CreateDetachedSrv(TextureSrv& textureSrv) const;
        Descriptor CreateDetachedUav(const TextureUav& textureUav) const;
        Descriptor CreateDetachedRtv() const;
        Descriptor CreateDetachedDsv() const;

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
