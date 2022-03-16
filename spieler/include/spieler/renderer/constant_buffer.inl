#include "constant_buffer.h"

#include "resource.h"
#include "descriptor_heap.h"

namespace Spieler
{

    namespace _Internal
    {

        inline static std::uint32_t CalcConstantBufferSize(std::uint32_t size)
        {
            return (size + 255) & ~255;
        }

    } // namespace _Internal

    template <typename T>
    bool ConstantBuffer::InitAsRootDescriptorTable(const DescriptorHeap& heap, std::uint32_t index)
    {
        return InitAsRootDescriptorTable(T{}, heap, index);
    }

    template <typename T>
    bool ConstantBuffer::InitAsRootDescriptorTable(const T& data, const DescriptorHeap& heap, std::uint32_t index)
    {
        SPIELER_RETURN_IF_FAILED(Init(data));

        m_Index = index;

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation  = m_UploadBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes     = _Internal::CalcConstantBufferSize(sizeof(T));

        GetDevice()->CreateConstantBufferView(&cbvDesc, heap.GetCPUHandle(index));

        return true;
    }

    template <typename T>
    bool ConstantBuffer::InitAsRootDescriptor()
    {
        return InitAsRootDescriptor(T{});
    }

    template <typename T>
    bool ConstantBuffer::InitAsRootDescriptor(const T& data)
    {
        return Init(data);
    }

    template <typename T>
    bool ConstantBuffer::Init(const T& data)
    {
        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment          = 0;
        resourceDesc.Width              = _Internal::CalcConstantBufferSize(sizeof(T));
        resourceDesc.Height             = 1;
        resourceDesc.DepthOrArraySize   = 1;
        resourceDesc.MipLevels          = 1;
        resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count   = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

        m_UploadBuffer = Resource::CreateUploadBuffer(resourceDesc);

        SPIELER_RETURN_IF_FAILED(m_UploadBuffer);
        SPIELER_RETURN_IF_FAILED(m_UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedData)));

        std::memcpy(m_MappedData, &data, sizeof(data));

        return true;
    }

} // namespace Spieler