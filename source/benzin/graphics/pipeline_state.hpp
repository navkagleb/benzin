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

        const uint64_t Hash = g_InvalidIndex<uint64_t>;

        ShaderCreation() = default; // To remove designated initialization

        static ShaderCreation CreateVertexShader(std::string_view fileName, std::string_view entryPoint);
        static ShaderCreation CreatePixelShader(std::string_view fileName, std::string_view entryPoint);
        static ShaderCreation CreateComputeShader(std::string_view fileName, std::string_view entryPoint);
        static ShaderCreation CreateLibrary(std::string_view fileName);

        bool IsValid() const { return !FileName.empty() && !EntryPoint.empty(); }
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

        bool Reload();

    private:
        void Create(const GraphicsPipelineStateCreation& creation, bool isShaderCacheIgnored);
        void Create(const ComputePipelineStateCreation& creation, bool isShaderCacheIgnored);

        bool IsAllShadersValid() const;

    private:
        Device& m_Device;

        ID3D12PipelineState* m_D3D12PipelineState = nullptr;

        PipelineStateCreationVariant m_CreationVariant;
    };

} // namespace benzin
