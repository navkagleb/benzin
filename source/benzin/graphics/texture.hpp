#pragma once

#include "benzin/core/enum_flags.hpp"
#include "benzin/graphics/resource.hpp"

namespace benzin
{

    enum class TextureFlag : uint8_t
    {
        AllowRenderTarget,
        AllowDepthStencil,
        AllowUnorderedAccess,
    };
    BenzinEnableFlagsForEnum(TextureFlag);

    struct TextureCreation
    {
        std::string_view DebugName;

        bool IsCubeMap = false;
        GraphicsFormat Format = GraphicsFormat::Unknown;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint16_t Depth = 1; // ArraySize
        uint16_t MipCount = 0; // By default select all mip levels

        TextureFlags Flags;

        ResourceState InitialState = ResourceState::Common;
    };

    struct TextureSrv
    {
        GraphicsFormat Format = GraphicsFormat::Unknown;
        bool IsCubeMap = false;
        uint32_t MostDetailedMipIndex = 0;
        uint32_t MipCount = 0xffffffff; // By default select all mips
        IndexRangeU16 DepthRange;
    };

    struct TextureUav
    {
        GraphicsFormat Format = GraphicsFormat::Unknown;
        IndexRangeU16 DepthRange;
    };

    struct TextureRtv
    {
        GraphicsFormat Format = GraphicsFormat::Unknown;
        IndexRangeU32 DepthRange;
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

        uint32_t GetSizeInBytes() const override;
        uint32_t GetSubResourceCount() const;

        const Descriptor& GetSrv(const TextureSrv& textureSrv = {}) const;
        const Descriptor& GetUav(const TextureUav& textureUav = {}) const;
        const Descriptor& GetRtv(const TextureRtv& textureRtv = {}) const;
        const Descriptor& GetDsv() const;

    private:
        bool m_IsCubeMap = false;
        GraphicsFormat m_Format = GraphicsFormat::Unknown;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint16_t m_Depth = 0;
        uint16_t m_MipCount = 0;
    };

} // namespace benzin
