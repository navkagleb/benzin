#pragma once

#include <d3d12.h>

#include "bindable.h"

namespace Spieler
{

    enum FillMode : std::uint32_t
    {
        FillMode_Wireframe  = D3D12_FILL_MODE_WIREFRAME,
        FillMode_Solid      = D3D12_FILL_MODE_SOLID,
    };

    enum CullMode : std::uint32_t
    {
        CullMode_None   = D3D12_CULL_MODE_BACK,
        CullMode_Front  = D3D12_CULL_MODE_BACK,
        CullMode_Back   = D3D12_CULL_MODE_BACK
    };

    class RasterizerState : public Bindable
    {
    public:
        bool Init(Renderer& renderer, FillMode fillMode, CullMode cullMode)
        {
            D3D12_RASTERIZER_DESC desc{};
            desc.FillMode               = static_cast<D3D12_FILL_MODE>(fillMode);
            desc.CullMode               = static_cast<D3D12_CULL_MODE>(cullMode);
            desc.FrontCounterClockwise  = false;
            desc.DepthBias              = D3D12_DEFAULT_DEPTH_BIAS;
            desc.DepthBiasClamp         = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            desc.SlopeScaledDepthBias   = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            desc.DepthClipEnable        = true;
            desc.MultisampleEnable      = false;
            desc.AntialiasedLineEnable  = false;
            desc.ForcedSampleCount      = 0;
            desc.ConservativeRaster     = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        }

        void Bind(Renderer& renderer) override
        {

        }

    private:
        
    };

} // namespace Spieler