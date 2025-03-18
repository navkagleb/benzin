#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/geometry_pass.hpp"

#include <benzin/core/engine_math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/geometry_resources.hpp>
#include <shaders/joint/mesh_types.hpp>

#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

BenzinEnableUnaryPlusForEnum(joint::GeometryResources);

namespace sandbox
{

    static constexpr auto g_GBufferColor0Format = benzin::GraphicsFormat::Rgba8Unorm; // Albedo, Albedo, Albedo, Roughness
    static constexpr auto g_GBufferColor1Format = benzin::GraphicsFormat::Rgba8Unorm; // Emissive, Emissive, Emissive, Metallic
    static constexpr auto g_GBufferColor2Format = benzin::GraphicsFormat::Rgba16Float; // WorldNormal, WorldNormal, WorldNormal, None
    static constexpr auto g_GBufferColor3Format = benzin::GraphicsFormat::Rgba16Float; // UvMv, UvMv, ViewDepthMv, None
    static constexpr auto g_GBufferColor4Format = benzin::GraphicsFormat::R32Float; // ViewDepth

    static constexpr auto g_DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint;

    //

    GeometryPass::GeometryPass(const benzin::Scene& scene)
        : m_Scene{ scene }
    {
        const auto createPso = [](PsoId id, benzin::IndexOrder indexOrder)
        {
            ms_PsoManager->Create(id, [id, indexOrder](benzin::GraphicsPsoProxy& proxy)
            {
                proxy.DebugName = magic_enum::enum_name(id);
                proxy.VsFileName = "geometry_pass.hlsl";
                proxy.PsFileName = "geometry_pass.hlsl";
                proxy.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle;

                proxy.RasterizerState.CullMode = benzin::CullMode::Back;
                proxy.RasterizerState.IndexOrder = indexOrder;

                proxy.RenderTargetFormats.reserve(5);
                proxy.RenderTargetFormats.push_back(g_GBufferColor0Format);
                proxy.RenderTargetFormats.push_back(g_GBufferColor1Format);
                proxy.RenderTargetFormats.push_back(g_GBufferColor2Format);
                proxy.RenderTargetFormats.push_back(g_GBufferColor3Format);
                proxy.RenderTargetFormats.push_back(g_GBufferColor4Format);
                proxy.DepthStencilFormat = g_DepthStencilFormat;
            });
        };

        createPso(PsoId::GeometryPassClockwise, benzin::IndexOrder::Clockwise);
        createPso(PsoId::GeometryPassCounterClockwise, benzin::IndexOrder::CounterClockwise);
    }

    GeometryPass::~GeometryPass()
    {
        ms_PsoManager->Destroy(PsoId::GeometryPassClockwise);
        ms_PsoManager->Destroy(PsoId::GeometryPassCounterClockwise);

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
                .Width = GetRenderViewportWidth(),
                .Height = GetRenderViewportHeight(),
                .MipCount = 1,
                .AccessFlags = accessFlag,
            });
        };

        createGBufferTexture(TextureId::AlbedoAndRoughness, g_GBufferColor0Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::EmissiveAndMetallic, g_GBufferColor1Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::WorldNormal, g_GBufferColor2Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::Mv, g_GBufferColor3Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(TextureId::DepthStencil, g_DepthStencilFormat, benzin::TextureAccessFlag::AllowDepthStencil);

        ms_Resources->Create(TextureId::ViewDepth, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(TextureId::ViewDepth),
            .Format = g_GBufferColor4Format,
            .Width = GetRenderViewportWidth(),
            .Height = GetRenderViewportHeight(),
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowRenderTarget | benzin::TextureAccessFlag::AllowUnorderedAccess,
            .ClearValueVariant = DirectX::XMFLOAT4{ std::numeric_limits<float>::max(), 0.0f, 0.0f, 0.0f }, // R32 max value
        });
    }

    void GeometryPass::OnRender() const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "Geometry");

        const auto& albedoAndRoughness = ms_Resources->Get(TextureId::AlbedoAndRoughness);
        const auto& emissiveAndMetallic = ms_Resources->Get(TextureId::EmissiveAndMetallic);
        const auto& worldNormal = ms_Resources->Get(TextureId::WorldNormal);
        const auto& mv = ms_Resources->Get(TextureId::Mv);
        const auto& viewDepth = ms_Resources->Get(TextureId::ViewDepth);
        const auto& depthStencil = ms_Resources->Get(TextureId::DepthStencil);

        cmdList.SetViewport(ms_RenderViewport);
        cmdList.SetScissorRect(ms_RenderScissorRect);

        BenzinMakeScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ albedoAndRoughness, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ emissiveAndMetallic, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ worldNormal, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ mv, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ viewDepth, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ depthStencil, benzin::ResourceState::DepthWrite },
        );

        cmdList.SetRenderTargets(
            {
                albedoAndRoughness.GetRtv(),
                emissiveAndMetallic.GetRtv(),
                worldNormal.GetRtv(),
                mv.GetRtv(),
                viewDepth.GetRtv(),
            },
            &ms_Resources->Get(TextureId::DepthStencil).GetDsv()
        );

        cmdList.ClearRenderTarget(albedoAndRoughness);
        cmdList.ClearRenderTarget(emissiveAndMetallic);
        cmdList.ClearRenderTarget(worldNormal);
        cmdList.ClearRenderTarget(mv);
        cmdList.ClearRenderTarget(viewDepth);
        cmdList.ClearDepthStencil(depthStencil);

        cmdList.SetGraphicsRootResource(+joint::GeometryResources::MeshTransforms, m_Scene.GetTransformBufferSrv());

        const MeshRenderContext context
        {
            .CmdList = cmdList,
            .WorldToViewMatrix = m_Scene.GetCamera().GetWorldToViewMatrix(),
            .CameraFrustum = m_Scene.GetPerspectiveProjection().GetBoundingFrustum(),
            .EntityRegistry = m_Scene.GetEntityRegistry(),
            .MeshRegistry = m_Scene.GetMeshRegistry(),
            .IsFrustumCullingEnabled = ms_Settings->GetSection<GBufferSettings>().IsFrustumCullingEnabled,
            .Stats = ms_Settings->GetSection<GBufferSettings>().Stats,
        };

        context.Stats = {};

        {
            BenzinScopeProfile("Render CounterClockwise Meshes");
            BenzinGpuEvent(cmdList, "Render CounterClockwise Meshes");

            cmdList.SetGraphicsPso(ms_PsoManager->GetGraphics(PsoId::GeometryPassCounterClockwise));
            RenderMeshes(context, (bool)benzin::IndexOrder::CounterClockwise);
        }

        {
            BenzinScopeProfile("Render Clockwise Meshes");
            BenzinGpuEvent(cmdList, "Render Clockwise Meshes");
        
            cmdList.SetGraphicsPso(ms_PsoManager->GetGraphics(PsoId::GeometryPassClockwise));

            RenderMeshes(context, (bool)benzin::IndexOrder::Clockwise);
        }

        {
            BenzinScopeProfile("Render Lights");
            BenzinGpuEvent(cmdList, "Render Lights");

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
        if (!benzin::IsValidEnum(meshComponent.MeshHandle))
        {
            return;
        }

        const std::string_view meshName = context.MeshRegistry.get<std::string>(meshComponent.MeshHandle);
        BenzinGpuEvent(context.CmdList, meshName);

        const auto& mesh = context.MeshRegistry.get<benzin::Mesh>(meshComponent.MeshHandle);
        const auto& meshGpuStorage = context.MeshRegistry.get<benzin::MeshGpuStorage>(meshComponent.MeshHandle);

        context.CmdList.SetGraphicsRootConstant(+joint::GeometryResources::MeshTransformIndex, meshComponent.GpuTransformIndex);
        context.CmdList.SetGraphicsRootResource(+joint::GeometryResources::MeshVertices, meshGpuStorage.VertexBuffer->GetSrv());
        context.CmdList.SetGraphicsRootResource(+joint::GeometryResources::MeshIndices, meshGpuStorage.IndexBuffer->GetSrv());
        context.CmdList.SetGraphicsRootResource(+joint::GeometryResources::SubMeshInfos, meshGpuStorage.MeshInfoBuffer->GetSrv());
        context.CmdList.SetGraphicsRootResource(+joint::GeometryResources::SubMeshInstances, meshGpuStorage.MeshInstanceBuffer->GetSrv());
        context.CmdList.SetGraphicsRootResource(+joint::GeometryResources::Materials, meshGpuStorage.MaterialBuffer->GetSrv());

        for (const auto i : std::views::iota(0u, mesh.SubMeshInstances.size()))
        {
            context.Stats.MeshCount++;

            const joint::MeshInstance& meshInstance = mesh.SubMeshInstances[i];
            const benzin::MeshData& subMesh = mesh.SubMeshes[meshInstance.SubMeshIndex];

            if (context.IsFrustumCullingEnabled && subMesh.BoundingBox.has_value())
            {
                const DirectX::XMMATRIX localToViewMatrix = meshInstance.Transform * localToWorldMatrix * context.WorldToViewMatrix;
                const auto viewBoundingBox = benzin::TransformBoundingBox(*subMesh.BoundingBox, localToViewMatrix);

                if (context.CameraFrustum.Contains(viewBoundingBox) == DirectX::DISJOINT)
                {
                    continue;
                }
            }

            context.CmdList.SetGraphicsRootConstant(+joint::GeometryResources::SubMeshInstanceIndex, i);

            context.CmdList.SetPrimitiveTopology(subMesh.PrimitiveTopology);
            context.CmdList.DrawVertexed((uint32_t)subMesh.Indices.size());

            context.Stats.RenderedMeshCount++;
            context.Stats.RenderedTriangleCount += (uint32_t)(subMesh.Indices.size() / 3);
        }
    }

}
