#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/procedural_grass_pass.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>
#include <benzin/utility/random.hpp>

#include <sandbox/render_passes/gbuffer.hpp>
#include <sandbox/resources.hpp>
#include <sandbox/sandbox_render_settings.hpp>

BenzinEnableUnaryPlusForEnum(joint::ProceduralGrassResources);

namespace sandbox
{

    ProceduralGrassPass::ProceduralGrassPass()
    {
        ms_PsoManager->Create(PsoId::ProceduralGrass, [](benzin::MeshPsoProxy& outProxy)
        {
            outProxy.Ms.FileName = "procedural_grass_pass.hlsl";
            outProxy.Ps.FileName = "procedural_grass_pass.hlsl";
            
            outProxy.RasterizerState.CullMode = benzin::CullMode::None;
            outProxy.RasterizerState.IndexOrder = benzin::IndexOrder::Clockwise;

            outProxy.DepthState.IsEnabled = true;
            outProxy.DepthState.IsWriteEnabled = true;
            outProxy.DepthState.ComparisonFunction = benzin::ComparisonFunction::Less;

            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color0Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color1Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color2Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color3Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color4Format);
            outProxy.DepthStencilFormat = GBufferSettings::s_DepthStencilFormat;
        });

        ms_ConstBufferPool->PreAllocate(sizeof(m_Consts));

        auto& settings = ms_Settings->GetSection<ProceduralGrassSettings>();
        settings.Consts.BaseColor = { 0.243f, 0.525f, 0.235f };
        settings.Consts.GrassEndDistance = 20.0f;
        settings.Consts.WindDirection = DirectX::XM_PI;
        settings.Consts.SpacingInPatch = 0.04f;
        settings.Consts.BladeWidth = 0.01f;
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
                .Format = perlinNoiseImage.Format,
                .Width = perlinNoiseImage.Width,
                .Height = perlinNoiseImage.Height,
                .MipCount = 1,
            });

            auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(m_PerlinNoiseTexture->GetSize());
            cmdList.UploadToTexture(*m_PerlinNoiseTexture, benzin::ToSpan(perlinNoiseImage.PixelData));
        }
        
        {
            const auto grassPatchView = ms_Scene->GetEntityRegistry().view<joint::GrassPatch>();
            if (grassPatchView.empty())
            {
                return;
            }

            ms_Resources->Create(BufferId::ProceduralGrass_GrassPatches, benzin::BufferCreation
            {
                .DebugName = magic_enum::enum_name(BufferId::ProceduralGrass_GrassPatches),
                .Type = benzin::BufferType::Structured,
                .ElementSize = sizeof(joint::GrassPatch),
                .ElementCount = (uint32_t)grassPatchView.size(),
            });

            auto& grassPatchBuffer = const_cast<benzin::Buffer&>(ms_Resources->Get(BufferId::ProceduralGrass_GrassPatches));

            auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(grassPatchBuffer.GetSize());
            for (const auto [i, entityHandle] : grassPatchView | std::views::enumerate)
            {
                // TODO: Maybe there is opportunity to use raw pointer as array to upload to GPU?

                const auto patchData = benzin::ToSingleByteSpan(grassPatchView.get<joint::GrassPatch>(entityHandle));
                const auto patchOffsetInBytes = (uint32_t)patchData.size_bytes() * i;
            
                cmdList.UploadToBuffer(grassPatchBuffer, patchData, patchOffsetInBytes);
            }

            auto& stats = ms_Settings->GetSection<ProceduralGrassStats>();
            stats.PatchCount = grassPatchBuffer.GetElementCount();
        }
    }

    void ProceduralGrassPass::OnUpdate()
    {
        const auto& settings = ms_Settings->GetSection<ProceduralGrassSettings>();

        benzin::RenderPass::m_IsRenderingEnabled = settings.IsEnabled;
        m_Consts = settings.Consts;
    }

    void ProceduralGrassPass::OnRender() const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "ProceduralGrass");

        cmdList.SetViewport(ms_RenderViewport);
        cmdList.SetScissorRect(ms_RenderScissorRect);

        cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer0, ms_ConstBufferPool->Allocate(m_Consts));
        cmdList.SetMeshPso(ms_PsoManager->GetMesh(PsoId::ProceduralGrass));

        const GBuffer gbuffer{ *ms_Resources };
        gbuffer.SetRenderTargets(cmdList);

        const benzin::ScopedResourceBarriers scopeGBufferBarriers = gbuffer.CreateResourceBarriers(cmdList, benzin::ResourceState::DepthWrite);

        const auto& grassPatchBuffer = ms_Resources->Get(BufferId::ProceduralGrass_GrassPatches);

        {
            using enum joint::ProceduralGrassResources;

            cmdList.SetGraphicsRootResource(+GrassPatches, grassPatchBuffer.GetSrv());
            cmdList.SetGraphicsRootResource(+PerlinNoise, m_PerlinNoiseTexture->GetSrv());
        }

        cmdList.DispatchMesh({ grassPatchBuffer.GetElementCount(), 1, 1});
    }

}
