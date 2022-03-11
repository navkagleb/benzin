#pragma once

#include <d3d12.h>

#include "renderer_object.h"
#include "descriptor_heap.h"

namespace Spieler
{

    class RootSignature : public RendererObject
    {
    public:
        ID3D12RootSignature* GetRaw()
        {
            return m_RootSignature.Get();
        }

    public:
        bool Init()
        {
            D3D12_DESCRIPTOR_RANGE cbvTable{};
            cbvTable.RangeType                          = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            cbvTable.NumDescriptors                     = 1;
            cbvTable.BaseShaderRegister                 = 0;
            cbvTable.RegisterSpace                      = 0;
            cbvTable.OffsetInDescriptorsFromTableStart  = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_ROOT_PARAMETER slotRootParameter[1]{};
            slotRootParameter[0].ParameterType                          = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            slotRootParameter[0].DescriptorTable.NumDescriptorRanges    = 1;
            slotRootParameter[0].DescriptorTable.pDescriptorRanges      = &cbvTable;
            slotRootParameter[0].ShaderVisibility                       = D3D12_SHADER_VISIBILITY_ALL;

            D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
            rootSignatureDesc.NumParameters     = 1;
            rootSignatureDesc.pParameters       = slotRootParameter;
            rootSignatureDesc.NumStaticSamplers = 0;
            rootSignatureDesc.pStaticSamplers   = nullptr;
            rootSignatureDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ComPtr<ID3DBlob> serializedRootSignature;
            ComPtr<ID3DBlob> error;

            SPIELER_RETURN_IF_FAILED(D3D12SerializeRootSignature(
                &rootSignatureDesc, 
                D3D_ROOT_SIGNATURE_VERSION_1, 
                &serializedRootSignature, 
                &error
            ));

            if (error)
            {
                ::OutputDebugString(reinterpret_cast<const char*>(error->GetBufferPointer()));
                return false;
            }

            SPIELER_RETURN_IF_FAILED(GetDevice()->CreateRootSignature(
                0,
                serializedRootSignature->GetBufferPointer(),
                serializedRootSignature->GetBufferSize(),
                __uuidof(ID3D12RootSignature),
                &m_RootSignature
            ));

            return true;
        }

        void Bind(const D3D12_GPU_DESCRIPTOR_HANDLE& handle)
        {
            GetCommandList()->SetGraphicsRootSignature(m_RootSignature.Get());
            GetCommandList()->SetGraphicsRootDescriptorTable(0, handle);
        }

    private:
        ComPtr<ID3D12RootSignature> m_RootSignature;
    };


} // namespace Spieler