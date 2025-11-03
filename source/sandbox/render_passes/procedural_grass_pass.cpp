#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/procedural_grass_pass.hpp>

#include <sandbox/render_passes/geometry_pass.hpp>
#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

BenzinAllowDereferenceOperatorForEnum(joint::ProceduralGrassResources);
BenzinAllowDereferenceOperatorForEnum(joint::ProceduralGrassConsts);

namespace sandbox
{

    ProceduralGrassPass::ProceduralGrassPass()
    {
        ms_PsoManager->Create(PsoId::ProceduralGrass, [](benzin::MeshPsoProxy& proxy)
        {
            proxy.m_As.m_FileName = "procedural_grass_pass.hlsl";
            proxy.m_Ms.m_FileName = "procedural_grass_pass.hlsl";
            proxy.m_Ps.m_FileName = "procedural_grass_pass.hlsl";

            proxy.m_Ms.m_Defines.push_back("CALC_STATS");
            
            proxy.m_RasterizerState.m_D3D12CullMode = D3D12_CULL_MODE_NONE;

            proxy.m_DepthState.m_IsEnabled = true;
            proxy.m_DepthState.m_IsWriteEnabled = true;
            proxy.m_DepthState.m_D3D12ComparisonFunction = D3D12_COMPARISON_FUNC_GREATER;

            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color0DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color1DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color2DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color3DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color4DxgiFormat);
            proxy.m_DepthStencilDxgiFormat = GBufferSettings::ms_DepthStencilDxgiFormat;
        });
    }

    ProceduralGrassPass::~ProceduralGrassPass()
    {
        ms_PsoManager->Destroy(PsoId::ProceduralGrass);
    }

    void ProceduralGrassPass::OnZeroFrameInit()
    {
        {
            benzin::TextureImage perlinNoiseImage;
            benzin::LoadTextureImageFromDdsFile("perlin_noise_256.dds", perlinNoiseImage);

            benzin::MakeUniquePtr(m_PerlinNoiseTexture, *ms_Device, benzin::TextureCreation
            {
                .m_DebugName = "PerlinNoise256",
                .m_DxgiFormat = perlinNoiseImage.m_DxgiFormat,
                .m_Width = perlinNoiseImage.m_Width,
                .m_Height = perlinNoiseImage.m_Height,
                .m_MipCount = 1,
            });

            benzin::CopyCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(m_PerlinNoiseTexture->GetSizeInBytes());
            cmdList.UploadToTexture(*m_PerlinNoiseTexture, benzin::ToSpan(perlinNoiseImage.m_PixelData));
        }
        
        {
            if (ms_Scene->m_GrassPatches.empty())
                return;

            m_GrassPatchBuffer = ms_Device->GetPersistentDefaultAllocator().AllocateBuffer(
                "ProceduralGrass::GrassPatches",
                benzin::ToSpan(ms_Scene->m_GrassPatches));

            benzin::CopyCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(m_GrassPatchBuffer->GetSizeInBytes());
            cmdList.UploadToBuffer(*m_GrassPatchBuffer, benzin::ToSpan(ms_Scene->m_GrassPatches));
        }

        auto& stats = ms_Settings->GetSection<ProceduralGrassStats>();
        stats.MaxPatchCount = (uint32_t)ms_Scene->m_GrassPatches.size();
    }

    void ProceduralGrassPass::OnUpdate()
    {
        const auto& settings = ms_Settings->GetSection<ProceduralGrassSettings>();
        const auto& stats = ms_Settings->GetSection<ProceduralGrassStats>();

        m_IsRenderingEnabled = settings.IsEnabled;
        if (!m_IsRenderingEnabled)
            return;

        m_Consts.GrassPatchCount = stats.MaxPatchCount;
        m_Consts.IsFrustumCullingEnabled = settings.IsFrustumCullingEnabled;
        m_Consts.GrassPatchCullRadius = settings.GrassPatchCullRadius;
        m_Consts.GrassEndDistance = settings.GrassEndDistance;
        m_Consts.SpacingInGrassPatch = settings.SpacingInGrassPatch;
        m_Consts.WindDirection = settings.WindDirection;
        m_Consts.BladeWidth = settings.BladeWidth;
        m_Consts.BaseColor = settings.BaseColor;
    }

    void ProceduralGrassPass::OnRender() const
    {
        using Resources = joint::ProceduralGrassResources;

        BenzinProfile();
        BenzinGpuProfile("ProceduralGrass");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);

        cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::RenderPassConsts, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));
        cmdList.SetMeshPso(ms_PsoManager->GetMesh(PsoId::ProceduralGrass));

        const GBuffer gbuffer{ *ms_Resources };

        cmdList.AddTransition(*m_GrassPatchBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList.AddTransition(*m_PerlinNoiseTexture, D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList.AddTransition(gbuffer.m_AlbedoAndRoughness, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddTransition(gbuffer.m_EmissiveAndMetallic, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddTransition(gbuffer.m_WorldNormal, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddTransition(gbuffer.m_Mv, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddTransition(gbuffer.m_ViewDepth, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddTransition(gbuffer.m_DepthStencil, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

        cmdList.AddRenderTarget(gbuffer.m_AlbedoAndRoughness);
        cmdList.AddRenderTarget(gbuffer.m_EmissiveAndMetallic);
        cmdList.AddRenderTarget(gbuffer.m_WorldNormal);
        cmdList.AddRenderTarget(gbuffer.m_Mv);
        cmdList.AddRenderTarget(gbuffer.m_ViewDepth);
        cmdList.AddDepthStencil(gbuffer.m_DepthStencil);
        cmdList.SetRenderTargets();

        cmdList.SetGraphicsRootSrv(*Resources::GrassPatches, *m_GrassPatchBuffer);
        cmdList.SetGraphicsRootSrv(*Resources::PerlinNoise, *m_PerlinNoiseTexture);

        cmdList.DispatchMesh({ m_Consts.GrassPatchCount, 1, 1 }, { *joint::ProceduralGrassConsts::AsGroupSize, 1, 1 });
    }

}
