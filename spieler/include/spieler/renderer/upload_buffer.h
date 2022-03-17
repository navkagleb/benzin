#pragma once

#include "renderer_object.h"

namespace Spieler
{

    enum UploadBufferType : std::uint32_t
    {
        UploadBufferType_Default        = 0,
        UploadBufferType_ConstantBuffer = 1
    };

    class UploadBuffer : public RendererObject
    {
    public:
        UploadBuffer() = default;
        UploadBuffer(UploadBuffer&& other) noexcept;

    public:
        ~UploadBuffer();

    public:
        std::uint32_t GetElementCount() const { return m_ElementCount; }
        std::uint32_t GetStride() const { return m_Stride; }

    public:
        template <typename T>
        bool Init(UploadBufferType type, std::uint32_t elementCount);

        std::uint64_t GetGPUVirtualAddress(std::uint32_t index = 0) const { return m_Buffer->GetGPUVirtualAddress() + static_cast<std::uint64_t>(index * m_Stride); }

        template <typename T>
        T& As(std::uint32_t index = 0);

    public:
        UploadBuffer& operator =(UploadBuffer&& other) noexcept;

        explicit operator ID3D12Resource* () const { return m_Buffer.Get(); }

    private:
        ComPtr<ID3D12Resource>  m_Buffer;
        std::byte*              m_MappedData{ nullptr };

        std::uint32_t           m_ElementCount{ 0 };
        std::uint32_t           m_Stride{ 0 };
    };

    namespace _Internal
    {

        inline std::uint32_t CalcConstantBufferSize(std::uint32_t size)
        {
            return (size + 255) & ~255;
        }

    } // namespace _Internal

    template <typename T>
    bool UploadBuffer::Init(UploadBufferType type, std::uint32_t elementCount)
    {
        SPIELER_ASSERT(elementCount != 0);

        m_ElementCount  = elementCount;
        m_Stride        = type == UploadBufferType_ConstantBuffer ? _Internal::CalcConstantBufferSize(sizeof(T)) : sizeof(T);
        
        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment          = 0;
        resourceDesc.Width              = m_Stride * m_ElementCount;
        resourceDesc.Height             = 1;
        resourceDesc.DepthOrArraySize   = 1;
        resourceDesc.MipLevels          = 1;
        resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count   = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES uploadHeapProps{};
        uploadHeapProps.Type                    = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeapProps.CPUPageProperty         = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeapProps.MemoryPoolPreference    = D3D12_MEMORY_POOL_UNKNOWN;
        uploadHeapProps.CreationNodeMask        = 1;
        uploadHeapProps.VisibleNodeMask         = 1;

        SPIELER_RETURN_IF_FAILED(GetDevice()->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            __uuidof(ID3D12Resource),
            &m_Buffer
        ));
        
        SPIELER_RETURN_IF_FAILED(m_Buffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedData)));

        return true;
    }

    template <typename T>
    T& UploadBuffer::As(std::uint32_t index)
    {
        SPIELER_ASSERT(index < m_ElementCount);

        return *reinterpret_cast<T*>(&m_MappedData[index * m_Stride]);
    }

} // namespace Spieler