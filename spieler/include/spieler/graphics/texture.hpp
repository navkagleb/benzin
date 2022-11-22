#pragma once

#include "spieler/graphics/resource.hpp"

namespace spieler
{

    class Device;
    class GraphicsCommandList;

    class TextureResource final : public Resource
    {
    public:
        enum class UsageFlags
        {
            None,
            RenderTarget,
            DepthStencil,
            UnorderedAccess,
        };

        enum class Type
        {
            Unknown,
            //_1D,
            _2D,
            //_3D,
            Cube
        };

        struct Config
        {
            Type Type{ Type::_2D };
            uint32_t Width{ 0 };
            uint32_t Height{ 0 };
            uint16_t ArraySize{ 1 };
            uint16_t MipCount{ 1 };
            GraphicsFormat Format{ GraphicsFormat::Unknown };
            UsageFlags UsageFlags{ UsageFlags::None };
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
        static std::shared_ptr<TextureResource> Create(Device& device, const Config& config);
        static std::shared_ptr<TextureResource> Create(Device& device, const Config& config, const ClearColor& clearColor);
        static std::shared_ptr<TextureResource> Create(Device& device, const Config& config, const ClearDepthStencil& clearDepthStencil);
        static std::shared_ptr<TextureResource> Create(ID3D12Resource* dx12Resource);

        static std::shared_ptr<TextureResource> LoadFromDDSFile(Device& device, GraphicsCommandList& graphicsCommandList, const std::wstring& filename);

    public:
        TextureResource() = default;
        TextureResource(Device& device, const Config& config);
        TextureResource(Device& device, const Config& config, const ClearColor& clearColor);
        TextureResource(Device& device, const Config& config, const ClearDepthStencil& clearDepthStencil);
        TextureResource(ID3D12Resource* dx12Resource);

    public:
        const Config& GetConfig() const { return m_Config; }

    public:
        std::vector<SubresourceData> LoadFromDDSFile(Device& device, const std::wstring& filepath);

    private:
        Config m_Config;
        std::unique_ptr<uint8_t[]> m_Data;
    };

    struct TextureViewConfig
    {
        std::string Tag;
        uint32_t MostDetailedMip{ 0 };
        float MinLODClamp{ 0.0f };
        uint32_t MipCount{ 0 };
        uint32_t FirstArraySlice{ 0 };
        uint32_t ArraySize{ 0 };
        GraphicsFormat Format{ GraphicsFormat::Unknown };
    };

    class TextureShaderResourceView : public DescriptorView<SRVDescriptor>
    {
    public:
        struct Config
        {
            TextureResource::Type Type{ TextureResource::Type::Unknown };
            uint32_t MostDetailedMipIndex{ 0 };
            uint32_t MipCount{ 0xffffffff }; // 0xffffffff value selects all mips
            uint32_t FirstArrayElement{ 0 };
            uint32_t ArraySize{ 0 };
        };

    public:
        TextureShaderResourceView() = default;
        TextureShaderResourceView(Device& device, const TextureResource& resource);
        TextureShaderResourceView(Device& device, const TextureResource& resource, const Config& config);
    };

    class TextureUnorderedAccessView : public DescriptorView<UAVDescriptor>
    {
    public:
        TextureUnorderedAccessView() = default;
        TextureUnorderedAccessView(Device& device, const TextureResource& resource);
    };

    class TextureRenderTargetView : public DescriptorView<RTVDescriptor>
    {
    public:
        TextureRenderTargetView() = default;
        TextureRenderTargetView(Device& device, const TextureResource& resource);
        TextureRenderTargetView(Device& device, const TextureResource& resource, const TextureViewConfig& config);
    };

    class TextureDepthStencilView : public DescriptorView<DSVDescriptor>
    {
    public:
        TextureDepthStencilView() = default;
        TextureDepthStencilView(Device& device, const TextureResource& resource);
    };

    class Texture
    {
    public:
        //SPIELER_NON_COPYABLE(Texture)

    private:
        using TextureViewVariant = std::variant<TextureShaderResourceView, TextureUnorderedAccessView, TextureRenderTargetView, TextureDepthStencilView>;

    private:
        template <typename View>
        static uint64_t GetViewID();

    public:
        Texture();

    public:
        std::shared_ptr<TextureResource>& GetTextureResource() { return m_TextureResource; }
        const std::shared_ptr<TextureResource>& GetTextureResource() const { return m_TextureResource; }
        void SetTextureResource(std::shared_ptr<TextureResource>&& textureResource);

    public:
        template <typename View>
        bool HasView(uint32_t index = 0) const;

        template <typename View>
        const View& GetView(uint32_t index = 0) const;

        template <typename View>
        uint32_t PushView(Device& device);

        template <typename View, typename Config>
        uint32_t PushView(Device& device, const Config& config);

    private:
        std::shared_ptr<TextureResource> m_TextureResource;
        std::unordered_map<uint64_t, std::vector<TextureViewVariant>> m_Views;
    };

    template <typename View>
    uint64_t Texture::GetViewID()
    {
        return typeid(View).hash_code();
    }

    template <typename View>
    bool Texture::HasView(uint32_t index) const
    {
        return m_Views.contains(GetViewID<View>()) && index < m_Views.at(GetViewID<View>()).size();
    }

    template <typename View>
    const View& Texture::GetView(uint32_t index) const
    {   
        SPIELER_ASSERT(m_TextureResource);
        SPIELER_ASSERT(HasView<View>(index));

        return std::get<View>(m_Views.at(GetViewID<View>())[index]);
    }

    template <typename View>
    uint32_t Texture::PushView(Device& device)
    {
        SPIELER_ASSERT(m_TextureResource);
        //SPIELER_ASSERT(!HasView<View, Index>());
        
        m_Views[GetViewID<View>()].push_back(View{ device, *m_TextureResource });

        return static_cast<uint32_t>(m_Views[GetViewID<View>()].size()) - 1;
    }

    template <typename View, typename Config>
    uint32_t Texture::PushView(Device& device, const Config& config)
    {
        SPIELER_ASSERT(m_TextureResource);
        //SPIELER_ASSERT(!HasView<View, Index>());

        m_Views[GetViewID<View>()].push_back(View{ device, *m_TextureResource, config });

        return static_cast<uint32_t>(m_Views[GetViewID<View>()].size()) - 1;
    }

} // namespace spieler
