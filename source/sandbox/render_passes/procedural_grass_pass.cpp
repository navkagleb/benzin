#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/procedural_grass_pass.hpp>

#include <benzin/core/cmd_line_args.hpp>
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
BenzinEnableUnaryPlusForEnum(joint::ProceduralGrassStat);
BenzinEnableUnaryPlusForEnum(joint::ProceduralGrassConsts);

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
            outProxy.DepthState.ComparisonFunction = benzin::ComparisonFunction::Less;

            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color0Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color1Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color2Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color3Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color4Format);
            outProxy.DepthStencilFormat = GBufferSettings::s_DepthStencilFormat;
        });

        const auto statFormat = benzin::GraphicsFormat::R32Uint;
        const uint32_t statElementSizeInBytes = benzin::GetFormatSizeInBytes(statFormat);
        const uint32_t statElementCount = (uint32_t)magic_enum::enum_count<joint::ProceduralGrassStat>();

        ms_Resources->Create(BufferId::ProceduralGrass_UavStats, benzin::BufferCreation
        {
            .DebugName = magic_enum::enum_name(BufferId::ProceduralGrass_UavStats),
            .Type = benzin::BufferType::Format,
            .Format = statFormat,
            .ElementSizeInBytes = statElementSizeInBytes,
            .ElementCount = statElementCount,
            .IsUnorderedAccessAllowed = true,
        });

        ms_Resources->Create(BufferId::ProceduralGrass_ReadbackStats, benzin::BufferCreation
        {
            .DebugName = magic_enum::enum_name(BufferId::ProceduralGrass_ReadbackStats),
            .MemoryType = benzin::ResourceMemoryType::Readback,
            .Type = benzin::BufferType::Format,
            .Format = statFormat,
            .ElementSizeInBytes = statElementSizeInBytes,
            .ElementCount = statElementCount * benzin::CmdLineArgs::GetReadbackLatency(),
        });

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        cmdList.AddResourceBarrier(benzin::TransitionBarrier{ ms_Resources->Get(BufferId::ProceduralGrass_ReadbackStats), benzin::ResourceState::Common });

        ms_ConstBufferPool->PreAllocate(sizeof(m_Consts));
    }

    ProceduralGrassPass::~ProceduralGrassPass()
    {
        ms_PsoManager->Destroy(PsoId::ProceduralGrass);

        ms_Resources->Destroy(BufferId::ProceduralGrass_GrassPatches);
        ms_Resources->Destroy(BufferId::ProceduralGrass_UavStats);
        ms_Resources->Destroy(BufferId::ProceduralGrass_ReadbackStats);
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

            auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(m_PerlinNoiseTexture->GetSizeInBytes());
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
                .ElementSizeInBytes = sizeof(joint::GrassPatch),
                .ElementCount = (uint32_t)grassPatchView.size(),
            });

            auto& grassPatchBuffer = const_cast<benzin::Buffer&>(ms_Resources->Get(BufferId::ProceduralGrass_GrassPatches));

            auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(grassPatchBuffer.GetSizeInBytes());
            for (const auto [i, entityHandle] : grassPatchView | std::views::enumerate)
            {
                // TODO: Maybe there is opportunity to use raw pointer as array to upload to GPU?

                const auto patchData = benzin::ToSingleByteSpan(grassPatchView.get<joint::GrassPatch>(entityHandle));
                const auto patchOffsetInBytes = (uint32_t)patchData.size_bytes() * i;
            
                cmdList.UploadToBuffer(grassPatchBuffer, patchData, patchOffsetInBytes);
            }

            auto& stats = ms_Settings->GetSection<ProceduralGrassStats>();
            stats.MaxPatchCount = (uint32_t)grassPatchBuffer.GetElementCount();
        }
    }

    void ProceduralGrassPass::OnUpdate()
    {
        const auto& settings = ms_Settings->GetSection<ProceduralGrassSettings>();
        const auto& stats = ms_Settings->GetSection<ProceduralGrassStats>();

        RenderPass::m_IsRenderingEnabled = settings.IsEnabled;

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

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "ProceduralGrass");

        RenderBlades(cmdList);
        CopyStats(cmdList);
    }

    void ProceduralGrassPass::RenderBlades(benzin::GraphicsCmdList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "RenderBlades");

        cmdList.SetViewport(ms_RenderViewport);
        cmdList.SetScissorRect(ms_RenderScissorRect);

        cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer0, ms_ConstBufferPool->Allocate(m_Consts));
        cmdList.SetMeshPso(ms_PsoManager->GetMesh(PsoId::ProceduralGrass));

        const GBuffer gbuffer{ *ms_Resources };
        gbuffer.SetRenderTargets(cmdList);

        const benzin::ScopedResourceBarriers scopeGBufferBarriers = gbuffer.CreateResourceBarriers(cmdList, benzin::ResourceState::DepthWrite);

        const auto& statsBuffer = ms_Resources->Get(BufferId::ProceduralGrass_UavStats);
        BenzinScopedResourceBarriers(cmdList, benzin::TransitionBarrier{ statsBuffer, benzin::ResourceState::UnorderedAccess });

        cmdList.ClearUnorderedAccess(statsBuffer, statsBuffer.GetUav(), {});

        {
            using enum joint::ProceduralGrassResources;

            cmdList.SetGraphicsRootResource(+GrassPatches, ms_Resources->Get(BufferId::ProceduralGrass_GrassPatches).GetSrv());
            cmdList.SetGraphicsRootResource(+PerlinNoise, m_PerlinNoiseTexture->GetSrv());
            cmdList.SetGraphicsRootResource(+Stats, statsBuffer.GetUav());
        }

        cmdList.DispatchMesh({ m_Consts.GrassPatchCount, 1, 1 }, { +joint::ProceduralGrassConsts::AsGroupSize, 1, 1 });
    }

    void ProceduralGrassPass::CopyStats(benzin::GraphicsCmdList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "CopyStats");

        const benzin::Buffer& destBuffer = ms_Resources->Get(BufferId::ProceduralGrass_ReadbackStats);
        const benzin::Buffer& sourceBuffer = ms_Resources->Get(BufferId::ProceduralGrass_UavStats);

        const uint32_t dataSizeInBytes = (uint32_t)sourceBuffer.GetSizeInBytes();
        const uint64_t destOffsetInBytes = (ms_Device->GetCpuFrameIndex() % benzin::CmdLineArgs::GetReadbackLatency()) * dataSizeInBytes;
        const uint64_t readbackOffsetInBytes = ((ms_Device->GetCpuFrameIndex() + 1) % benzin::CmdLineArgs::GetReadbackLatency()) * dataSizeInBytes;

        cmdList.CopyBufferRegion(destBuffer, destOffsetInBytes, sourceBuffer, 0, dataSizeInBytes);

        destBuffer.MapReadbackData(readbackOffsetInBytes, dataSizeInBytes, [](const std::byte* mappedData)
        {
            const auto statValues = benzin::ToSpan((const uint32_t*)mappedData, magic_enum::enum_count<joint::ProceduralGrassStat>());

            auto& stats = ms_Settings->GetSection<ProceduralGrassStats>();
            stats.PatchCount = statValues[+joint::ProceduralGrassStat::PatchCount];
            stats.BladeCount = statValues[+joint::ProceduralGrassStat::BladeCount];
            stats.VertexCount = statValues[+joint::ProceduralGrassStat::VertexCount];
            stats.TriangleCount = statValues[+joint::ProceduralGrassStat::TriangleCount];
        });
    }

}
