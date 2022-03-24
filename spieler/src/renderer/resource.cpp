#include "renderer/resource.hpp"

namespace Spieler
{

    ComPtr<ID3D12Resource> Resource::CreateUploadBuffer(const D3D12_RESOURCE_DESC& resourceDesc)
    {
        D3D12_HEAP_PROPERTIES uploadHeapProps{};
        uploadHeapProps.Type                    = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeapProps.CPUPageProperty         = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeapProps.MemoryPoolPreference    = D3D12_MEMORY_POOL_UNKNOWN;
        uploadHeapProps.CreationNodeMask        = 1;
        uploadHeapProps.VisibleNodeMask         = 1;

        ComPtr<ID3D12Resource> uploadBuffer;

        const HRESULT result = GetDevice()->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            __uuidof(ID3D12Resource),
            &uploadBuffer
        );

        return result != S_OK ? nullptr : uploadBuffer;
    }

    ComPtr<ID3D12Resource> Resource::CreateDefaultBuffer(const D3D12_RESOURCE_DESC& resourceDesc)
    {
        D3D12_HEAP_PROPERTIES defaultHeapProps{};
        defaultHeapProps.Type                  = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeapProps.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeapProps.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeapProps.CreationNodeMask      = 1;
        defaultHeapProps.VisibleNodeMask       = 1;

        ComPtr<ID3D12Resource> buffer;

        const HRESULT result = GetDevice()->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            __uuidof(ID3D12Resource),
            &buffer
        );

        return result != S_OK ? nullptr : buffer;
    }

    void Resource::SetName(const std::wstring& name)
    {
        SPIELER_ASSERT(m_Resource);

        m_Resource->SetName(name.c_str());
    }

} // namespace Spieler