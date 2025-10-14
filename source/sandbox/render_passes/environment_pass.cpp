#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/environment_pass.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/environment_resources.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

BenzinEnableUnaryPlusForEnum(joint::EnvironmentResources);
BenzinEnableUnaryPlusForEnum(joint::EquirectangularToCubeResources);

namespace sandbox
{

    EnvironmentPass::EnvironmentPass()
    {
        ms_PsoManager->Create(PsoId::Environment, [](benzin::VertexPsoProxy& proxy)
        {
            proxy.Vs.FileName = "fullscreen_triangle.hlsl";
            proxy.Vs.EntryPoint = "VsMainDepth0";
            proxy.Ps.FileName = "environment_pass.hlsl";
            proxy.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle;
            proxy.DepthState = benzin::DepthState
            {
                .IsWriteEnabled = false,
                .ComparisonFunction = benzin::ComparisonFunction::Equal,
            };
            proxy.RenderTargetFormats.push_back(DeferredLightingSettings::s_HdrColorFormat),
            proxy.DepthStencilFormat = GBufferSettings::s_DepthStencilFormat;
        });
    }

    EnvironmentPass::~EnvironmentPass()
    {
        ms_PsoManager->Destroy(PsoId::Environment);
    }

    void EnvironmentPass::OnZeroFrameInit()
    {
        std::unique_ptr equirectangularTexture = LoadEquirectangularTexture();
        ComputeCubeMapTexture(*equirectangularTexture);
    }

    void EnvironmentPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("Environment");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const benzin::Texture& hdrColor = ms_Resources->Get(TextureId::HdrColor);
        const benzin::Texture& depthStencil = ms_Resources->Get(TextureId::DepthStencil);

        cmdList.SetViewport(ms_RenderViewport);
        cmdList.SetScissorRect(ms_RenderScissorRect);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ hdrColor, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ depthStencil, benzin::ResourceState::DepthRead });

        cmdList.SetRenderTargets({ hdrColor.GetRtv() }, &depthStencil.GetDsv());

        cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::Environment));
        cmdList.SetGraphicsRootResource(+joint::EnvironmentResources::CubeMap, m_CubeTexture->GetSrv());

        cmdList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
        cmdList.DrawVertexed(3);
    }

    std::unique_ptr<benzin::Texture> EnvironmentPass::LoadEquirectangularTexture()
    {
        benzin::TextureImage equirectangularTextureImage;
        BenzinAssertExpr(benzin::LoadTextureImageFromHdrFile("spaichingen_hill_4k.hdr", equirectangularTextureImage));

        auto equirectangularTexture = std::make_unique<benzin::Texture>(*ms_Device, benzin::TextureCreation
        {
            .DebugName = equirectangularTextureImage.m_DebugName,
            .Format = equirectangularTextureImage.m_Format,
            .Width = equirectangularTextureImage.m_Width,
            .Height = equirectangularTextureImage.m_Height,
            .MipCount = 1,
        });

        benzin::CopyCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(equirectangularTexture->GetSizeInBytes());
        cmdList.UploadToTexture(*equirectangularTexture, benzin::ToSpan(equirectangularTextureImage.m_PixelData));
        
        return equirectangularTexture;
    }

    void EnvironmentPass::ComputeCubeMapTexture(benzin::Texture& equirectangularTexture)
    {
        ms_PsoManager->Create(PsoId::Environment_EquirectangularToCube, [](benzin::ComputePsoProxy& proxy)
        {
            proxy.Cs.FileName = "equirectangular_to_cube_pass.hlsl";
        });

        BenzinExecuteOnScopeExit([]
        {
            ms_PsoManager->Destroy(PsoId::Environment_EquirectangularToCube);
        });

        constexpr uint32_t cubeMapSize = 1024;
        benzin::MakeUniquePtr(m_CubeTexture, *ms_Device, benzin::TextureCreation
        {
            .DebugName = "EnvironmentPass::CubeMap",
            .IsCubeMap = true,
            .Format = benzin::GraphicsFormat::Rgba16Float,
            .Width = cubeMapSize,
            .Height = cubeMapSize,
            .Depth = 6,
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });

        benzin::ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        cmdList.AddResourceBarrier(benzin::TransitionBarrier{ *m_CubeTexture, benzin::ResourceState::UnorderedAccess }, true);

        using Resources = joint::EquirectangularToCubeResources;
        cmdList.SetComputeRootResource(+Resources::EquirectangularTexture, equirectangularTexture.GetSrv());
        cmdList.SetComputeRootResource(+Resources::OutCubeMap, m_CubeTexture->GetUav());

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::Environment_EquirectangularToCube));
        cmdList.Dispatch({ cubeMapSize, cubeMapSize, m_CubeTexture->GetDepth() }, { 8, 8, 1 });

        cmdList.AddResourceBarrier(benzin::UnorderedAccessBarrier{ *m_CubeTexture });
        cmdList.AddResourceBarrier(benzin::TransitionBarrier{ *m_CubeTexture, benzin::ResourceState::GenericRead }, true);
    }

}
