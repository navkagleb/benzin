#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/geometry_pass.hpp>

#include <sandbox/render_passes/gbuffer.hpp>
#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/buffer_writer.hpp>
#include <benzin/core/engine_math.hpp>
#include <benzin/core/math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/light.hpp>
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
        BenzinProfile();

        const auto& settings = ms_Settings->GetSection<GBufferSettings>();

        if (m_IsAmplificationDispatchUsed != settings.IsAmplificationDispatchUsed)
        {
            m_IsAmplificationDispatchUsed = settings.IsAmplificationDispatchUsed;

            ms_PsoManager->Destroy(PsoId::GeometryPass_Mesh);
            ms_PsoManager->Destroy(PsoId::GeometryPass_Mesh_Alpha);

            CreateGeometryPso(PsoId::GeometryPass_Mesh, PsoFlag::MeshPipeline);
            CreateGeometryPso(PsoId::GeometryPass_Mesh_Alpha, PsoFlag::MeshPipeline | PsoFlag::AlphaTest);
        }

        m_Consts.ColoringType = settings.ColoringType;
        m_Consts.IsFrustumCullingEnabled = settings.IsGpuFrustumCullingEnabled;
        m_Consts.IsBackfaceCullingEnabled = settings.IsBackfaceCullingEnabled;
        m_Consts.IsOcclusionCullingEnabled = settings.IsOcclusionCullingEnabled;

        CreateMeshBatches();
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

                outProxy.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle;

                configureGraphicsPsoProxy(outProxy);
            });
        }
    }

    void GeometryPass::CreateMeshBatches()
    {
        BenzinProfile();

        m_OpaqueMeshBatches.clear();
        m_AlphaMeshBatches.clear();

        m_TotalInstanceCount = 0;
        m_InstanceOffset = 0;

        m_BatchStorage.m_LocalToWorldMatrixBuffer.reset();
        m_BatchStorage.m_PrevLocalToWorldMatrixBuffer.reset();
        m_BatchStorage.m_MaterialIndexBuffer.reset();

        const auto entityView = ms_Scene->GetEntityRegistry().view<benzin::MeshComponent, benzin::Transform>();
        for (const entt::entity entityHandle : entityView)
        {
            const auto& meshComponent = entityView.get<benzin::MeshComponent>(entityHandle);
            const auto& transform = entityView.get<benzin::Transform>(entityHandle);

            AddToTempMeshBatches(meshComponent, transform);
        }

        const auto lightView = ms_Scene->GetEntityRegistry().view<benzin::MeshComponent, benzin::SphericalLight>();
        for (const entt::entity lightHandle : lightView)
        {
            const auto& meshComponent = lightView.get<benzin::MeshComponent>(lightHandle);
            const auto& sphericalLight = lightView.get<benzin::SphericalLight>(lightHandle);
        
            if (!sphericalLight.IsEnabled())
            {
                continue;
            }
        
            AddToTempMeshBatches(meshComponent, sphericalLight.GetTransform());
        }

        if (m_TotalInstanceCount != 0)
        {
            benzin::GpuHeapLinearBufferAllocator& allocator = ms_Device->GetTemporalLinearAllocator();

            m_BatchStorage.m_LocalToWorldMatrixBuffer = allocator.AllocateStructuredBuffer(
                "Batch_LocalToWorldMatrices",
                m_TotalInstanceCount,
                sizeof(DirectX::XMMATRIX));

            m_BatchStorage.m_PrevLocalToWorldMatrixBuffer = allocator.AllocateStructuredBuffer(
                "Batch_PrevLocalToWorldMatrices",
                m_TotalInstanceCount,
                sizeof(DirectX::XMMATRIX));

            m_BatchStorage.m_MaterialIndexBuffer = allocator.AllocateFormatBuffer(
                "Batch_MaterialIndices",
                m_TotalInstanceCount,
                benzin::GraphicsFormat::R32Uint);

            ProcessTempMeshBatches(m_TempOpaqueMeshBatches, m_OpaqueMeshBatches);
            ProcessTempMeshBatches(m_TempAlphaMeshBatches, m_AlphaMeshBatches);
        }

        m_TempOpaqueMeshBatches.clear();
        m_TempAlphaMeshBatches.clear();
    }

    void GeometryPass::AddToTempMeshBatches(const benzin::MeshComponent& meshComponent, const benzin::Transform& transform)
    {
        BenzinProfile();

#if 0
        const bool isCpuFrustumCullingEnabled = ms_Settings->GetSection<GBufferSettings>().IsCpuFrustumCullingEnabled;
        const DirectX::BoundingFrustum& worldFrustum = ms_Scene->GetCamera().GetWorldFrustum();
#endif

        const entt::entity meshHandle = meshComponent.GetMeshHandle();
        const benzin::Mesh& mesh = ms_Scene->GetMeshRegistry().view<benzin::Mesh>().get<benzin::Mesh>(meshHandle);

        for (const benzin::MeshInstance& instance : mesh.m_Instances)
        {
            const DirectX::XMMATRIX localToWorldMatrix = instance.m_ObjectToLocalMatrix * transform.GetLocalToWorldMatrix();

#if 0
            const DirectX::BoundingSphere& boundingSphere = mesh.DrawRanges[instance.DrawRangeIndex].BoundingSphere;
            if (isCpuFrustumCullingEnabled && IsFrustumCulled(worldFrustum, boundingSphere, localToWorldMatrix))
            {
                continue;
            }
#endif

            const benzin::Material& material = ms_Scene->GetMaterial(instance.m_MaterialIndex);
            
            TempMeshBatches& meshBatches = material.Consts.m_IsAlphaTestRequired ? m_TempAlphaMeshBatches : m_TempOpaqueMeshBatches;
            TempDrawRangeBatch& batch = meshBatches[meshHandle][instance.m_DrawRangeIndex];

            batch.m_LocalToWorldMatrices.push_back(localToWorldMatrix);
            batch.m_PrevLocalToWorldMatrices.push_back(instance.m_ObjectToLocalMatrix * transform.GetPrevLocalToWorldMatrix());
            batch.m_MaterialIndices.push_back(instance.m_MaterialIndex);

            ++m_TotalInstanceCount;
        }
    }

    void GeometryPass::ProcessTempMeshBatches(TempMeshBatches& tempMeshBatches, MeshBatches& outMeshBatches)
    {
        BenzinProfile();

        benzin::BufferWriter localToWorldMatrixWriter = benzin::MakeBufferWriter(*m_BatchStorage.m_LocalToWorldMatrixBuffer);
        benzin::BufferWriter prevLocalToWorldMatrixWriter = benzin::MakeBufferWriter(*m_BatchStorage.m_PrevLocalToWorldMatrixBuffer);
        benzin::BufferWriter materialIndexWriter = benzin::MakeBufferWriter(*m_BatchStorage.m_MaterialIndexBuffer);

        for (const auto& [meshHandle, tempMeshBatch] : tempMeshBatches)
        {
            for (const auto& [drawRangeIndex, tempDrawRangeBatch] : tempMeshBatch)
            {
                BenzinAssert(tempDrawRangeBatch.m_LocalToWorldMatrices.size() == tempDrawRangeBatch.m_PrevLocalToWorldMatrices.size());
                BenzinAssert(tempDrawRangeBatch.m_LocalToWorldMatrices.size() == tempDrawRangeBatch.m_MaterialIndices.size());
                
                const uint32_t instanceCount = (uint32_t)tempDrawRangeBatch.m_LocalToWorldMatrices.size();

                DrawRangeBatch& drawRangeBatch = outMeshBatches[meshHandle][drawRangeIndex];
                drawRangeBatch.m_InstanceRange.m_Offset = m_InstanceOffset;
                drawRangeBatch.m_InstanceRange.m_Count = instanceCount;

                localToWorldMatrixWriter.SetElementPosition<DirectX::XMMATRIX>(m_InstanceOffset);
                prevLocalToWorldMatrixWriter.SetElementPosition<DirectX::XMMATRIX>(m_InstanceOffset);
                materialIndexWriter.SetElementPosition<uint32_t>(m_InstanceOffset);

                localToWorldMatrixWriter.WriteArray(benzin::ToSpan(tempDrawRangeBatch.m_LocalToWorldMatrices));
                prevLocalToWorldMatrixWriter.WriteArray(benzin::ToSpan(tempDrawRangeBatch.m_PrevLocalToWorldMatrices));
                materialIndexWriter.WriteArray(benzin::ToSpan(tempDrawRangeBatch.m_MaterialIndices));

                m_InstanceOffset += instanceCount;
            }
        }

        tempMeshBatches.clear();
    }

    void GeometryPass::RenderMeshBatches(benzin::GraphicsCmdList& cmdList, const MeshBatches& meshBatches) const
    {
        using Resources = joint::GeometryResources;

        const bool isMeshPipelineUsed = ms_Settings->GetSection<GBufferSettings>().IsMeshPipelineUsed;
        const bool isAmplificationDispatchUsed = ms_Settings->GetSection<GBufferSettings>().IsAmplificationDispatchUsed;

        const auto meshView = ms_Scene->GetMeshRegistry().view<benzin::MeshTag, benzin::Mesh, benzin::MeshGpuStorage>();

        for (const auto& [meshHandle, meshBatch] : meshBatches)
        {
            const auto& meshTag = meshView.get<benzin::MeshTag>(meshHandle);
            const auto& mesh = meshView.get<benzin::Mesh>(meshHandle);
            const auto& meshGpuStorage = meshView.get<benzin::MeshGpuStorage>(meshHandle);

            BenzinGpuEvent(meshTag);

            if (!isMeshPipelineUsed)
            {
                cmdList.SetVertexBuffer(*meshGpuStorage.VertexBuffer);
                cmdList.SetIndexBuffer(*meshGpuStorage.IndexBuffer);
            }

            for (const auto& [drawRangeIndex, drawRangeBatch] : meshBatch)
            {
                const benzin::MeshDrawRange& drawRange = mesh.m_DrawRanges[drawRangeIndex];

                cmdList.SetGraphicsRootResource(*Resources::Batch_LocalToWorldMatrices, m_BatchStorage.m_LocalToWorldMatrixBuffer->GetSrv(drawRangeBatch.m_InstanceRange));
                cmdList.SetGraphicsRootResource(*Resources::Batch_PrevLocalToWorldMatrices, m_BatchStorage.m_PrevLocalToWorldMatrixBuffer->GetSrv(drawRangeBatch.m_InstanceRange));
                cmdList.SetGraphicsRootResource(*Resources::Batch_MaterialIndices, m_BatchStorage.m_MaterialIndexBuffer->GetSrv(drawRangeBatch.m_InstanceRange));

                if (isMeshPipelineUsed)
                {
                    cmdList.SetGraphicsRootResource(*Resources::Vertices, meshGpuStorage.VertexBuffer->GetSrv(drawRange.m_VertexRange));
                    cmdList.SetGraphicsRootResource(*Resources::Meshlets, meshGpuStorage.MeshletBuffer->GetSrv(drawRange.m_MeshletRange));
                    cmdList.SetGraphicsRootResource(*Resources::MeshletCullVolumes, meshGpuStorage.MeshletCullVolumeBuffer->GetSrv(drawRange.m_MeshletRange));
                    cmdList.SetGraphicsRootResource(*Resources::MeshletIndirectVertices, meshGpuStorage.MeshletIndirectVertexBuffer->GetSrv(drawRange.m_MeshletIndirectVertexRange));
                    cmdList.SetGraphicsRootResource(*Resources::MeshletIndices, meshGpuStorage.MeshletIndexBuffer->GetSrv(drawRange.m_MeshletIndexRange));
                    cmdList.SetGraphicsRootConstant(*Resources::MeshletCountPerInstance, drawRange.m_MeshletRange.m_Count);

                    if (isAmplificationDispatchUsed)
                    {
                        const uint32_t totalMeshletCount = drawRange.m_MeshletRange.m_Count * drawRangeBatch.m_InstanceRange.m_Count;

                        cmdList.SetGraphicsRootConstant(*Resources::TotalMeshletCount, totalMeshletCount);
                        cmdList.DispatchMesh({ totalMeshletCount, 1, 1 }, { *joint::MeshletConsts::AsGroupSize, 1, 1 });
                    }
                    else
                    {
                        cmdList.SetGraphicsRootConstant(*Resources::TotalMeshletCount, drawRange.m_MeshletRange.m_Count);

                        for (uint32_t instanceIndex = 0; instanceIndex < drawRangeBatch.m_InstanceRange.m_Count; ++instanceIndex)
                        {
                            cmdList.SetGraphicsRootConstant(*Resources::InstanceIndex, instanceIndex);
                            cmdList.DispatchMesh({ drawRange.m_MeshletRange.m_Count, 1, 1 });
                        }
                    }
                }
                else
                {
                    for (uint32_t instanceIndex = 0; instanceIndex < drawRangeBatch.m_InstanceRange.m_Count; ++instanceIndex)
                    {
                        cmdList.SetGraphicsRootConstant(*Resources::InstanceIndex, instanceIndex);

                        cmdList.SetPrimitiveTopology(drawRange.m_Topology);
                        cmdList.DrawIndexed(drawRange.m_IndexRange.m_Count, drawRange.m_IndexRange.m_Offset, drawRange.m_VertexRange.m_Offset);
                    }
                }
            }
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
        cmdList.SetComputeRootResource(*Resources::ReprojectedDepth, hzb.GetUav({ .MipIndex = 0 }));

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
                cmdList.SetComputeRootResource(*Resources::SourceMip, hzb.GetSrv({ .MipRange = sourceMipIndex }));
                cmdList.SetComputeRootResource(*Resources::DestMip, hzb.GetUav({ .MipIndex = destMipIndex }));
            }

            cmdList.Dispatch({ destMipWidth, destMipHeight, 1 }, { 8, 8, 1 });

            cmdList.AddResourceBarrier(benzin::UnorderedAccessBarrier{ hzb }, true);
        }

        cmdList.AddResourceBarrier(benzin::TransitionBarrier{ hzb, benzin::ResourceState::GenericRead }, true);
    }

    void GeometryPass::RunColorPass(benzin::GraphicsCmdList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile("ColorPass");

        const GBuffer gbuffer{ *ms_Resources };
        const benzin::ScopedResourceBarriers scopeGBufferBarriers = gbuffer.CreateResourceBarriers(cmdList, benzin::ResourceState::DepthWrite);

        gbuffer.SetRenderTargets(cmdList);
        gbuffer.ClearRenderTargets(cmdList);
        gbuffer.ClearDepthStencil(cmdList);

#if 0 // TODO !!!
        for (uint16_t mipIndex = 0; mipIndex < hzb.GetMipCount(); ++mipIndex)
        {
            cmdList.ClearUnorderedAccess(hzb, hzb.GetUav({ .MipIndex = mipIndex }), {});
        }
#endif

        const benzin::Texture& reprojectedHzb = ms_Resources->Get(TextureId::Hzb);

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);

        cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer0, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));

        using Resources = joint::GeometryResources;
        cmdList.SetGraphicsRootResource(*Resources::UnifiedMaterials, ms_Scene->GetUnifiedMaterialBuffer().GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::ReprojectedHzb, reprojectedHzb.GetSrv());

        const auto setPso = [&cmdList](PsoId meshId, PsoId vertexId)
        {
            const bool isMeshPipelineUsed = ms_Settings->GetSection<GBufferSettings>().IsMeshPipelineUsed;
            if (isMeshPipelineUsed)
            {
                cmdList.SetMeshPso(ms_PsoManager->GetMesh(meshId));
            }
            else
            {
                cmdList.SetVertexPso(ms_PsoManager->GetVertex(vertexId));
            }
        };

        setPso(PsoId::GeometryPass_Mesh_Alpha, PsoId::GeometryPass_Vertex_Alpha);
        RenderMeshBatches(cmdList, m_AlphaMeshBatches);

        setPso(PsoId::GeometryPass_Mesh, PsoId::GeometryPass_Vertex);
        RenderMeshBatches(cmdList, m_OpaqueMeshBatches);
    }

}
