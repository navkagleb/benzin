#pragma once

#include "benzin/graphics/api/resource.hpp"

namespace benzin
{

    struct TextureShaderResourceViewConfig
    {
        GraphicsFormat Format{ GraphicsFormat::Unknown };
        bool IsCubeMap{ false };
        uint32_t MostDetailedMipIndex{ 0 };
        uint32_t MipCount{ 0xffffffff }; // Default value to select all mips
        uint32_t FirstArrayIndex{ 0 }; // TODO: Rename TextureUnorderedAccessViewConfig
        uint32_t ArraySize{ 0 };
    };

    struct TextureUnorderedAccessViewConfig
    {
        GraphicsFormat Format{ GraphicsFormat::Unknown };
        uint32_t StartElementIndex{ 0 };
        uint32_t ElementCount{ 0 };
    };

    struct TextureRenderTargetViewConfig
    {
        GraphicsFormat Format{ GraphicsFormat::Unknown };
        uint32_t StartElementIndex{ 0 };
        uint32_t ElementCount{ 0 };
    };

    class TextureResource final : public Resource
    {
    public:
        friend class TextureLoader;

    public:
        struct ClearColor
        {
            DirectX::XMFLOAT4 Color{ 0.0f, 0.0f, 0.0f, 1.0f };
        };

        struct ClearDepthStencil
        {
            float Depth{ 1.0f };
            uint8_t Stencil{ 0 };
        };

        using ClearValue = std::variant<ClearColor, ClearDepthStencil>;

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
            uint16_t MipCount{ 0 }; // 0 - all mip levels
            bool IsCubeMap{ false };
            GraphicsFormat Format{ GraphicsFormat::Unknown };
            Flags Flags{ Flags::None };
        };

    public:
        TextureResource(Device& device, const Config& config, std::optional<ClearValue> optimizedClearValue = std::nullopt);
        TextureResource(Device& device, ID3D12Resource* d3d12Resource);

    public:
        const Config& GetConfig() const { return m_Config; }

        template <typename T>
        T GetOptimizedClearValue() const;

    public:
        bool HasRenderTargetView(uint32_t index = 0) const;
        bool HasDepthStencilView(uint32_t index = 0) const;

        const Descriptor& GetRenderTargetView(uint32_t index = 0) const;
        const Descriptor& GetDepthStencilView(uint32_t index = 0) const;

        uint32_t PushShaderResourceView(const TextureShaderResourceViewConfig& config = {});
        uint32_t PushUnorderedAccessView(const TextureUnorderedAccessViewConfig& config = {});
        uint32_t PushRenderTargetView(const TextureRenderTargetViewConfig& config = {});
        uint32_t PushDepthStencilView();

    private:
        Config m_Config;
        std::optional<ClearValue> m_OptimizedClearValue;
    };

    template <typename T>
    T TextureResource::GetOptimizedClearValue() const
    {
        if (!m_OptimizedClearValue)
        {
            return T{};
        }

        BENZIN_ASSERT(std::holds_alternative<T>(*m_OptimizedClearValue));
        return std::get<T>(*m_OptimizedClearValue);
    }

} // namespace benzin
