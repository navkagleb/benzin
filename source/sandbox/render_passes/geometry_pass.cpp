#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/geometry_pass.hpp>

#include <sandbox/render_passes/gbuffer.hpp>
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

    GeometryPass::GeometryPass()
    {
        CreateGeometryPso(PsoId::GeometryPass_Vertex, false);
        CreateGeometryPso(PsoId::GeometryPass_Mesh, true);

        {
            std::array<D3D12_INDIRECT_ARGUMENT_DESC, 2> d3d12ArgumentDescs = {};
            d3d12ArgumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            d3d12ArgumentDescs[0].Constant.RootParameterIndex = *benzin::UnifiedRootParameter::Root32Consts;
            d3d12ArgumentDescs[0].Constant.DestOffsetIn32BitValues = *joint::GeometryResources::MeshDrawIndex;
            d3d12ArgumentDescs[0].Constant.Num32BitValuesToSet = 2;
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
            d3d12ArgumentDescs[0].Constant.Num32BitValuesToSet = 4;
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
        const auto createGBufferTexture = [](TextureId id, benzin::GraphicsFormat format, benzin::TextureAccessFlag accessFlag)
        {
            ms_Resources->Create(id, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(id),
                .Format = format,
                .Width = ms_RenderViewportWidth,
                .Height = ms_RenderViewportHeight,
                .MipCount = 1,
                .AccessFlags = accessFlag,
            });
        };

        createGBufferTexture(TextureId::AlbedoAndRoughness, GBufferSettings::s_Color0Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::EmissiveAndMetallic, GBufferSettings::s_Color1Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::WorldNormal, GBufferSettings::s_Color2Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::Mv, GBufferSettings::s_Color3Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::DepthStencil, GBufferSettings::s_DepthStencilFormat, benzin::TextureAccessFlag::AllowDepthStencil);

        ms_Resources->Create(TextureId::ViewDepth, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(TextureId::ViewDepth),
            .Format = GBufferSettings::s_Color4Format,
            .Width = ms_RenderViewportWidth,
            .Height = ms_RenderViewportHeight,
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowRenderTarget | benzin::TextureAccessFlag::AllowUnorderedAccess,
            .ClearValueVariant = DirectX::XMFLOAT4{ std::numeric_limits<float>::max(), 0.0f, 0.0f, 0.0f }, // R32 max value
        });
    }

    void GeometryPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("Geometry");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const GBuffer gbuffer{ *ms_Resources };
        const benzin::ScopedResourceBarriers scopeGBufferBarriers = gbuffer.CreateResourceBarriers(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

        gbuffer.SetRenderTargets(cmdList);
        gbuffer.ClearRenderTargets(cmdList);
        gbuffer.ClearDepthStencil(cmdList);

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);

        using Resources = joint::GeometryResources;

        cmdList.SetGraphicsRootResource(*Resources::MeshDrawParts, ms_Scene->m_MeshDrawPartBuffer->GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::MeshDraws, ms_Scene->m_MeshDrawBuffer->GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::Materials, ms_Scene->m_MaterialBuffer->GetSrv());

        const auto& settings = ms_Settings->GetSection<GBufferSettings>();

        if (settings.m_IsMeshPipelineUsed)
        {
            cmdList.SetMeshPso(ms_PsoManager->GetMesh(PsoId::GeometryPass_Mesh));

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
                for (uint32_t drawIndex = 0; drawIndex < ms_Scene->m_MeshDraws.size(); ++drawIndex)
                {
                    cmdList.SetGraphicsRootConstant(*Resources::MeshDrawIndex, drawIndex);

                    const benzin::MeshDraw& draw = ms_Scene->m_MeshDraws[drawIndex];
                    const uint32_t drawPartOffset = ms_Scene->m_MeshRanges[draw.m_MeshRangeIndex].m_DrawPartOffset;
                    const uint32_t drawPartCount = ms_Scene->m_MeshRanges[draw.m_MeshRangeIndex].m_DrawPartCount;

                    for (uint32_t drawPartIndex = drawPartOffset; drawPartIndex < drawPartOffset + drawPartCount; ++drawPartIndex)
                    {
                        const benzin::MeshDrawPart& drawPart = ms_Scene->m_MeshDrawParts[drawPartIndex];
                        const benzin::MeshPart& part = ms_Scene->m_MeshParts[drawPart.m_PartIndex];

                        cmdList.SetGraphicsRootConstant(*Resources::MeshDrawPartIndex, drawPartIndex);
                        cmdList.SetGraphicsRootConstant(*Resources::PartMeshletOffset, part.m_MeshletOffset);
                        cmdList.SetGraphicsRootConstant(*Resources::PartMeshletCount, part.m_MeshletCount);

                        cmdList.DispatchMesh({ part.m_MeshletCount, 1, 1, }, { *joint::MeshletConsts::AsGroupSize, 1, 1 });
                    }
                }
            }
        }
        else
        {
            cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::GeometryPass_Vertex));
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
                for (uint32_t drawIndex = 0; drawIndex < ms_Scene->m_MeshDraws.size(); ++drawIndex)
                {
                    cmdList.SetGraphicsRootConstant(*Resources::MeshDrawIndex, drawIndex);

                    const benzin::MeshDraw& draw = ms_Scene->m_MeshDraws[drawIndex];
                    const uint32_t drawPartOffset = ms_Scene->m_MeshRanges[draw.m_MeshRangeIndex].m_DrawPartOffset;
                    const uint32_t drawPartCount = ms_Scene->m_MeshRanges[draw.m_MeshRangeIndex].m_DrawPartCount;

                    for (uint32_t drawPartIndex = drawPartOffset; drawPartIndex < drawPartOffset + drawPartCount; ++drawPartIndex)
                    {
                        const benzin::MeshDrawPart& drawPart = ms_Scene->m_MeshDrawParts[drawPartIndex];
                        const benzin::MeshPart& part = ms_Scene->m_MeshParts[drawPart.m_PartIndex];

                        cmdList.SetGraphicsRootConstant(*Resources::MeshDrawPartIndex, drawPartIndex);
                        cmdList.DrawIndexed(part.m_IndexCount, part.m_IndexOffset, part.m_VertexOffset);
                    }
                }
            }
        }
    }

    void GeometryPass::CreateGeometryPso(PsoId id, bool isMeshPipeline)
    {
        const auto configureGraphicsPsoProxy = [](auto& proxy)
        {
            proxy.Ps.FileName = "geometry_pass.hlsl";
            proxy.RasterizerState.CullMode = benzin::CullMode::Back;
            proxy.RasterizerState.IsIndexOrderClockwise = true;
            proxy.DepthStencilFormat = GBufferSettings::s_DepthStencilFormat;
            proxy.RenderTargetFormats.reserve(5);
            proxy.RenderTargetFormats.push_back(GBufferSettings::s_Color0Format);
            proxy.RenderTargetFormats.push_back(GBufferSettings::s_Color1Format);
            proxy.RenderTargetFormats.push_back(GBufferSettings::s_Color2Format);
            proxy.RenderTargetFormats.push_back(GBufferSettings::s_Color3Format);
            proxy.RenderTargetFormats.push_back(GBufferSettings::s_Color4Format);

            proxy.DepthState.IsEnabled = true;
            proxy.DepthState.IsWriteEnabled = true;
            proxy.DepthState.ComparisonFunction = benzin::ComparisonFunction::Greater;
        };

        if (isMeshPipeline)
        {
            ms_PsoManager->Create(id, [&configureGraphicsPsoProxy](benzin::MeshPsoProxy& proxy)
            {
                proxy.As.FileName = "geometry_pass.hlsl";
                proxy.Ms.FileName = "geometry_pass.hlsl";

                proxy.As.Defines.push_back("MESH_PIPELINE");
                proxy.Ms.Defines.push_back("MESH_PIPELINE");

                configureGraphicsPsoProxy(proxy);
            });
        }
        else
        {
            ms_PsoManager->Create(id, [&configureGraphicsPsoProxy](benzin::VertexPsoProxy& proxy)
            {
                proxy.InputLayout.emplace_back("Position", benzin::GraphicsFormat::Rgb32Float);
                proxy.InputLayout.emplace_back("Normal", benzin::GraphicsFormat::Rgb32Float);
                proxy.InputLayout.emplace_back("Uv", benzin::GraphicsFormat::Rg32Float);

                BenzinAssert(benzin::GetFormatSizeInBytes(proxy.InputLayout[0].Format) == sizeof(joint::MeshVertex::Position));
                BenzinAssert(benzin::GetFormatSizeInBytes(proxy.InputLayout[1].Format) == sizeof(joint::MeshVertex::Normal));
                BenzinAssert(benzin::GetFormatSizeInBytes(proxy.InputLayout[2].Format) == sizeof(joint::MeshVertex::Uv));

                proxy.Vs.FileName = "geometry_pass.hlsl";

                configureGraphicsPsoProxy(proxy);
            });
        }
    }

}
