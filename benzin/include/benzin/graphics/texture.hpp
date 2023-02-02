#pragma once

#include "benzin/graphics/resource.hpp"
#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    class TextureResource final : public Resource
    {
    public:
        friend class Device;
        friend class ResourceLoader;

    public:
        BENZIN_DEBUG_NAME_D3D12_OBJECT(m_D3D12Resource, "Texture")

    public:
        enum class Flags
        {
            None = 0,
            BindAsRenderTarget = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            BindAsDepthStencil = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            BindAsUnorderedAccess = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        };

        enum class Type
        {
            Unknown = D3D12_RESOURCE_DIMENSION_UNKNOWN,
            Texture2D = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        };

        struct Config
        {
            Type Type{ Type::Unknown };
            uint32_t Width{ 0 };
            uint32_t Height{ 0 };
            uint16_t ArraySize{ 1 };
            uint16_t MipCount{ 1 };
            bool IsCubeMap{ false };
            GraphicsFormat Format{ GraphicsFormat::Unknown };
            Flags Flags{ Flags::None };
        };

        struct ClearColor
        {
            DirectX::XMFLOAT4 Color{ 0.0f, 0.0f, 0.0f, 1.0f };
        };

        struct ClearDepthStencil
        {
            float Depth{ 0.0f };
            uint8_t Stencil{ 0 };
        };

    public:
        TextureResource() = default;

    private:
        TextureResource(ID3D12Resource* d3d12Resource, const Config& config, const std::string& debugName);

    public:
        const Config& GetConfig() const { return m_Config; }

    public:
        uint32_t PushRenderTargetView(const Descriptor& rtv);
        uint32_t PushDepthStencilView(const Descriptor& dsv);

        const Descriptor& GetRenderTargetView(uint32_t index = 0) const;
        const Descriptor& GetDepthStencilView(uint32_t index = 0) const;

    private:
        Config m_Config;
    };

} // namespace benzin
