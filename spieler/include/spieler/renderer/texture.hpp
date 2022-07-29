#pragma once

#include "spieler/core/assert.hpp"

#include "spieler/renderer/resource.hpp"
#include "spieler/renderer/common.hpp"
#include "spieler/renderer/resource_view.hpp"

namespace spieler::renderer
{

    class Device;

    class Texture2DResource : public Resource
    {
    public:
        friend class Device;

    public:
        const Texture2DConfig& GetConfig() const { return m_Config; }

    public:
        std::vector<SubresourceData> LoadFromDDSFile(Device& device, const std::wstring& filename);

        void SetResource(ComPtr<ID3D12Resource>&& resource);

    private:
        Texture2DConfig m_Config;
        std::unique_ptr<uint8_t[]> m_Data; // #TODO: Move to another place. For now store it here because it need for SubResources
    };

    template <typename T>
    concept TextureResourceView = std::_Is_any_of_v<T, RenderTargetView, DepthStencilView, ShaderResourceView, UnorderedAccessView>;

    class Texture2D
    {
    private:
        using ResourceViewInternal = std::variant<RenderTargetView, DepthStencilView, ShaderResourceView, UnorderedAccessView>;
        using ViewContainer = std::unordered_map<uint64_t, ResourceViewInternal>;

    public:
        Texture2D() = default;

    public:
        // Resource
        Texture2DResource& GetTexture2DResource() { return m_Texture2DResource; }
        const Texture2DResource& GetTexture2DResource() const { return m_Texture2DResource; }

        // View
        template <TextureResourceView ResourceView> const ResourceView& GetView() const;
        template <TextureResourceView ResourceView> void SetView(Device& device);
        template <TextureResourceView ResourceView> bool IsViewExist() const;

        void Reset();

    private:
        template <TextureResourceView ResourceView>
        static uint64_t GetHashCode();

    private:
        Texture2DResource m_Texture2DResource;
        ViewContainer m_Views;
    };

    template <TextureResourceView ResourceView>
    const ResourceView& Texture2D::GetView() const
    {
        return std::get<ResourceView>(m_Views.at(GetHashCode<ResourceView>()));
    }

    template <TextureResourceView ResourceView>
    void Texture2D::SetView(Device& device)
    {
        SPIELER_ASSERT(m_Texture2DResource.GetResource());

        m_Views[GetHashCode<ResourceView>()] = ResourceView{ device, m_Texture2DResource };
    }

    template <TextureResourceView ResourceView>
    bool Texture2D::IsViewExist() const
    {
        return m_Views.contains(GetHashCode<ResourceView>());
    }

    template <TextureResourceView ResourceView>
    static uint64_t Texture2D::GetHashCode()
    {
        return static_cast<uint64_t>(typeid(ResourceView).hash_code());
    }

} // namespace spieler::renderer
