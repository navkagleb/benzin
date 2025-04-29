#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/geometry_pass.hpp>

#include <benzin/core/engine_math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/pso.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/geometry_resources.hpp>
#include <shaders/joint/mesh_types.hpp>

#include <sandbox/render_passes/gbuffer.hpp>
#include <sandbox/resources.hpp>
#include <sandbox/sandbox_render_settings.hpp>

BenzinEnableUnaryPlusForEnum(joint::GeometryResources);
BenzinEnableFlagsForEnum(sandbox::GeometryPass::PsoFlag);

namespace sandbox
{

    GeometryPass::GeometryPass()
    {
        CreatePso(PsoId::GeometryPass_Depth_Clockwise, PsoFlag::DepthPrePass | PsoFlag::IndexOrderClockwise);
        CreatePso(PsoId::GeometryPass_Depth_CounterClockwise, PsoFlag::DepthPrePass);
        CreatePso(PsoId::GeometryPass_Clockwise, PsoFlag::IndexOrderClockwise);
        CreatePso(PsoId::GeometryPass_CounterClockwise);

        CreatePso(PsoId::GeometryPass_Meshlet_Depth_Clockwise, PsoFlag::Mesh | PsoFlag::DepthPrePass | PsoFlag::IndexOrderClockwise);
        CreatePso(PsoId::GeometryPass_Meshlet_Depth_CounterClockwise, PsoFlag::Mesh | PsoFlag::DepthPrePass);
        CreatePso(PsoId::GeometryPass_Meshlet_Clockwise, PsoFlag::Mesh | PsoFlag::IndexOrderClockwise);
        CreatePso(PsoId::GeometryPass_Meshlet_CounterClockwise, PsoFlag::Mesh);
    }

    GeometryPass::~GeometryPass()
    {
        ms_PsoManager->Destroy(PsoId::GeometryPass_Depth_Clockwise);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Depth_CounterClockwise);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Clockwise);
        ms_PsoManager->Destroy(PsoId::GeometryPass_CounterClockwise);

        ms_PsoManager->Destroy(PsoId::GeometryPass_Meshlet_Depth_Clockwise);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Meshlet_Depth_CounterClockwise);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Meshlet_Clockwise);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Meshlet_CounterClockwise);

        ms_Resources->Destroy(TextureId::AlbedoAndRoughness);
        ms_Resources->Destroy(TextureId::EmissiveAndMetallic);
        ms_Resources->Destroy(TextureId::WorldNormal);
        ms_Resources->Destroy(TextureId::Mv);
        ms_Resources->Destroy(TextureId::ViewDepth);
        ms_Resources->Destroy(TextureId::DepthStencil);
    }

    void GeometryPass::CreatePso(PsoId id, benzin::EnumFlags<PsoFlag> flags)
    {
        const auto setWriteDepth = [](auto& outProxy)
        {
            outProxy.Ps.Defines.push_back("IS_ALPHA_TEST_ENABLED");

            outProxy.DepthState.IsEnabled = true;
            outProxy.DepthState.IsWriteEnabled = true;
            outProxy.DepthState.ComparisonFunction = benzin::ComparisonFunction::Less;
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
            outProxy.RasterizerState.IsIndexOrderClockwise = flags.IsSet(PsoFlag::IndexOrderClockwise);

            outProxy.DepthStencilFormat = GBufferSettings::s_DepthStencilFormat;

            if (flags.IsSet(PsoFlag::DepthPrePass))
            {
                outProxy.Ps.Defines.push_back("IS_DEPTH_PREPASS");

                setWriteDepth(outProxy);
            }
            else
            {
                outProxy.RenderTargetFormats.reserve(5);
                outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color0Format);
                outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color1Format);
                outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color2Format);
                outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color3Format);
                outProxy.RenderTargetFormats.push_back(GBufferSettings::s_Color4Format);

                if (m_IsDepthPrePassEnabled)
                {
                    setReadDepth(outProxy);
                }
                else
                {
                    setWriteDepth(outProxy);
                }
            }
        };

        if (flags.IsSet(PsoFlag::Mesh))
        {
            ms_PsoManager->Create(id, [&configureGraphicsPsoProxy](benzin::MeshPsoProxy& outProxy)
            {
                outProxy.Ms.FileName = "geometry_pass.hlsl";

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


    void GeometryPass::SetPso(benzin::GraphicsCmdList& cmdList, benzin::PsoId meshId, benzin::PsoId vertexId) const
    {
        if (m_IsMeshPipelineUsed)
        {
            cmdList.SetMeshPso(ms_PsoManager->GetMesh(meshId));
        }
        else
        {
            cmdList.SetVertexPso(ms_PsoManager->GetVertex(vertexId));
        }
    }

    void GeometryPass::OnRenderViewportResize()
    {
        const auto createGBufferTexture = [](TextureId id, benzin::GraphicsFormat format, benzin::TextureAccessFlag accessFlag)
        {
            ms_Resources->Create(id, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(id),
                .Format = format,
                .Width = GetRenderViewportWidth(),
                .Height = GetRenderViewportHeight(),
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
            .Width = GetRenderViewportWidth(),
            .Height = GetRenderViewportHeight(),
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowRenderTarget | benzin::TextureAccessFlag::AllowUnorderedAccess,
            .ClearValueVariant = DirectX::XMFLOAT4{ std::numeric_limits<float>::max(), 0.0f, 0.0f, 0.0f }, // R32 max value
        });
    }

    void GeometryPass::OnUpdate()
    {
        const auto& settings = ms_Settings->GetSection<GBufferSettings>();

        if (m_IsDepthPrePassEnabled != settings.IsDepthPrePassEnabled)
        {
            m_IsDepthPrePassEnabled = settings.IsDepthPrePassEnabled;

            ms_PsoManager->Destroy(PsoId::GeometryPass_Clockwise);
            ms_PsoManager->Destroy(PsoId::GeometryPass_CounterClockwise);

            ms_PsoManager->Destroy(PsoId::GeometryPass_Meshlet_Clockwise);
            ms_PsoManager->Destroy(PsoId::GeometryPass_Meshlet_CounterClockwise);

            CreatePso(PsoId::GeometryPass_Clockwise, PsoFlag::IndexOrderClockwise);
            CreatePso(PsoId::GeometryPass_CounterClockwise);

            CreatePso(PsoId::GeometryPass_Meshlet_Clockwise, PsoFlag::Mesh | PsoFlag::IndexOrderClockwise);
            CreatePso(PsoId::GeometryPass_Meshlet_CounterClockwise, PsoFlag::Mesh);
        }

        m_IsCpuFrustumCullingEnabled = settings.IsFrustumCullingEnabled;
        m_IsMeshPipelineUsed = settings.IsMeshPipelineUsed;
    }

    bool GeometryPass::IsSphereCulled(
        const DirectX::BoundingSphere& localBoundingSphere,
        const DirectX::XMMATRIX& localToWorldMatrix,
        const DirectX::XMMATRIX& localInstanceMatrix
    ) const
    {
        if (!m_IsCpuFrustumCullingEnabled || localBoundingSphere.Radius == benzin::g_BadBoundingSphereRadius)
        {
            return false;
        }

        DirectX::BoundingSphere worldBoundingSphere;
        localBoundingSphere.Transform(worldBoundingSphere, localInstanceMatrix * localToWorldMatrix);

        return ms_Scene->GetCamera().GetWorldFrustum().Contains(worldBoundingSphere) == DirectX::DISJOINT;
    }

    void GeometryPass::OnRender() const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "Geometry");

        cmdList.SetViewport(ms_RenderViewport);
        cmdList.SetScissorRect(ms_RenderScissorRect);

        cmdList.SetGraphicsRootResource(+joint::GeometryResources::EntityTransforms, ms_Scene->GetEntityTransformBufferSrv());
        cmdList.SetGraphicsRootResource(+joint::GeometryResources::UnifiedMaterials, ms_Scene->GetUnifiedMaterialBufferSrv());

        const GBuffer gbuffer{ *ms_Resources };

        if (m_IsDepthPrePassEnabled)
        {
            BenzinScopeProfile("DepthPrePass");
            BenzinGpuProfile(*ms_GpuProfiler, cmdList, "DepthPrePass");

            const benzin::ScopedResourceBarriers scopeGBufferBarriers = gbuffer.CreateResourceBarriers(cmdList, benzin::ResourceState::DepthWrite, true);

            gbuffer.SetDepthStencilOnly(cmdList);
            gbuffer.ClearDepthStencil(cmdList);

            SetPso(cmdList, PsoId::GeometryPass_Meshlet_Depth_CounterClockwise, PsoId::GeometryPass_Depth_CounterClockwise);
            RenderMeshes(cmdList, false);

            SetPso(cmdList, PsoId::GeometryPass_Meshlet_Depth_Clockwise, PsoId::GeometryPass_Depth_Clockwise);
            RenderMeshes(cmdList, true);
            RenderLights(cmdList);
        }

        {
            BenzinScopeProfile("ColorPass");
            BenzinGpuProfile(*ms_GpuProfiler, cmdList, "ColorPass");

            const benzin::ResourceState depthStencilState = m_IsDepthPrePassEnabled ? benzin::ResourceState::DepthRead : benzin::ResourceState::DepthWrite;
            const benzin::ScopedResourceBarriers scopeGBufferBarriers = gbuffer.CreateResourceBarriers(cmdList, depthStencilState);

            gbuffer.SetRenderTargets(cmdList);
            gbuffer.ClearRenderTargets(cmdList);

            if (!m_IsDepthPrePassEnabled)
            {
                gbuffer.ClearDepthStencil(cmdList);
            }

            SetPso(cmdList, PsoId::GeometryPass_Meshlet_CounterClockwise, PsoId::GeometryPass_CounterClockwise);
            RenderMeshes(cmdList, false);

            SetPso(cmdList, PsoId::GeometryPass_Meshlet_Clockwise, PsoId::GeometryPass_Clockwise);
            RenderMeshes(cmdList, true);
            RenderLights(cmdList);
        }
    }

    void GeometryPass::RenderMeshes(benzin::GraphicsCmdList& cmdList, bool isIndexOrderClockwise) const
    {
        const auto view = ms_Scene->GetEntityRegistry().view<benzin::MeshInstanceComponent, benzin::Transform>();
        for (const entt::entity entityHandle : view)
        {
            const auto& meshInstanceComponent = view.get<benzin::MeshInstanceComponent>(entityHandle);
            const auto& mesh = ms_Scene->GetMeshRegistry().get<benzin::Mesh>(meshInstanceComponent.GetMeshHandle());

            if (mesh.IsIndexOrderClockwise != isIndexOrderClockwise)
            {
                continue;
            }

            const auto& transform = view.get<benzin::Transform>(entityHandle);
            RenderMesh(cmdList, meshInstanceComponent, transform.GetLocalToWorldMatrix());
        }
    }

    void GeometryPass::RenderLights(benzin::GraphicsCmdList& cmdList) const
    {
        const auto view = ms_Scene->GetEntityRegistry().view<benzin::MeshInstanceComponent, benzin::SphericalLight>();
        for (const entt::entity entityHandle : view)
        {
            const auto& light = view.get<benzin::SphericalLight>(entityHandle);

            if (!light.IsEnabled())
            {
                continue;
            }

            const auto& meshInstanceComponent = view.get<benzin::MeshInstanceComponent>(entityHandle);

            const auto& mesh = ms_Scene->GetMeshRegistry().get<benzin::Mesh>(meshInstanceComponent.GetMeshHandle());
            BenzinEnsure(mesh.IsIndexOrderClockwise);

            RenderMesh(cmdList, meshInstanceComponent, light.GetTransform().GetLocalToWorldMatrix());
        }
    }

    void GeometryPass::RenderMesh(benzin::GraphicsCmdList& cmdList, const benzin::MeshInstanceComponent& meshInstanceComponent, const DirectX::XMMATRIX& localToWorldMatrix) const
    {
        using Resources = joint::GeometryResources;

        const entt::entity meshHandle = meshInstanceComponent.GetMeshHandle();
        BenzinAssert(benzin::IsGoodEnum(meshHandle));

        const auto& meshTag = ms_Scene->GetMeshRegistry().get<benzin::MeshTag>(meshHandle);
        const auto& mesh = ms_Scene->GetMeshRegistry().get<benzin::Mesh>(meshHandle);
        const auto& meshGpuStorage = ms_Scene->GetMeshRegistry().get<benzin::MeshGpuStorage>(meshHandle);

        if (IsSphereCulled(mesh.BoundingSphere, localToWorldMatrix))
        {
            return;
        }

        BenzinGpuEvent(cmdList, meshTag);

        cmdList.SetVertexBuffer(*meshGpuStorage.VertexBuffer);
        cmdList.SetIndexBuffer(*meshGpuStorage.IndexBuffer);

        cmdList.SetGraphicsRootConstant(+Resources::EntityTransformIndex, meshInstanceComponent.GetEntityTransformIndex());
        cmdList.SetGraphicsRootResource(+Resources::InstanceTransforms, meshGpuStorage.InstanceTransformBuffer->GetSrv());

        for (const auto& [i, instance] : mesh.Instances | std::views::enumerate)
        {
            const benzin::MeshDrawRange& drawRange = mesh.DrawRanges[instance.DrawRangeIndex];

            if (IsSphereCulled(drawRange.BoundingSphere, localToWorldMatrix, instance.LocalTransform))
            {
                continue;
            }

            cmdList.SetGraphicsRootConstant(+Resources::InstanceTransformIndex, (uint32_t)i);
            cmdList.SetGraphicsRootConstant(+Resources::InstanceMaterialIndex, instance.MaterialIndex);

            if (m_IsMeshPipelineUsed)
            {
                cmdList.SetGraphicsRootResource(+Resources::Vertices, meshGpuStorage.VertexBuffer->GetSrv(drawRange.VertexRange));
                cmdList.SetGraphicsRootResource(+Resources::Meshlets, meshGpuStorage.MeshletBuffer->GetSrv(drawRange.MeshletRange));
                cmdList.SetGraphicsRootResource(+Resources::MeshletIndirectVertices, meshGpuStorage.MeshletIndirectVertexBuffer->GetSrv(drawRange.MeshletIndirectVertexRange));
                cmdList.SetGraphicsRootResource(+Resources::MeshletIndices, meshGpuStorage.MeshletIndexBuffer->GetSrv(drawRange.MeshletIndexRange));

                cmdList.DispatchMesh({ drawRange.MeshletRange.Count, 1, 1 });
            }
            else
            {
                cmdList.SetPrimitiveTopology(drawRange.PrimitiveTopology);
                cmdList.DrawIndexed(drawRange.IndexRange.Count, drawRange.IndexRange.Offset, drawRange.VertexRange.Offset);
            }
        }
    }

}
