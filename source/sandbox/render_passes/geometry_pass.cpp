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

    static bool IsFrustumCulled(
        const DirectX::BoundingFrustum& worldFrustum,
        const DirectX::BoundingSphere& localBoundingSphere,
        const DirectX::XMMATRIX& localToWorldMatrix)
    {
        const DirectX::XMFLOAT3 center = localBoundingSphere.Center;
        if (center.x == 0.0f && center.y == 0.0f && center.z == 0.0f && localBoundingSphere.Radius == 1.0f)
            return false;

        DirectX::BoundingSphere worldBoundingSphere;
        localBoundingSphere.Transform(worldBoundingSphere, localToWorldMatrix);

        return worldFrustum.Contains(worldBoundingSphere) == DirectX::DISJOINT;
    }

    //

    GeometryPass::GeometryPass()
    {
        ms_PsoManager->Create(PsoId::GeometryPass_DepthReprojection, [](benzin::ComputePsoProxy& proxy)
        {
            proxy.Cs.FileName = "depth_reprojection.hlsl";
            proxy.Cs.Defines.push_back("DEPTH_REPROJECTION");
        });

        ms_PsoManager->Create(PsoId::GeometryPass_DepthReduction, [](benzin::ComputePsoProxy& proxy)
        {
            proxy.Cs.FileName = "depth_reprojection.hlsl";
            proxy.Cs.Defines.push_back("DEPTH_REDUCTION");
        });

        CreateGeometryPso(PsoId::GeometryPass_Vertex);
        CreateGeometryPso(PsoId::GeometryPass_Vertex_Alpha, PsoFlag::AlphaTest);
        CreateGeometryPso(PsoId::GeometryPass_Mesh, PsoFlag::MeshPipeline);
        CreateGeometryPso(PsoId::GeometryPass_Mesh_Alpha, PsoFlag::MeshPipeline | PsoFlag::AlphaTest);
    }

    GeometryPass::~GeometryPass()
    {
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

    void GeometryPass::OnUpdate()
    {
        const auto& settings = ms_Settings->GetSection<GBufferSettings>();

        m_Consts.IsFrustumCullingEnabled = settings.IsGpuFrustumCullingEnabled;
        m_Consts.IsBackfaceCullingEnabled = settings.IsBackfaceCullingEnabled;
        m_Consts.IsOcclusionCullingEnabled = settings.IsOcclusionCullingEnabled;
    }

    void GeometryPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("Geometry");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        ReprojectDepth(cmdList);
        GenerateHzb(cmdList);
        RunColorPass(cmdList);
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

    void GeometryPass::ReprojectDepth(benzin::ComputeCmdList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile("ReprojectDepth");

        const benzin::Texture& prevDepth = ms_Resources->GetPrev(TextureId::DepthStencil);
        const benzin::Texture& hzb = ms_Resources->Get(TextureId::Hzb);

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::GeometryPass_DepthReprojection));

        cmdList.AddResourceBarrier(benzin::TransitionBarrier{ hzb, benzin::ResourceState::UnorderedAccess }, true);

        using Resources = joint::DepthResprojectionResources;
        cmdList.SetComputeRootResource(*Resources::PrevDepth, prevDepth.GetSrv());
        cmdList.SetComputeRootResource(*Resources::ReprojectedDepth, hzb.GetUav({ .m_MipIndex = 0 }));

        cmdList.Dispatch({ prevDepth.GetWidth(), prevDepth.GetHeight(), 1}, { 8, 8, 1 });

        cmdList.AddResourceBarrier(benzin::UnorderedAccessBarrier{ hzb }, true);
    }

    void GeometryPass::GenerateHzb(benzin::ComputeCmdList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile("GenerateHzb");

        const benzin::Texture& hzb = ms_Resources->Get(TextureId::Hzb);

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::GeometryPass_DepthReduction));

        for (uint16_t sourceMipIndex = 0; sourceMipIndex < hzb.GetMipCount() - 1; ++sourceMipIndex)
        {
            const uint16_t destMipIndex = sourceMipIndex + 1;
            const uint32_t destMipWidth = hzb.GetMipWidth(destMipIndex);
            const uint32_t destMipHeight = hzb.GetMipHeight(destMipIndex);

            joint::DepthReductionPassConsts consts = {};
            consts.m_DestMipTexelSize = { 1.0f / destMipWidth, 1.0f / destMipHeight };
            consts.m_IsSourceWidthOdd = (hzb.GetMipWidth(sourceMipIndex) & 1) == 1;
            consts.m_IsSourceHeightOdd = (hzb.GetMipHeight(sourceMipIndex) & 1) == 1;

            cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer0, ms_Device->GetConstBufferAllocator().Allocate(consts));

            {
                using Resources = joint::DepthReductionResources;
                cmdList.SetComputeRootResource(*Resources::SourceMip, hzb.GetSrv({ .m_MipOffset = sourceMipIndex, .m_MipCount = 1 }));
                cmdList.SetComputeRootResource(*Resources::DestMip, hzb.GetUav({ .m_MipIndex = destMipIndex }));
            }

            cmdList.Dispatch({ destMipWidth, destMipHeight, 1 }, { 8, 8, 1 });

            cmdList.AddResourceBarrier(benzin::UnorderedAccessBarrier{ hzb }, true);
        }

        cmdList.AddResourceBarrier(benzin::TransitionBarrier{ hzb, benzin::ResourceState::GenericRead }, true);
    }

    void GeometryPass::RunColorPass(benzin::GraphicsCmdList& cmdList) const
    {
        using Resources = joint::GeometryResources;

        BenzinProfile();
        BenzinGpuProfile("ColorPass");

        const GBuffer gbuffer{ *ms_Resources };
        const benzin::ScopedResourceBarriers scopeGBufferBarriers = gbuffer.CreateResourceBarriers(cmdList, benzin::ResourceState::DepthWrite);

        gbuffer.SetRenderTargets(cmdList);
        gbuffer.ClearRenderTargets(cmdList);
        gbuffer.ClearDepthStencil(cmdList);

        cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::GeometryPass_Vertex));

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);

        cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer0, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));

        cmdList.SetVertexBuffer(*ms_Scene->m_VertexBuffer);
        cmdList.SetIndexBuffer(*ms_Scene->m_IndexBuffer);

        cmdList.SetGraphicsRootResource(*Resources::MeshDrawParts, ms_Scene->m_MeshDrawPartBuffer->GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::MeshDraws, ms_Scene->m_MeshDrawBuffer->GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::Materials, ms_Scene->m_MaterialBuffer->GetSrv());

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
