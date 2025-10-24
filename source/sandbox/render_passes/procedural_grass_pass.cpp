#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/procedural_grass_pass.hpp>

#include <sandbox/render_passes/gbuffer.hpp>
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
        ms_PsoManager->Create(PsoId::ProceduralGrass, [](benzin::MeshPsoProxy& outProxy)
        {
            outProxy.As.FileName = "procedural_grass_pass.hlsl";
            outProxy.Ms.FileName = "procedural_grass_pass.hlsl";
            outProxy.Ps.FileName = "procedural_grass_pass.hlsl";

            outProxy.Ms.Defines.push_back("CALC_STATS");
            
            outProxy.RasterizerState.CullMode = benzin::CullMode::None;
            outProxy.RasterizerState.IsIndexOrderClockwise = true;

            outProxy.DepthState.IsEnabled = true;
            outProxy.DepthState.IsWriteEnabled = true;
            outProxy.DepthState.ComparisonFunction = benzin::ComparisonFunction::Greater;

            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color0Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color1Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color2Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color3Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color4Format);
            outProxy.DepthStencilFormat = GBufferSettings::s_DepthStencilFormat;
        });
    }

    ProceduralGrassPass::~ProceduralGrassPass()
    {
        ms_PsoManager->Destroy(PsoId::ProceduralGrass);

        ms_Resources->Destroy(BufferId::ProceduralGrass_GrassPatches);
    }

    void ProceduralGrassPass::OnZeroFrameInit()
    {
        {
            benzin::TextureImage perlinNoiseImage;
            benzin::LoadTextureImageFromDdsFile("perlin_noise_256.dds", perlinNoiseImage);

            benzin::MakeUniquePtr(m_PerlinNoiseTexture, *ms_Device, benzin::TextureCreation
            {
                .DebugName = "PerlinNoise256",
                .Format = perlinNoiseImage.m_Format,
                .Width = perlinNoiseImage.m_Width,
                .Height = perlinNoiseImage.m_Height,
                .MipCount = 1,
            });

            benzin::CopyCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(m_PerlinNoiseTexture->GetSizeInBytes());
            cmdList.UploadToTexture(*m_PerlinNoiseTexture, benzin::ToSpan(perlinNoiseImage.m_PixelData));
        }
        
        {
            if (ms_Scene->m_GrassPatches.empty())
                return;

            ms_Resources->Create(BufferId::ProceduralGrass_GrassPatches, benzin::BufferCreation
            {
                .DebugName = magic_enum::enum_name(BufferId::ProceduralGrass_GrassPatches),
                .HeapType = benzin::GpuHeapType::Default,
                .Type = benzin::BufferType::Structured,
                .ElementSizeInBytes = sizeof(joint::GrassPatch),
                .ElementCount = (uint32_t)ms_Scene->m_GrassPatches.size(),
            });

            benzin::Buffer& buffer = const_cast<benzin::Buffer&>(ms_Resources->Get(BufferId::ProceduralGrass_GrassPatches));
            benzin::CopyCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(buffer.GetSizeInBytes());
            cmdList.UploadToBuffer(buffer, benzin::ToSpan(ms_Scene->m_GrassPatches));
        }

        auto& stats = ms_Settings->GetSection<ProceduralGrassStats>();
        stats.MaxPatchCount = (uint32_t)ms_Scene->m_GrassPatches.size();
    }

    void ProceduralGrassPass::OnUpdate()
    {
        const auto& settings = ms_Settings->GetSection<ProceduralGrassSettings>();
        const auto& stats = ms_Settings->GetSection<ProceduralGrassStats>();

        RenderPass::m_IsRenderingEnabled = settings.IsEnabled;
        if (!RenderPass::m_IsRenderingEnabled)
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
        BenzinProfile();
        BenzinGpuProfile("ProceduralGrass");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);

        cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer0, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));
        cmdList.SetMeshPso(ms_PsoManager->GetMesh(PsoId::ProceduralGrass));

        const GBuffer gbuffer{ *ms_Resources };
        gbuffer.SetRenderTargets(cmdList);

        const benzin::ScopedResourceBarriers scopeGBufferBarriers = gbuffer.CreateResourceBarriers(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

        {
            using Resources = joint::ProceduralGrassResources;

            cmdList.SetGraphicsRootResource(*Resources::GrassPatches, ms_Resources->Get(BufferId::ProceduralGrass_GrassPatches).GetSrv());
            cmdList.SetGraphicsRootResource(*Resources::PerlinNoise, m_PerlinNoiseTexture->GetSrv());
        }

        cmdList.DispatchMesh({ m_Consts.GrassPatchCount, 1, 1 }, { *joint::ProceduralGrassConsts::AsGroupSize, 1, 1 });
    }

}
