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
        if (ms_Scene->m_GrassPatchBuffer.get() == nullptr)
        {
            ms_Settings->GetSection<ProceduralGrassSettings>().m_IsEnabled = false;
            m_IsRenderingEnabled = false;
            return;
        }

        benzin::TextureImage perlinNoiseImage;
        BenzinEnsure(benzin::LoadTextureImageFromDdsFile("perlin_noise_256.dds", perlinNoiseImage));

        m_PerlinNoiseTexture = ms_Device->GetPersistentDefaultAllocator().AllocateTexture([&perlinNoiseImage](benzin::TextureCreation& creation)
        {
            creation.m_DebugName = "ProceduralGrass::PerlinNoise256";
            creation.m_DxgiFormat = perlinNoiseImage.m_DxgiFormat;
            creation.m_Width = perlinNoiseImage.m_Width;
            creation.m_Height = perlinNoiseImage.m_Height;
            creation.m_MipCount = 1;
        });

        benzin::CopyCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(m_PerlinNoiseTexture->GetSizeInBytes());
        cmdList.UploadToTexture(*m_PerlinNoiseTexture, benzin::ToSpan(perlinNoiseImage.m_PixelData));

        auto& stats = ms_Settings->GetSection<ProceduralGrassStats>();
        stats.m_MaxPatchCount = (uint32_t)ms_Scene->m_GrassPatchBuffer->GetElementCount();
    }

    void ProceduralGrassPass::OnUpdate()
    {
        const auto& settings = ms_Settings->GetSection<ProceduralGrassSettings>();
        const auto& stats = ms_Settings->GetSection<ProceduralGrassStats>();

        m_IsRenderingEnabled = settings.m_IsEnabled;
        if (!m_IsRenderingEnabled)
            return;

        m_Consts.m_GrassPatchCount = stats.m_MaxPatchCount;
        m_Consts.m_IsFrustumCullingEnabled = settings.m_IsFrustumCullingEnabled;
        m_Consts.m_GrassPatchCullRadius = settings.m_GrassPatchCullRadius;
        m_Consts.m_GrassEndDistance = settings.m_GrassEndDistance;
        m_Consts.m_SpacingInGrassPatch = settings.m_SpacingInGrassPatch;
        m_Consts.m_WindDirection = settings.m_WindDirection;
        m_Consts.m_BladeWidth = settings.m_BladeWidth;
        m_Consts.m_BaseColor = settings.m_BaseColor;
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

        cmdList.AddRenderTarget(gbuffer.m_AlbedoAndRoughness, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddRenderTarget(gbuffer.m_EmissiveAndMetallic, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddRenderTarget(gbuffer.m_WorldNormal, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddRenderTarget(gbuffer.m_Mv, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddRenderTarget(gbuffer.m_ViewDepth, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddDepthStencil(gbuffer.m_Depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        cmdList.SetRenderTargets();

        cmdList.SetGraphicsRootSrv(*Resources::GrassPatches, *ms_Scene->m_GrassPatchBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmdList.SetGraphicsRootSrv(*Resources::PerlinNoise, *m_PerlinNoiseTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmdList.FlushBarriers();

        cmdList.DispatchMesh({ m_Consts.m_GrassPatchCount, 1, 1 }, { *joint::ProceduralGrassConsts::AsGroupSize, 1, 1 });
    }

}
