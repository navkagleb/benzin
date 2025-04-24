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

namespace sandbox
{

    GeometryPass::GeometryPass()
    {
        using enum benzin::IndexOrder;

        CreatePso(PsoId::GeometryPass_DepthClockwise, Clockwise, true);
        CreatePso(PsoId::GeometryPass_DepthCounterClockwise, CounterClockwise, true);

        CreatePso(PsoId::GeometryPass_Clockwise, Clockwise, false);
        CreatePso(PsoId::GeometryPass_CounterClockwise, CounterClockwise, false);
    }

    GeometryPass::~GeometryPass()
    {
        ms_PsoManager->Destroy(PsoId::GeometryPass_DepthClockwise);
        ms_PsoManager->Destroy(PsoId::GeometryPass_DepthCounterClockwise);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Clockwise);
        ms_PsoManager->Destroy(PsoId::GeometryPass_CounterClockwise);

        ms_Resources->Destroy(TextureId::AlbedoAndRoughness);
        ms_Resources->Destroy(TextureId::EmissiveAndMetallic);
        ms_Resources->Destroy(TextureId::WorldNormal);
        ms_Resources->Destroy(TextureId::Mv);
        ms_Resources->Destroy(TextureId::ViewDepth);
        ms_Resources->Destroy(TextureId::DepthStencil);
    }

    void GeometryPass::CreatePso(PsoId id, benzin::IndexOrder indexOrder, bool isDepthPrePass)
    {
        ms_PsoManager->Create(id, [this, indexOrder, isDepthPrePass](benzin::VertexPsoProxy& proxy)
        {
            proxy.InputLayout.emplace_back("Position", benzin::GraphicsFormat::Rgb32Float);
            proxy.InputLayout.emplace_back("Normal", benzin::GraphicsFormat::Rgb32Float);
            proxy.InputLayout.emplace_back("Uv", benzin::GraphicsFormat::Rg32Float);

            BenzinAssert(benzin::GetFormatSizeInBytes(proxy.InputLayout[0].Format) == sizeof(joint::MeshVertex::Position));
            BenzinAssert(benzin::GetFormatSizeInBytes(proxy.InputLayout[1].Format) == sizeof(joint::MeshVertex::Normal));
            BenzinAssert(benzin::GetFormatSizeInBytes(proxy.InputLayout[2].Format) == sizeof(joint::MeshVertex::Uv));

            proxy.Vs.FileName = "geometry_pass.hlsl";
            proxy.Ps.FileName = "geometry_pass.hlsl";

            proxy.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle;
            proxy.RasterizerState.CullMode = benzin::CullMode::Back;
            proxy.RasterizerState.IndexOrder = indexOrder;
            proxy.DepthState.IsEnabled = true;
            proxy.DepthStencilFormat = GBufferSettings::s_DepthStencilFormat;

            const auto setWriteDepth = [&proxy]
            {
                proxy.Ps.Defines.push_back("IS_ALPHA_TEST_ENABLED");

                proxy.DepthState.IsWriteEnabled = true;
                proxy.DepthState.ComparisonFunction = benzin::ComparisonFunction::Less;
            };

            const auto setReadDepth = [&proxy]
            {
                proxy.DepthState.IsWriteEnabled = false;
                proxy.DepthState.ComparisonFunction = benzin::ComparisonFunction::Equal;
            };

            if (isDepthPrePass)
            {
                proxy.Ps.Defines.push_back("IS_DEPTH_PREPASS");

                setWriteDepth();
            }
            else
            {
                proxy.RenderTargetFormats.reserve(5);
                proxy.RenderTargetFormats.push_back(GBufferSettings::s_Color0Format);
                proxy.RenderTargetFormats.push_back(GBufferSettings::s_Color1Format);
                proxy.RenderTargetFormats.push_back(GBufferSettings::s_Color2Format);
                proxy.RenderTargetFormats.push_back(GBufferSettings::s_Color3Format);
                proxy.RenderTargetFormats.push_back(GBufferSettings::s_Color4Format);

                if (m_IsDepthPrePassEnabled)
                {
                    setReadDepth();
                }
                else
                {
                    setWriteDepth();
                }
            }
        });
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

            CreatePso(PsoId::GeometryPass_Clockwise, benzin::IndexOrder::Clockwise, false);
            CreatePso(PsoId::GeometryPass_CounterClockwise, benzin::IndexOrder::CounterClockwise, false);
        }
    }

    void GeometryPass::OnRender() const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "Geometry");

        MeshRenderContext context
        {
            .CmdList = cmdList,
            .WorldFrustum = ms_Scene->GetCamera().GetWorldFrustum(),
            .EntityRegistry = ms_Scene->GetEntityRegistry(),
            .MeshRegistry = ms_Scene->GetMeshRegistry(),
            .Settings = ms_Settings->GetSection<GBufferSettings>(),
            .Stats = ms_Settings->GetSection<GBufferStats>(),
        };
        context.Stats = {};

        cmdList.SetViewport(ms_RenderViewport);
        cmdList.SetScissorRect(ms_RenderScissorRect);
        cmdList.SetGraphicsRootResource(+joint::GeometryResources::MeshTransforms, ms_Scene->GetTransformBufferSrv());

        const GBuffer gbuffer{ *ms_Resources };

        {
            BenzinScopeProfile("DepthPrePass");
            BenzinGpuProfile(*ms_GpuProfiler, cmdList, "DepthPrePass");

            BenzinScopedResourceBarriers(
                cmdList,
                benzin::TransitionBarrier{ gbuffer.DepthStencil, benzin::ResourceState::DepthWrite }
            );

            cmdList.ClearDepthStencil(gbuffer.DepthStencil);

            if (m_IsDepthPrePassEnabled)
            {
                context.IsDepthPrePass = true;

                cmdList.SetRenderTargets({}, &ms_Resources->Get(TextureId::DepthStencil).GetDsv());
                cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::GeometryPass_DepthCounterClockwise));
                RenderMeshes(context, (bool)benzin::IndexOrder::CounterClockwise);

                cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::GeometryPass_DepthClockwise));
                RenderMeshes(context, (bool)benzin::IndexOrder::Clockwise);
                RenderLights(context);
            }
        }

        {
            BenzinScopeProfile("ColorPass");
            BenzinGpuProfile(*ms_GpuProfiler, cmdList, "ColorPass");

            context.IsDepthPrePass = false;

            const benzin::ResourceState depthStencilState = m_IsDepthPrePassEnabled ? benzin::ResourceState::DepthRead : benzin::ResourceState::DepthWrite;
            const benzin::ScopedResourceBarriers scopeGBufferBarriers = gbuffer.CreateResourceBarriers(cmdList, depthStencilState);

            gbuffer.SetRenderTargets(cmdList);

            cmdList.ClearRenderTarget(gbuffer.AlbedoAndRoughness);
            cmdList.ClearRenderTarget(gbuffer.EmissiveAndMetallic);
            cmdList.ClearRenderTarget(gbuffer.WorldNormal);
            cmdList.ClearRenderTarget(gbuffer.Mv);
            cmdList.ClearRenderTarget(gbuffer.ViewDepth);

            cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::GeometryPass_CounterClockwise));
            RenderMeshes(context, (bool)benzin::IndexOrder::CounterClockwise);

            cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::GeometryPass_Clockwise));
            RenderMeshes(context, (bool)benzin::IndexOrder::Clockwise);
            RenderLights(context);
        }
    }

    void GeometryPass::RenderMeshes(const MeshRenderContext& context, bool isIndexOrderClockwise) const
    {
        const auto view = context.EntityRegistry.view<benzin::MeshComponent, benzin::Transform>();
        for (const auto entity : view)
        {
            const auto& meshComponent = view.get<benzin::MeshComponent>(entity);
            const auto& mesh = context.MeshRegistry.get<benzin::Mesh>(meshComponent.MeshHandle);

            if (mesh.IsIndexOrderClockwise == isIndexOrderClockwise)
            {
                const auto& transform = view.get<benzin::Transform>(entity);
                RenderMesh(context, meshComponent, transform.GetLocalToWorldMatrix());
            }
        }
    }

    void GeometryPass::RenderLights(const MeshRenderContext& context) const
    {
        const auto view = context.EntityRegistry.view<benzin::MeshComponent, benzin::SphericalLight>();
        for (const auto entityHandle : view)
        {
            const auto& light = view.get<benzin::SphericalLight>(entityHandle);

            if (!light.IsEnabled())
            {
                continue;
            }

            const auto& meshComponent = view.get<benzin::MeshComponent>(entityHandle);

            const auto& mesh = context.MeshRegistry.get<benzin::Mesh>(meshComponent.MeshHandle);
            BenzinEnsure(mesh.IsIndexOrderClockwise);

            RenderMesh(context, meshComponent, light.GetTransform().GetLocalToWorldMatrix());
        }
    }

    void GeometryPass::RenderMesh(const MeshRenderContext& context, const benzin::MeshComponent& meshComponent, const DirectX::XMMATRIX& localToWorldMatrix) const
    {
        if (!benzin::IsGoodEnum(meshComponent.MeshHandle))
        {
            return;
        }

        const std::string_view meshName = context.MeshRegistry.get<std::string>(meshComponent.MeshHandle);
        BenzinGpuEvent(context.CmdList, meshName);

        const auto& meshGpuStorage = context.MeshRegistry.get<benzin::MeshGpuStorage>(meshComponent.MeshHandle);
        context.CmdList.SetVertexBuffer(*meshGpuStorage.VertexBuffer);
        context.CmdList.SetIndexBuffer(*meshGpuStorage.IndexBuffer);
        context.CmdList.SetGraphicsRootConstant(+joint::GeometryResources::MeshTransformIndex, meshComponent.GpuTransformIndex);
        context.CmdList.SetGraphicsRootResource(+joint::GeometryResources::SubMeshInstances, meshGpuStorage.MeshInstanceBuffer->GetSrv());
        context.CmdList.SetGraphicsRootResource(+joint::GeometryResources::Materials, meshGpuStorage.MaterialBuffer->GetSrv());

        const auto& mesh = context.MeshRegistry.get<benzin::Mesh>(meshComponent.MeshHandle);
        for (const auto& [i, meshInstance] : mesh.SubMeshInstances | std::views::enumerate)
        {
            context.Stats.MeshCount++;

            const benzin::MeshData& subMesh = mesh.SubMeshes[meshInstance.SubMeshIndex];

            if (context.Settings.IsFrustumCullingEnabled && subMesh.BoundingBox.has_value())
            {
                const auto worldBoundingBox = benzin::TransformBoundingBox(*subMesh.BoundingBox, meshInstance.Transform * localToWorldMatrix);
                if (context.WorldFrustum.Contains(worldBoundingBox) == DirectX::DISJOINT)
                {
                    continue;
                }
            }

            context.CmdList.SetGraphicsRootConstant(+joint::GeometryResources::SubMeshInstanceIndex, (uint32_t)i);

            context.CmdList.SetPrimitiveTopology(subMesh.PrimitiveTopology);

            const benzin::MeshInfo& subMeshInfo = mesh.SubMeshInfos[meshInstance.SubMeshIndex];
            context.CmdList.DrawIndexed((uint32_t)subMesh.Indices.size(), subMeshInfo.IndexOffset, subMeshInfo.VertexOffset);

            context.Stats.RenderedMeshCount++;
            context.Stats.RenderedTriangleCount += (uint32_t)(subMesh.Indices.size() / 3);
        }
    }

}
