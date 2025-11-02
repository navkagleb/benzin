#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/environment_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>
#include <shaders/joint/environment_resources.hpp>

BenzinAllowDereferenceOperatorForEnum(joint::EnvironmentResources);
BenzinAllowDereferenceOperatorForEnum(joint::EquirectangularToCubeResources);

namespace sandbox
{

    EnvironmentPass::EnvironmentPass()
    {
        ms_PsoManager->Create(PsoId::Environment, [](benzin::VertexPsoProxy& proxy)
        {
            proxy.m_Vs.m_FileName = "fullscreen_triangle.hlsl";
            proxy.m_Vs.m_EntryPoint = "VsMainDepth0";
            proxy.m_Ps.m_FileName = "environment_pass.hlsl";
            proxy.m_DepthState.m_IsEnabled = true;
            proxy.m_DepthState.m_D3D12ComparisonFunction = D3D12_COMPARISON_FUNC_EQUAL;
            proxy.m_RenderTargetDxgiFormats.push_back(DeferredLightingSettings::ms_HdrColorDxgiFormat),
            proxy.m_DepthStencilDxgiFormat = GBufferSettings::ms_DepthStencilDxgiFormat;
        });
    }

    EnvironmentPass::~EnvironmentPass()
    {
        ms_PsoManager->Destroy(PsoId::Environment);
    }

    void EnvironmentPass::OnZeroFrameInit()
    {
        std::unique_ptr<benzin::Texture> equirectangularTexture;

        {
            benzin::TextureImage equirectangularTextureImage;
            BenzinAssertExpr(benzin::LoadTextureImageFromHdrFile("spaichingen_hill_4k.hdr", equirectangularTextureImage));

            benzin::TextureCreation textureCreation;
            textureCreation.m_DebugName = equirectangularTextureImage.m_DebugName;
            textureCreation.m_DxgiFormat = equirectangularTextureImage.m_DxgiFormat;
            textureCreation.m_Width = equirectangularTextureImage.m_Width;
            textureCreation.m_Height = equirectangularTextureImage.m_Height;
            textureCreation.m_MipCount = 1;
            benzin::MakeUniquePtr(equirectangularTexture, *ms_Device, textureCreation);

            benzin::CopyCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(equirectangularTexture->GetSizeInBytes());
            cmdList.UploadToTexture(*equirectangularTexture, benzin::ToSpan(equirectangularTextureImage.m_PixelData));
        }

        {
            ms_PsoManager->Create(PsoId::Environment_EquirectangularToCube, [](benzin::ComputePsoProxy& proxy)
            {
                proxy.m_Cs.m_FileName = "equirectangular_to_cube_pass.hlsl";
            });

            BenzinExecuteOnScopeExit([]
            {
                ms_PsoManager->Destroy(PsoId::Environment_EquirectangularToCube);
            });

            constexpr uint32_t cubeMapSize = 1024;

            benzin::TextureCreation textureCreation;
            textureCreation.m_DebugName = "EnvironmentPass::CubeMap";
            textureCreation.m_IsCubeMap = true;
            textureCreation.m_DxgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
            textureCreation.m_Width = cubeMapSize;
            textureCreation.m_Height = cubeMapSize;
            textureCreation.m_Depth = 6;
            textureCreation.m_MipCount = 1;
            textureCreation.m_AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess;
            benzin::MakeUniquePtr(m_CubeTexture, *ms_Device, textureCreation);

            benzin::ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

            cmdList.AddTransition(*m_CubeTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

            using Resources = joint::EquirectangularToCubeResources;
            cmdList.SetComputeRootResource(*Resources::EquirectangularTexture, equirectangularTexture->GetSrv());
            cmdList.SetComputeRootResource(*Resources::OutCubeMap, m_CubeTexture->GetUav());

            cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::Environment_EquirectangularToCube));
            cmdList.Dispatch({ cubeMapSize, cubeMapSize, m_CubeTexture->GetDepth() }, { 8, 8, 1 });

            cmdList.AddUnorderedAccess(*m_CubeTexture);
            cmdList.AddTransition(*m_CubeTexture, D3D12_RESOURCE_STATE_GENERIC_READ, true);
        }
    }

    void EnvironmentPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("Environment");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const auto& hdrColor = ms_Resources->Get(TextureId::HdrColor);
        const auto& depthStencil = ms_Resources->Get(TextureId::DepthStencil);

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);

        cmdList.AddTransition(hdrColor, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddTransition(depthStencil, D3D12_RESOURCE_STATE_DEPTH_READ, true);

        cmdList.SetRenderTargets({ hdrColor.GetRtv() }, &depthStencil.GetDsv());

        cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::Environment));
        cmdList.SetGraphicsRootResource(*joint::EnvironmentResources::CubeMap, m_CubeTexture->GetSrv());

        cmdList.GetD3D12GraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList.DrawVertexed(3);
    }

}
