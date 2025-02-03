#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/environment_pass.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/environment_resources.hpp>
#include <shaders/joint/full_screen_debug_resources.hpp>

#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

BenzinEnableUnaryPlusForEnum(joint::Rc_Environment);
BenzinEnableUnaryPlusForEnum(joint::Rc_EquirectangularToCube);

namespace sandbox
{

    EnvironmentPass::EnvironmentPass()
    {
        ms_PsoManager->CreateGraphicsPso(+Pso::Environment, [](benzin::GraphicsPsoProxy& proxy)
        {
            proxy.DebugName = "EnvironmentPass";
            proxy.VsFileName = "fullscreen_triangle.hlsl";
            proxy.VsEntryPoint = "VsMainDepth1";
            proxy.PsFileName = "environment_pass.hlsl";
            proxy.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle;
            proxy.DepthState = benzin::DepthState
            {
                .IsWriteEnabled = false,
                .ComparisonFunction = benzin::ComparisonFunction::Equal,
            };
            proxy.RenderTargetFormats.push_back(benzin::GraphicsFormat::Rgba8Unorm),
            proxy.DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint;
        });
    }

    EnvironmentPass::~EnvironmentPass()
    {
        ms_PsoManager->DestroyPso(+Pso::Environment);
    }

    void EnvironmentPass::OnZeroFrameInit()
    {
        std::unique_ptr equirectangularTexture = LoadEquirectangularTexture();
        ComputeCubeMapTexture(*equirectangularTexture);
    }

    void EnvironmentPass::OnUpdate()
    {
        const auto& settings = ms_Settings->GetSection<FullScreenDebugSettings>();

        m_IsRenderingEnabled = settings.DebugOutputType == joint::DebugOutputType::None;
    }

    void EnvironmentPass::OnRender() const
    {
        BenzinProfile();

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "Environment");

        const auto& finalTexture = ms_Resources->GetTexture(+Texture::Final);
        const auto& depthStencilBuffer = ms_Resources->GetTexture(+Texture::DepthStencil);

        commandList.SetViewport(ms_RenderViewport);
        commandList.SetScissorRect(ms_RenderScissorRect);

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ finalTexture, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ depthStencilBuffer, benzin::ResourceState::DepthRead },
        );

        commandList.SetRenderTargets({ finalTexture.GetRtv() }, &depthStencilBuffer.GetDsv());

        commandList.SetPso(ms_PsoManager->GetPso(+Pso::Environment));
        commandList.SetRootResource(+joint::Rc_Environment::CubeMap, m_CubeTexture->GetSrv());

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
        ms_PsoManager->CreateComputePso(+Pso::Environment_EquirectangularToCube, [](benzin::ComputePsoProxy& proxy)
        {
            proxy.DebugName = "EquirectangularToCube";
            proxy.CsFileName = "equirectangular_to_cube_pass.hlsl";
        });

        BenzinExecuteOnScopeExit([]
        {
            ms_PsoManager->DestroyPso(+Pso::Environment_EquirectangularToCube);
        });

        const uint32_t cubeMapSize = 1024;
        benzin::MakeUniquePtr(m_CubeTexture, *ms_Device, benzin::TextureCreation
        {
            .DebugName = "EnvironmentCubeMap",
            .IsCubeMap = true,
            .Format = benzin::GraphicsFormat::Rgba32Float,
            .Width = cubeMapSize,
            .Height = cubeMapSize,
            .Depth = 6,
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        commandList.SetPso(ms_PsoManager->GetPso(+Pso::Environment_EquirectangularToCube));
        commandList.SetRootResource(+joint::Rc_EquirectangularToCube::EquirectangularTexture, equirectangularTexture.GetSrv());
        commandList.SetRootResource(+joint::Rc_EquirectangularToCube::OutCubeMap, m_CubeTexture->GetUav());

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ *m_CubeTexture, benzin::ResourceState::UnorderedAccess },
        );

        const DirectX::XMUINT3 dimensions{ cubeMapSize, cubeMapSize, m_CubeTexture->GetDepth() };
        commandList.Dispatch(dimensions, { 8, 8, 1 });
    }

}
