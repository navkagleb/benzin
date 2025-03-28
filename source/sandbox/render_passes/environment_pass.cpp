#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/environment_pass.hpp"

#include <benzin/core/profiler.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/environment_resources.hpp>

#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

BenzinEnableUnaryPlusForEnum(joint::Rc_Environment);
BenzinEnableUnaryPlusForEnum(joint::Rc_EquirectangularToCube);

namespace sandbox
{

    EnvironmentPass::EnvironmentPass()
    {
        ms_PsoManager->Create(PsoId::Environment, [](benzin::GraphicsPsoProxy& proxy)
        {
            proxy.VsFileName = "fullscreen_triangle.hlsl";
            proxy.VsEntryPoint = "VsMainDepth1";
            proxy.PsFileName = "environment_pass.hlsl";
            proxy.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle;
            proxy.DepthState = benzin::DepthState
            {
                .IsWriteEnabled = false,
                .ComparisonFunction = benzin::ComparisonFunction::Equal,
            };
            proxy.RenderTargetFormats.push_back(benzin::GraphicsFormat::Rgba16Float),
            proxy.DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint;
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

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "Environment");

        const auto& hdrColor = ms_Resources->Get(TextureId::HdrColor);
        const auto& depthStencil = ms_Resources->Get(TextureId::DepthStencil);

        commandList.SetViewport(ms_RenderViewport);
        commandList.SetScissorRect(ms_RenderScissorRect);

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ hdrColor, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ depthStencil, benzin::ResourceState::DepthRead },
        );

        commandList.SetRenderTargets({ hdrColor.GetRtv() }, &depthStencil.GetDsv());

        commandList.SetGraphicsPso(ms_PsoManager->GetGraphics(PsoId::Environment));
        commandList.SetGraphicsRootResource(+joint::Rc_Environment::CubeMap, m_CubeTexture->GetSrv());

        commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
        commandList.DrawVertexed(3);
    }

    std::unique_ptr<benzin::Texture> EnvironmentPass::LoadEquirectangularTexture()
    {
        benzin::TextureImage equirectangularTextureImage;
        BenzinAssertExpr(benzin::LoadTextureImageFromHdrFile("spaichingen_hill_4k.hdr", equirectangularTextureImage));

        auto equirectangularTexture = std::make_unique<benzin::Texture>(*ms_Device, benzin::TextureCreation
        {
            .DebugName = equirectangularTextureImage.DebugName,
            .Format = equirectangularTextureImage.Format,
            .Width = equirectangularTextureImage.Width,
            .Height = equirectangularTextureImage.Height,
            .MipCount = 1,
        });

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList(equirectangularTexture->GetSize());
        commandList.UploadToTextureTopMip(*equirectangularTexture, std::as_bytes(std::span{ equirectangularTextureImage.ImageData }));
        
        return equirectangularTexture;
    }

    void EnvironmentPass::ComputeCubeMapTexture(benzin::Texture& equirectangularTexture)
    {
        ms_PsoManager->Create(PsoId::Environment_EquirectangularToCube, [](benzin::ComputePsoProxy& proxy)
        {
            proxy.CsFileName = "equirectangular_to_cube_pass.hlsl";
        });

        BenzinExecuteOnScopeExit([]
        {
            ms_PsoManager->Destroy(PsoId::Environment_EquirectangularToCube);
        });

        const uint32_t cubeMapSize = 1024;
        benzin::MakeUniquePtr(m_CubeTexture, *ms_Device, benzin::TextureCreation
        {
            .DebugName = "EnvironmentCubeMap",
            .IsCubeMap = true,
            .Format = benzin::GraphicsFormat::Rgba16Float,
            .Width = cubeMapSize,
            .Height = cubeMapSize,
            .Depth = 6,
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        commandList.SetComputePso(ms_PsoManager->GetCompute(PsoId::Environment_EquirectangularToCube));
        commandList.SetComputeRootResource(+joint::Rc_EquirectangularToCube::EquirectangularTexture, equirectangularTexture.GetSrv());
        commandList.SetComputeRootResource(+joint::Rc_EquirectangularToCube::OutCubeMap, m_CubeTexture->GetUav());

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ *m_CubeTexture, benzin::ResourceState::UnorderedAccess },
        );

        const DirectX::XMUINT3 dimensions{ cubeMapSize, cubeMapSize, m_CubeTexture->GetDepth() };
        commandList.Dispatch(dimensions, { 8, 8, 1 });
    }

}
