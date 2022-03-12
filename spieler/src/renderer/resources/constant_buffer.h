#pragma once

#include <d3d12.h>

#include "resource.h"
#include "renderer/renderer_object.h"

namespace Spieler
{

    template <typename T>
    class ConstantBuffer : public RendererObject
    {
    public:
        ~ConstantBuffer();

    public:
        bool Init(const D3D12_CPU_DESCRIPTOR_HANDLE& handle);

        T& GetData() { return *m_MappedData; }

    public:
        ComPtr<ID3D12Resource>  m_UploadBuffer;
        T*                      m_MappedData = nullptr;
    };

} // namespace Spieler

#include "constant_buffer.inl"