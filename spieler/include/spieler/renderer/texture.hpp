#pragma once

#include <string>
#include <memory>
#include <vector>

#include "renderer_object.hpp"

namespace Spieler
{

    class DescriptorHeap;

    class Texture2D : public RendererObject
    {
    public:
        friend class ShaderResourceView;

    public:
        bool LoadFromDDSFile(const std::wstring& filename);

    private:
        ComPtr<ID3D12Resource> m_Resource;
        ComPtr<ID3D12Resource> m_UploadHeap;

        std::unique_ptr<std::uint8_t[]> m_Data;
        std::vector<D3D12_SUBRESOURCE_DATA> m_SubresourceData;
    };

    class ShaderResourceView : public RendererObject
    {
    public:
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