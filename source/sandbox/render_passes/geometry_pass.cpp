#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/geometry_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/pso.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/geometry_resources.hpp>
#include <shaders/joint/mesh_types.hpp>

BenzinAllowDereferenceOperatorForEnum(joint::GeometryResources);
BenzinAllowDereferenceOperatorForEnum(joint::MeshletConsts);

namespace sandbox
{

    // GeometryPass

    GeometryPass::GeometryPass()
    {
        CreateGeometryPso(PsoId::GeometryPass_Vertex, false);
        CreateGeometryPso(PsoId::GeometryPass_Mesh, true);

        {
            std::array<D3D12_INDIRECT_ARGUMENT_DESC, 2> d3d12ArgumentDescs = {};
            d3d12ArgumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            d3d12ArgumentDescs[0].Constant.RootParameterIndex = *benzin::UnifiedRootParameter::Root32Consts;
            d3d12ArgumentDescs[0].Constant.DestOffsetIn32BitValues = *joint::GeometryResources::MeshDrawIndex;
            d3d12ArgumentDescs[0].Constant.Num32BitValuesToSet = 1;
            d3d12ArgumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

            D3D12_COMMAND_SIGNATURE_DESC d3d12CmdSignatureDesc = {};
            d3d12CmdSignatureDesc.ByteStride = sizeof(benzin::DrawIndirectCmd);
            d3d12CmdSignatureDesc.NumArgumentDescs = (uint32_t)d3d12ArgumentDescs.size();
            d3d12CmdSignatureDesc.pArgumentDescs = d3d12ArgumentDescs.data();
            d3d12CmdSignatureDesc.NodeMask = 0;

            BenzinD3D12Call(ms_Device->GetD3D12Device()->CreateCommandSignature(
                &d3d12CmdSignatureDesc,
                ms_Device->GetUnifiedRootSignature().GetD3D12RootSignature(),
                IID_PPV_ARGS(&m_D3D12DrawIndirectCmdSignature)));
        }

        {
            std::array<D3D12_INDIRECT_ARGUMENT_DESC, 2> d3d12ArgumentDescs = {};
            d3d12ArgumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            d3d12ArgumentDescs[0].Constant.RootParameterIndex = *benzin::UnifiedRootParameter::Root32Consts;
            d3d12ArgumentDescs[0].Constant.DestOffsetIn32BitValues = *joint::GeometryResources::MeshDrawIndex;
            d3d12ArgumentDescs[0].Constant.Num32BitValuesToSet = 3;
            d3d12ArgumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;

            D3D12_COMMAND_SIGNATURE_DESC d3d12CmdSignatureDesc = {};
            d3d12CmdSignatureDesc.ByteStride = sizeof(benzin::DispatchMeshIndirectCmd);
            d3d12CmdSignatureDesc.NumArgumentDescs = (uint32_t)d3d12ArgumentDescs.size();
            d3d12CmdSignatureDesc.pArgumentDescs = d3d12ArgumentDescs.data();
            d3d12CmdSignatureDesc.NodeMask = 0;

            BenzinD3D12Call(ms_Device->GetD3D12Device()->CreateCommandSignature(
                &d3d12CmdSignatureDesc,
                ms_Device->GetUnifiedRootSignature().GetD3D12RootSignature(),
                IID_PPV_ARGS(&m_D3D12DispatchMeshIndirectCmdSignature)));
        }
    }

    GeometryPass::~GeometryPass()
    {
        ms_Device->DeferredRelease(m_D3D12DrawIndirectCmdSignature);
        m_D3D12DrawIndirectCmdSignature = nullptr;

        ms_Device->DeferredRelease(m_D3D12DispatchMeshIndirectCmdSignature);
        m_D3D12DispatchMeshIndirectCmdSignature = nullptr;

        ms_PsoManager->Destroy(PsoId::GeometryPass_Vertex);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Mesh);

        ms_Resources->Destroy(TextureId::AlbedoAndRoughness);
        ms_Resources->Destroy(TextureId::EmissiveAndMetallic);
        ms_Resources->Destroy(TextureId::WorldNormal);
        ms_Resources->Destroy(TextureId::Mv);
        ms_Resources->Destroy(TextureId::ViewDepth);
        ms_Resources->Destroy(TextureId::DepthStencil);
    }

    void GeometryPass::OnRenderViewportResize()
    {
        const auto createGBufferTexture = [](TextureId id, DXGI_FORMAT dxgiFormat, benzin::TextureAccessFlag accessFlag)
        {
            ms_Resources->Create(id, benzin::TextureCreation
            {
                .m_DebugName = magic_enum::enum_name(id),
                .m_DxgiFormat = dxgiFormat,
                .m_Width = ms_RenderViewportWidth,
                .m_Height = ms_RenderViewportHeight,
                .m_MipCount = 1,
                .m_AccessFlags = accessFlag,
            });
        };

        createGBufferTexture(TextureId::AlbedoAndRoughness, GBufferSettings::ms_Color0DxgiFormat, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::EmissiveAndMetallic, GBufferSettings::ms_Color1DxgiFormat, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::WorldNormal, GBufferSettings::ms_Color2DxgiFormat, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::Mv, GBufferSettings::ms_Color3DxgiFormat, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::DepthStencil, GBufferSettings::ms_DepthStencilDxgiFormat, benzin::TextureAccessFlag::AllowDepthStencil);

        ms_Resources->Create(TextureId::ViewDepth, benzin::TextureCreation
        {
            .m_DebugName = magic_enum::enum_name(TextureId::ViewDepth),
            .m_DxgiFormat = GBufferSettings::ms_Color4DxgiFormat,
            .m_Width = ms_RenderViewportWidth,
            .m_Height = ms_RenderViewportHeight,
            .m_MipCount = 1,
            .m_AccessFlags = benzin::TextureAccessFlag::AllowRenderTarget | benzin::TextureAccessFlag::AllowUnorderedAccess,
            .m_ClearValueVariant = DirectX::XMFLOAT4{ std::numeric_limits<float>::max(), 0.0f, 0.0f, 0.0f }, // R32 max value
        });
    }

    void GeometryPass::OnRender() const
    {
        using Resources = joint::GeometryResources;

        BenzinProfile();
        BenzinGpuProfile("Geometry");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const GBuffer gbuffer{ *ms_Resources };

        cmdList.AddTransition(*ms_Scene->m_MeshDrawBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList.AddTransition(*ms_Scene->m_MaterialBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);

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

        cmdList.ClearRenderTarget(gbuffer.m_AlbedoAndRoughness);
        cmdList.ClearRenderTarget(gbuffer.m_EmissiveAndMetallic);
        cmdList.ClearRenderTarget(gbuffer.m_WorldNormal);
        cmdList.ClearRenderTarget(gbuffer.m_Mv);
        cmdList.ClearRenderTarget(gbuffer.m_ViewDepth);
        cmdList.ClearDepthStencil(gbuffer.m_DepthStencil);

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);

        cmdList.SetGraphicsRootResource(*Resources::MeshDraws, ms_Scene->m_MeshDrawBuffer->GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::Materials, ms_Scene->m_MaterialBuffer->GetSrv());

        const auto& settings = ms_Settings->GetSection<GBufferSettings>();

        if (settings.m_IsMeshPipelineUsed)
        {
            cmdList.SetMeshPso(ms_PsoManager->GetMesh(PsoId::GeometryPass_Mesh));

            cmdList.AddTransition(*ms_Scene->m_VertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.AddTransition(*ms_Scene->m_MeshletBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.AddTransition(*ms_Scene->m_MeshletCullVolumeBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.AddTransition(*ms_Scene->m_MeshletVertexIndexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.AddTransition(*ms_Scene->m_MeshletIndexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.AddTransition(*ms_Scene->m_DispatchMeshIndirectCmdBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, true);

            cmdList.SetGraphicsRootResource(*Resources::Vertices, ms_Scene->m_VertexBuffer->GetSrv());
            cmdList.SetGraphicsRootResource(*Resources::Meshlets, ms_Scene->m_MeshletBuffer->GetSrv());
            cmdList.SetGraphicsRootResource(*Resources::MeshletCullVolumes, ms_Scene->m_MeshletCullVolumeBuffer->GetSrv());
            cmdList.SetGraphicsRootResource(*Resources::MeshletVertexIndices, ms_Scene->m_MeshletVertexIndexBuffer->GetSrv());
            cmdList.SetGraphicsRootResource(*Resources::MeshletIndices, ms_Scene->m_MeshletIndexBuffer->GetSrv());

            if (settings.m_IsIndirectDrawEnabled)
            {
                cmdList.GetD3D12GraphicsCommandList()->ExecuteIndirect(
                    m_D3D12DispatchMeshIndirectCmdSignature,
                    (uint32_t)ms_Scene->m_DispatchMeshIndirectCmdBuffer->GetElementCount(),
                    ms_Scene->m_DispatchMeshIndirectCmdBuffer->GetD3D12Resource(),
                    0,
                    nullptr,
                    0);
            }
            else
            {
                for (uint32_t i = 0; i < ms_Scene->m_JointMeshDraws.size(); ++i)
                {
                    cmdList.SetGraphicsRootConstant(*Resources::MeshDrawIndex, i);

                    const joint::MeshDraw& jointDraw = ms_Scene->m_JointMeshDraws[i];
                    const benzin::MeshPart& part = ms_Scene->m_MeshParts[jointDraw.m_PartIndex];

                    cmdList.SetGraphicsRootConstant(*Resources::PartMeshletOffset, part.m_MeshletOffset);
                    cmdList.SetGraphicsRootConstant(*Resources::PartMeshletCount, part.m_MeshletCount);
                    cmdList.DispatchMesh({ part.m_MeshletCount, 1, 1, }, { *joint::MeshletConsts::AsGroupSize, 1, 1 });
                }
            }
        }
        else
        {
            cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::GeometryPass_Vertex));

            cmdList.AddTransition(*ms_Scene->m_VertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            cmdList.AddTransition(*ms_Scene->m_IndexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER, true);

            cmdList.SetVertexBuffer(*ms_Scene->m_VertexBuffer);
            cmdList.SetIndexBuffer(*ms_Scene->m_IndexBuffer);

            cmdList.GetD3D12GraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            if (settings.m_IsIndirectDrawEnabled)
            {
                cmdList.GetD3D12GraphicsCommandList()->ExecuteIndirect(
                    m_D3D12DrawIndirectCmdSignature,
                    (uint32_t)ms_Scene->m_DrawIndirectCmdBuffer->GetElementCount(),
                    ms_Scene->m_DrawIndirectCmdBuffer->GetD3D12Resource(),
                    0,
                    nullptr,
                    0);
            }
            else
            {
                for (uint32_t i = 0; i < ms_Scene->m_JointMeshDraws.size(); ++i)
                {
                    cmdList.SetGraphicsRootConstant(*Resources::MeshDrawIndex, i);

                    const joint::MeshDraw& jointDraw = ms_Scene->m_JointMeshDraws[i];
                    const benzin::MeshPart& part = ms_Scene->m_MeshParts[jointDraw.m_PartIndex];

                    cmdList.DrawIndexed(part.m_IndexCount, part.m_IndexOffset, part.m_VertexOffset);
                }
            }
        }
    }

    void GeometryPass::CreateGeometryPso(PsoId id, bool isMeshPipeline)
    {
        const auto configureGraphicsPsoProxy = [](auto& proxy)
        {
            proxy.m_Ps.m_FileName = "geometry_pass.hlsl";
            proxy.m_DepthStencilDxgiFormat = GBufferSettings::ms_DepthStencilDxgiFormat;
            proxy.m_RenderTargetDxgiFormats.reserve(5);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color0DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color1DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color2DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color3DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color4DxgiFormat);

            proxy.m_DepthState.m_IsEnabled = true;
            proxy.m_DepthState.m_IsWriteEnabled = true;
            proxy.m_DepthState.m_D3D12ComparisonFunction = D3D12_COMPARISON_FUNC_GREATER;
        };

        if (isMeshPipeline)
        {
            ms_PsoManager->Create(id, [&configureGraphicsPsoProxy](benzin::MeshPsoProxy& proxy)
            {
                proxy.m_As.m_FileName = "geometry_pass.hlsl";
                proxy.m_Ms.m_FileName = "geometry_pass.hlsl";

                proxy.m_As.m_Defines.push_back("MESH_PIPELINE");
                proxy.m_Ms.m_Defines.push_back("MESH_PIPELINE");

                configureGraphicsPsoProxy(proxy);
            });
        }
        else
        {
            ms_PsoManager->Create(id, [&configureGraphicsPsoProxy](benzin::VertexPsoProxy& proxy)
            {
                proxy.m_InputLayout.emplace_back("Position", DXGI_FORMAT_R32G32B32_FLOAT);
                proxy.m_InputLayout.emplace_back("Normal", DXGI_FORMAT_R32G32B32_FLOAT);
                proxy.m_InputLayout.emplace_back("Uv", DXGI_FORMAT_R32G32_FLOAT);

                BenzinAssert(benzin::GetDxgiFormatSizeInBytes(proxy.m_InputLayout[0].m_DxgiFormat) == sizeof(joint::MeshVertex::m_Position));
                BenzinAssert(benzin::GetDxgiFormatSizeInBytes(proxy.m_InputLayout[1].m_DxgiFormat) == sizeof(joint::MeshVertex::m_Normal));
                BenzinAssert(benzin::GetDxgiFormatSizeInBytes(proxy.m_InputLayout[2].m_DxgiFormat) == sizeof(joint::MeshVertex::m_Uv));

                proxy.m_Vs.m_FileName = "geometry_pass.hlsl";

                configureGraphicsPsoProxy(proxy);
            });
        }
    }

    // GBuffer

    GBuffer::GBuffer(const benzin::RenderResources& resources)
        : m_AlbedoAndRoughness{ resources.Get(TextureId::AlbedoAndRoughness) }
        , m_EmissiveAndMetallic{ resources.Get(TextureId::EmissiveAndMetallic) }
        , m_WorldNormal{ resources.Get(TextureId::WorldNormal) }
        , m_Mv{ resources.Get(TextureId::Mv) }
        , m_ViewDepth{ resources.Get(TextureId::ViewDepth) }
        , m_DepthStencil{ resources.Get(TextureId::DepthStencil) }
    {}

}
