#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/environment_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
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
            proxy.m_Vs.m_FileName = "environment_pass.hlsl";
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

            BenzinExecuteOnScopeExit([] { ms_PsoManager->Destroy(PsoId::Environment_EquirectangularToCube); });

            constexpr uint32_t cubeMapSize = 1024;

            m_CubeTexture = ms_Device->GetPersistentDefaultAllocator().AllocateTexture([](benzin::TextureCreation& creation)
            {
                creation.m_DebugName = "EnvironmentPass::CubeMap";
                creation.m_IsCubeMap = true;
                creation.m_DxgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
                creation.m_Width = cubeMapSize;
                creation.m_Height = cubeMapSize;
                creation.m_Depth = 6;
                creation.m_MipCount = 1;
                creation.m_AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess;
            });

            benzin::ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

            cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::Environment_EquirectangularToCube));

            using Resources = joint::EquirectangularToCubeResources;
            cmdList.SetComputeRootSrv(*Resources::EquirectangularTexture, *equirectangularTexture);
            cmdList.SetComputeRootUav(*Resources::OutCubeMap, *m_CubeTexture);
            cmdList.FlushBarriers();

            cmdList.Dispatch({ cubeMapSize, cubeMapSize, m_CubeTexture->GetDepth() }, { 8, 8, 1 });

            cmdList.AddUnorderedAccess(*m_CubeTexture);
        }
    }

    void EnvironmentPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("Environment");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);
        cmdList.GetD3D12GraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::Environment));

        cmdList.AddRenderTarget(ms_Resources->Get(TextureId::HdrColor), D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddDepthStencil(ms_Resources->Get(TextureId::Depth), D3D12_RESOURCE_STATE_DEPTH_READ);
        cmdList.SetRenderTargets();

        cmdList.SetGraphicsRootSrv(*joint::EnvironmentResources::CubeMap, *m_CubeTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmdList.FlushBarriers();

        cmdList.DrawVertexed(3);
    }

}
