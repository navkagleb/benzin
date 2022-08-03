#pragma once

#include "spieler/renderer/resource.hpp"
#include "spieler/renderer/common.hpp"
#include "spieler/renderer/resource_view.hpp"

namespace spieler::renderer
{

    class Device;

    class TextureResource final : public Resource
    {
    public:
        enum class Flags
        {
            None,
            RenderTarget,
            DepthStencil,
            UnorderedAccess,
        };

        enum class Dimension
        {
            Unknown,
            _1D,
            _2D,
            _3D
        };

        struct Config
        {
            Dimension Dimension{ Dimension::_2D };
            uint32_t Width{ 0 };
            uint32_t Height{ 0 };
            uint16_t Depth{ 1 };
            uint16_t MipCount{ 1 };
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
        TextureResource(Device& device, const Config& config);
        TextureResource(Device& device, const Config& config, const ClearColor& clearColor);
        TextureResource(Device& device, const Config& config, const ClearDepthStencil& clearDepthStencil);

        TextureResource(ComPtr<ID3D12Resource>&& dx12Texture);

    public:
        const Config& GetConfig() const { return m_Config; }

    public:
        std::vector<SubresourceData> LoadFromDDSFile(Device& device, const std::wstring& filepath);

    private:
        Config m_Config;
        std::unique_ptr<uint8_t[]> m_Data;
    };

    struct Texture
    {
        TextureResource Resource;
        ViewContainer Views{ Resource };
    };

} // namespace spieler::renderer
