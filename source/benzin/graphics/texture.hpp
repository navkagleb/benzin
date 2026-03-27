#pragma once

#include <benzin/graphics/resource.hpp>

namespace benzin
{

    class GpuHeap;

    struct DepthStencilValue
    {
        float m_Depth = 0.0f;
        uint8_t m_Stencil = 0;
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

        std::string m_DebugName;

        bool m_IsCubeMap = false;
        DXGI_FORMAT m_DxgiFormat = DXGI_FORMAT_UNKNOWN;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint32_t m_Depth = 1; // ArraySize
        uint32_t m_MipCount = 0; // By default select all mip levels

        EnumFlags<TextureAccessFlag> m_AccessFlags;
        ClearValueVariant m_ClearValueVariant;
    };

    struct TextureSrv
    {
        bool m_IsCubeMap = false;
        DXGI_FORMAT m_DxgiFormat = DXGI_FORMAT_UNKNOWN;
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
        Texture(GpuHeap& gpuHeap, uint64_t gpuHeapOffsetInBytes, const TextureCreation& creation);
        Texture(Device& device, ID3D12Resource* d3d12Resource);

        auto IsCubeMap() const { return m_IsCubeMap; }
        auto GetDxgiFormat() const { return m_DxgiFormat; }
        auto GetWidth() const { return m_Width; }
        auto GetHeight() const { return m_Height; }
        auto GetDepth() const { return m_Depth; }
        auto GetMipCount() const { return m_MipCount; }
        auto GetAccessFlags() const { return m_AccessFlags; }

        const DirectX::XMFLOAT4& GetClearColor() const;
        DepthStencilValue GetClearDepthStencil() const;

        uint64_t GetSizeInBytes() const override;
        uint32_t GetSubResourceCount() const;

        uint32_t GetMipWidth(uint32_t mipIndex) const;
        uint32_t GetMipHeight(uint32_t mipIndex) const;
        uint32_t CalcSubResourceIndex(uint32_t mipIndex, uint32_t depthIndex) const;

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

        bool m_IsCubeMap = false;
        DXGI_FORMAT m_DxgiFormat = DXGI_FORMAT_UNKNOWN;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint32_t m_Depth = 0;
        uint32_t m_MipCount = 0;

        EnumFlags<TextureAccessFlag> m_AccessFlags;
        ClearValueVariant m_ClearValueVariant;
    };

    uint64_t CalcTextureSizeInBytes(uint32_t width, uint32_t height, uint32_t depth, DXGI_FORMAT dxgiFormat);
    uint32_t CalcTextureMipCount(uint32_t width, uint32_t height);

}
