#pragma once

#include "benzin/graphics/render_states.hpp"

namespace benzin
{

    class Device;

    struct ShaderCreation
    {
        ShaderType Type;
        std::string_view FileName;
        std::string_view EntryPoint;

        bool IsValid() const
        {
            return !FileName.empty() && !EntryPoint.empty();
        }
    };

    struct GraphicsPipelineStateCreation
    {
        std::string_view DebugName;

        std::array<ShaderCreation, 2> Shaders;

        PrimitiveTopologyType PrimitiveTopologyType = PrimitiveTopologyType::Unknown;
        RasterizerState RasterizerState;

        DepthState DepthState;
        StencilState StencilState;

        std::vector<GraphicsFormat> RenderTargetFormats;
        GraphicsFormat DepthStencilFormat = GraphicsFormat::Unknown;
        BlendState BlendState;
    };

    struct ComputePipelineStateCreation
    {
        std::string_view DebugName;

        ShaderCreation Shader;
    };

    using PipelineStateCreationVariant = std::variant<GraphicsPipelineStateCreation, ComputePipelineStateCreation>;

    class PipelineState
    {
    public:
        PipelineState(Device& device, const PipelineStateCreationVariant& creation);
        ~PipelineState();

        BenzinDefineNonCopyable(PipelineState);
        BenzinDefineNonMoveable(PipelineState);

    public:
        ID3D12PipelineState* GetD3D12PipelineState() const { return m_D3D12PipelineState; }

        std::span<const ShaderCreation> GetShaders() const;

        void Reload();

    private:
        void Create(const GraphicsPipelineStateCreation& creation);
        void Create(const ComputePipelineStateCreation& creation);

    private:
        Device& m_Device;

        ID3D12PipelineState* m_D3D12PipelineState = nullptr;

        std::variant<GraphicsPipelineStateCreation, ComputePipelineStateCreation> m_Creation;
    };

} // namespace benzin
