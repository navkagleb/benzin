#pragma once

#include <d3d12.h>

#include "bindable.h"

namespace Spieler
{

    class PipelineState : public Bindable
    {
    public:
        bool Init(Renderer& renderer)
        {
            D3D12_BLEND_DESC blendDesc{};
            blendDesc.AlphaToCoverageEnable     = false;
            blendDesc.IndependentBlendEnable    = false;

            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
            desc.pRootSignature         = ;
            desc.VS                     = ;
            desc.PS                     = ;
            desc.DS                     = ;
            desc.HS                     = ;
            desc.GS                     = ;
            desc.StreamOutput           = ;
            desc.BlendState             = blendDesc;
            desc.SampleMask             = 0xffffffff;
            desc.RasterizerState        = ;
            desc.DepthStencilState      = ;
            desc.InputLayout            = ;
            desc.IBStripCutValue        = ;
            desc.PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            desc.NumRenderTargets       = 1;
            desc.RTVFormats[0]          = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.DSVFormat              = DXGI_FORMAT_D24_UNORM_S8_UINT;
            desc.SampleDesc.Count       = 1; // Use 4xMSAA
            desc.SampleDesc.Quality     = 0; // Use 4xMSAA
            desc.NodeMask               = ;
            desc.CachedPSO              = ;
            desc.Flags                  = ;

            SPIELER_RETURN_IF_FAILED(GetDevice(renderer)->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState), &m_PipelineState));

            return true;
        }

        void Bind(Renderer& renderer) override
        {

        }

    private:
        ComPtr<ID3D12PipelineState> m_PipelineState;
    };

} // namespace Spieler