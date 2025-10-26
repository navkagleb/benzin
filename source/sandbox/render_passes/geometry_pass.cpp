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

#include <shaders/joint/depth_reprojection_resources.hpp>
#include <shaders/joint/geometry_resources.hpp>
#include <shaders/joint/mesh_types.hpp>

BenzinAllowDereferenceOperatorForEnum(joint::DepthReductionResources);
BenzinAllowDereferenceOperatorForEnum(joint::DepthResprojectionResources);
BenzinAllowDereferenceOperatorForEnum(joint::GeometryResources);
BenzinAllowDereferenceOperatorForEnum(joint::MeshletConsts);
BenzinEnableFlagsForEnum(sandbox::GeometryPass::PsoFlag);

namespace sandbox
{

    GeometryPass::GeometryPass()
    {
        CreateGeometryPso(PsoId::GeometryPass_Vertex);
        CreateGeometryPso(PsoId::GeometryPass_Vertex_Alpha, PsoFlag::AlphaTest);
        CreateGeometryPso(PsoId::GeometryPass_Mesh, PsoFlag::MeshPipeline);
        CreateGeometryPso(PsoId::GeometryPass_Mesh_Alpha, PsoFlag::MeshPipeline | PsoFlag::AlphaTest);

        std::array<D3D12_INDIRECT_ARGUMENT_DESC, 2> d3d12IndirectArgumentDescs = {};
        d3d12IndirectArgumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
        d3d12IndirectArgumentDescs[0].Constant.RootParameterIndex = *benzin::UnifiedRootParameter::Root32Consts;
        d3d12IndirectArgumentDescs[0].Constant.DestOffsetIn32BitValues = *joint::GeometryResources::MeshDrawIndex;
        d3d12IndirectArgumentDescs[0].Constant.Num32BitValuesToSet = 2;
        d3d12IndirectArgumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

        D3D12_COMMAND_SIGNATURE_DESC d3d12CommandSignatureDesc = {};
        d3d12CommandSignatureDesc.ByteStride = sizeof(benzin::MeshDrawIndirectCmd);
        d3d12CommandSignatureDesc.NumArgumentDescs = (uint32_t)d3d12IndirectArgumentDescs.size();
        d3d12CommandSignatureDesc.pArgumentDescs = d3d12IndirectArgumentDescs.data();
        d3d12CommandSignatureDesc.NodeMask = 0;

        BenzinD3D12Call(ms_Device->GetD3D12Device()->CreateCommandSignature(
            &d3d12CommandSignatureDesc,
            ms_Device->GetUnifiedRootSignature().GetD3D12RootSignature(),
            IID_PPV_ARGS(&m_D3D12DrawIndexedIndirectCmdSignature)));
    }

    GeometryPass::~GeometryPass()
    {
        ms_Device->DeferredRelease(m_D3D12DrawIndexedIndirectCmdSignature);
        m_D3D12DrawIndexedIndirectCmdSignature = nullptr;

        ms_PsoManager->Destroy(PsoId::GeometryPass_DepthReprojection);
        ms_PsoManager->Destroy(PsoId::GeometryPass_DepthReduction);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Vertex);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Vertex_Alpha);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Mesh);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Mesh_Alpha);

        ms_Resources->Destroy(TextureId::AlbedoAndRoughness);
        ms_Resources->Destroy(TextureId::EmissiveAndMetallic);
        ms_Resources->Destroy(TextureId::WorldNormal);
        ms_Resources->Destroy(TextureId::Mv);
        ms_Resources->Destroy(TextureId::ViewDepth);
        ms_Resources->Destroy(TextureId::DepthStencil);
        ms_Resources->Destroy(TextureId::Hzb);
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

        ms_Resources->Create(TextureId::Hzb, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(TextureId::Hzb),
            .Format = benzin::GraphicsFormat::R32Float,
            .Width = ms_RenderViewportWidth,
            .Height = ms_RenderViewportHeight,
            .MipCount = 0, // All mip levels
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });

        auto& stats = ms_Settings->GetSection<GBufferStats>();
        stats.m_ViewportPixelCount = ms_RenderViewportWidth * ms_RenderViewportHeight;
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

        cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::GeometryPass_Vertex));

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);
        cmdList.GetD3D12GraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        cmdList.SetVertexBuffer(*ms_Scene->m_VertexBuffer);
        cmdList.SetIndexBuffer(*ms_Scene->m_IndexBuffer);

        using Resources = joint::GeometryResources;

        cmdList.SetGraphicsRootResource(*Resources::MeshDrawParts, ms_Scene->m_MeshDrawPartBuffer->GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::MeshDraws, ms_Scene->m_MeshDrawBuffer->GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::Materials, ms_Scene->m_MaterialBuffer->GetSrv());

        if (ms_Settings->GetSection<GBufferSettings>().m_IsIndirectDrawEnabled)
        {
            cmdList.GetD3D12GraphicsCommandList()->ExecuteIndirect(
                m_D3D12DrawIndexedIndirectCmdSignature,
                (uint32_t)ms_Scene->m_MeshDrawIndirectCmdBuffer->GetElementCount(),
                ms_Scene->m_MeshDrawIndirectCmdBuffer->GetD3D12Resource(),
                0,
                nullptr,
                0);
        }
        else
        {
            for (uint32_t drawIndex = 0; drawIndex < ms_Scene->m_MeshDraws.size(); ++drawIndex)
            {
                const benzin::MeshDraw& draw = ms_Scene->m_MeshDraws[drawIndex];

                cmdList.SetGraphicsRootConstant(*Resources::MeshDrawIndex, drawIndex);

                const uint32_t drawPartOffset = ms_Scene->m_MeshRanges[draw.m_MeshRangeIndex].m_DrawPartOffset;
                const uint32_t drawPartCount = ms_Scene->m_MeshRanges[draw.m_MeshRangeIndex].m_DrawPartCount;

                for (uint32_t drawPartIndex = drawPartOffset; drawPartIndex < drawPartOffset + drawPartCount; ++drawPartIndex)
                {
                    const benzin::MeshDrawPart& drawPart = ms_Scene->m_MeshDrawParts[drawPartIndex];
                    const benzin::MeshPart& part = ms_Scene->m_MeshParts[drawPart.m_PartIndex];

                    cmdList.SetGraphicsRootConstant(*Resources::MeshDrawPartIndex, drawPartIndex);

                    cmdList.GetD3D12GraphicsCommandList()->IASetPrimitiveTopology(part.m_D3D12PrimitiveTopology);
                    cmdList.DrawIndexed(part.m_IndexCount, part.m_IndexOffset, part.m_VertexOffset);
                }
            }
        }
    }

    void GeometryPass::CreateGeometryPso(PsoId id, benzin::EnumFlags<PsoFlag> flags)
    {
        const auto setWriteDepth = [flags](auto& outProxy)
        {
            if (flags.IsSet(PsoFlag::AlphaTest))
            {
                outProxy.Ps.Defines.push_back("ALPHA_TEST");
            }

            outProxy.DepthState.IsEnabled = true;
            outProxy.DepthState.IsWriteEnabled = true;
            outProxy.DepthState.ComparisonFunction = benzin::ComparisonFunction::Greater;
        };

        const auto setReadDepth = [](auto& outProxy)
        {
            outProxy.DepthState.IsEnabled = true;
            outProxy.DepthState.IsWriteEnabled = false;
            outProxy.DepthState.ComparisonFunction = benzin::ComparisonFunction::Equal;
        };

        const auto configureGraphicsPsoProxy = [this, &setReadDepth, &setWriteDepth, flags](auto& outProxy)
        {
            outProxy.Ps.FileName = "geometry_pass.hlsl";

            outProxy.RasterizerState.CullMode = benzin::CullMode::Back;
            outProxy.RasterizerState.IsIndexOrderClockwise = true;

            outProxy.DepthStencilFormat = GBufferSettings::s_DepthStencilFormat;
            outProxy.RenderTargetFormats.reserve(5);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color0Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color1Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color2Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color3Format);
            outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color4Format);

            setWriteDepth(outProxy);
        };

        if (flags.IsSet(PsoFlag::MeshPipeline))
        {
        #if 0
            ms_PsoManager->Create(id, [this, &configureGraphicsPsoProxy, flags](benzin::MeshPsoProxy& outProxy)
            {
                outProxy.Ms.FileName = "geometry_pass.hlsl";

                if (m_IsAmplificationDispatchUsed)
                {
                    outProxy.As.FileName = "geometry_pass.hlsl";
                    outProxy.Ms.Defines.push_back("AMPLIFICATION_DISPATCH");
                }

                configureGraphicsPsoProxy(outProxy);
            });
        #endif
        }
        else
        {
            ms_PsoManager->Create(id, [this, &configureGraphicsPsoProxy](benzin::VertexPsoProxy& outProxy)
            {
                outProxy.InputLayout.emplace_back("Position", benzin::GraphicsFormat::Rgb32Float);
                outProxy.InputLayout.emplace_back("Normal", benzin::GraphicsFormat::Rgb32Float);
                outProxy.InputLayout.emplace_back("Uv", benzin::GraphicsFormat::Rg32Float);

                BenzinAssert(benzin::GetFormatSizeInBytes(outProxy.InputLayout[0].Format) == sizeof(joint::MeshVertex::Position));
                BenzinAssert(benzin::GetFormatSizeInBytes(outProxy.InputLayout[1].Format) == sizeof(joint::MeshVertex::Normal));
                BenzinAssert(benzin::GetFormatSizeInBytes(outProxy.InputLayout[2].Format) == sizeof(joint::MeshVertex::Uv));

                outProxy.Vs.FileName = "geometry_pass.hlsl";

                configureGraphicsPsoProxy(outProxy);
            });
        }
    }

}
