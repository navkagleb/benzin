#pragma once

#include <string>
#include <memory>
#include <vector>

#include "renderer_object.hpp"

namespace Spieler
{

    class DescriptorHeap;
    class UploadBuffer;

    class Texture2D : public RendererObject
    {
    public:
        friend class ShaderResourceView;

    public:
        bool LoadFromDDSFile(const std::wstring& filename, UploadBuffer& uploadBuffer);

        void SetDebugName(const std::wstring& name) { m_Texture->SetName(name.c_str()); }

    private:
        ComPtr<ID3D12Resource> m_Texture;
        std::unique_ptr<std::uint8_t[]> m_Data;
        std::vector<D3D12_SUBRESOURCE_DATA> m_SubresourceData;
    };

    class ShaderResourceView : public RendererObject
    {
    public:
        ShaderResourceView() = default;
        ShaderResourceView(const Texture2D& texture2D, const DescriptorHeap& descriptorHeap, std::uint32_t index);

    public:
        void Init(const Texture2D& texture2D, const DescriptorHeap& descriptorHeap, std::uint32_t index);

    public:
        void Bind(std::uint32_t rootParameterIndex) const;

    private:
        const DescriptorHeap* m_DescriptorHeap{ nullptr };
        std::uint32_t m_Index{ 0 };
    };

} // namespace Spieler