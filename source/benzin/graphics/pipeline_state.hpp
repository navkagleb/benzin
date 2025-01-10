#pragma once

#include "benzin/graphics/render_states.hpp"

namespace benzin
{

    class Device;

    class ShaderInfo
    {
    public:
        ShaderInfo() = default;
        ShaderInfo(ShaderType type, std::string_view fileName, std::string_view entryPoint, std::vector<std::string_view>&& defines);

        auto GetType() const { return m_Type; }
        auto GetFileName() const { return m_FileName; }
        auto GetEntryPoint() const { return m_EntryPoint; }
        const auto& GetDefines() const { return m_Defines; }

        auto GetHash() const { return m_Hash; }

        bool IsValid() const
        {
            if (m_Type == ShaderType::Library)
            {
                return !m_FileName.empty();
            }

            return !m_FileName.empty() && !m_EntryPoint.empty();
        }

    private:
        ShaderType m_Type = g_InvalidEnum<ShaderType>;
        std::string_view m_FileName;
        std::string_view m_EntryPoint;
        std::vector<std::string_view> m_Defines;

        uint64_t m_Hash = g_InvalidUnsigned<uint64_t>;
    };

    struct GraphicsPipelineStateCreation
    {
        std::string_view DebugName;

        std::string_view VsFileName;
        std::string_view VsEntryPoint;
        std::vector<std::string_view> VsDefines;

        std::string_view PsFileName;
        std::string_view PsEntryPoint;
        std::vector<std::string_view> PsDefines;

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

        std::string_view CsFileName;
        std::string_view CsEntryPoint;
        std::vector<std::string_view> CsDefines;
    };

    struct RayTracingPipelineStateCreation
    {
        std::string_view DebugName;

        struct
        {
            std::string_view FileName;
            std::vector<std::string_view> Defines;
        } ShaderLibrary;

        struct
        {
            std::string_view Name;
            std::string_view ClosestHitEntryPoint;
        } HitGroup;

        struct
        {
            Bytes32 PayloadSize;
            Bytes32 AttributeSize;
        } ShaderConfig;
    };

    using PipelineStateCreationVariant = std::variant<
        GraphicsPipelineStateCreation,
        ComputePipelineStateCreation,
        RayTracingPipelineStateCreation
    >;

    class PipelineState
    {
    public:
        PipelineState(Device& device, const PipelineStateCreationVariant& creation);
        ~PipelineState();

        BenzinDefineNonCopyable(PipelineState);
        BenzinDefineNonMoveable(PipelineState);

    public:
        ID3D12PipelineState* GetD3D12PipelineState() const;
        ID3D12StateObject* GetD3D12StateObject() const;

        bool IsRayTracing() const;

        std::span<const ShaderInfo> GetShaders() const { return { m_Shaders.data(), m_ShaderCount }; }

        bool Reload();

    private:
        void StoreShaders(const GraphicsPipelineStateCreation& creation);
        void StoreShaders(const ComputePipelineStateCreation& creation);
        void StoreShaders(const RayTracingPipelineStateCreation& creation);

        void Compile(const GraphicsPipelineStateCreation& creation, bool isShaderCacheIgnored);
        void Compile(const ComputePipelineStateCreation& creation, bool isShaderCacheIgnored);
        void Compile(const RayTracingPipelineStateCreation& creation, bool isShaderCacheIgnored);

        bool IsAllShadersValid() const;

        void Reset();

    private:
        Device& m_Device;

        union
        {
            ID3D12PipelineState* m_D3D12PipelineState = nullptr;
            ID3D12StateObject* m_D3D12StateObject;
        };

        PipelineStateCreationVariant m_CreationVariant;

        std::array<ShaderInfo, 2> m_Shaders;
        uint32_t m_ShaderCount = 0;
    };

} // namespace benzin
