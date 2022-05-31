#pragma once

#include "spieler/math/vector.hpp"

#include "spieler/renderer/types.hpp"

namespace spieler::renderer
{

    class Device;
    class Context;

    enum class ResourceDimension : uint8_t
    {
        Unknown = D3D12_RESOURCE_DIMENSION_UNKNOWN,
        Buffer = D3D12_RESOURCE_DIMENSION_BUFFER,
        Texture1D = D3D12_RESOURCE_DIMENSION_TEXTURE1D,
        Texture2D = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        Texture3D = D3D12_RESOURCE_DIMENSION_TEXTURE3D
    };

    // Texture
    enum Texture2DFlags : uint8_t
    {
        Texture2DFlags_None = D3D12_RESOURCE_FLAG_NONE,
        Texture2DFlags_RenderTarget = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        Texture2DFlags_DepthStencil = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
        Texture2DFlags_UnorderedAccess = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
    };

    struct Texture2DConfig
    {
        uint64_t Width{ 0 };
        uint32_t Height{ 0 };
        GraphicsFormat Format{ GraphicsFormat::UNKNOWN };
        Texture2DFlags Flags{ Texture2DFlags_None };
    };

    struct TextureClearValue
    {
        math::Vector4f Color{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    struct DepthStencilClearValue
    {
        float Depth{ 0.0f };
        uint8_t Stencil{ 0 };
    };

    class Resource
    {
    public:
        friend class Device;

    public:
        Resource() = default;
        Resource(Resource&& other) noexcept;
        virtual ~Resource() = default;

    public:
        ComPtr<ID3D12Resource>& GetResource() { return m_Resource; }
        const ComPtr<ID3D12Resource>& GetResource() const { return m_Resource; }

    public:
        void SetDebugName(const std::wstring& name);
        void Reset();

    public:
        Resource& operator =(Resource&& other) noexcept;
        operator ID3D12Resource* () const { return m_Resource.Get(); }

    protected:
        ComPtr<ID3D12Resource> m_Resource;
    };

} // namespace spieler::renderer