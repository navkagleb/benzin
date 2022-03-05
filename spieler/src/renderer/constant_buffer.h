#pragma once

#include <d3d12.h>

#include "bindable.h"

namespace Spieler
{
    
    inline std::uint32_t CalcConstantBufferSize(std::uint32_t size)
    {
        return (size + 255) & ~255;
    }

    template <typename T>
    class ConstantBuffer : public Bindable
    {
    public:
        ~ConstantBuffer()
        {
            if (m_UploadBuffer)
            {
                m_UploadBuffer->Unmap(0, nullptr);
            }

            m_MappedData = nullptr;
        }

    public:
        bool Init()
        {
            const std::uint32_t size = CalcConstantBufferSize(sizeof(T));

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Alignment          = 0;
            desc.Width              = size;
            desc.Height             = 1;
            desc.DepthOrArraySize   = 1;
            desc.MipLevels          = 1;
            desc.Format             = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count   = 1;
            desc.SampleDesc.Quality = 0;
            desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            desc.Flags              = D3D12_RESOURCE_FLAG_NONE;

            D3D12_HEAP_PROPERTIES uploadHeapProps{};
            uploadHeapProps.Type                  = D3D12_HEAP_TYPE_UPLOAD;
            uploadHeapProps.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            uploadHeapProps.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;
            uploadHeapProps.CreationNodeMask      = 1;
            uploadHeapProps.VisibleNodeMask       = 1;

            SPIELER_CHECK_STATUS(GetDevice()->CreateCommittedResource(
                &uploadHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                __uuidof(ID3D12Resource),
                &m_UploadBuffer
            ));

            SPIELER_CHECK_STATUS(m_UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedData)));

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
            cbvDesc.BufferLocation  = m_UploadBuffer->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes     = size;
            
            GetDevice()->CreateConstantBufferView(&cbvDesc, GetMixtureDescriptorHeap().GetCPUHandle(0));

            return true;
        }

        void Update(const T& data)
        {
            std::memcpy(m_MappedData, &data, sizeof(T));
        }

        void Bind() override
        {

        }

    private:
        ComPtr<ID3D12Resource>  m_UploadBuffer;
        T*                      m_MappedData    = nullptr;
    };

} // namespace Spieler