#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/geometry_pass.hpp"

#include <benzin/core/engine_math.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/scene.hpp>
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

BenzinEnableUnaryPlusForEnum(joint::Rc_Geometry);

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
        , m_Stats{ ms_Settings->GetSection<GBufferSettings>().Stats }
    {
        ms_PsoManager->CreateGraphicsPso(+Pso::GeometryPass, [](benzin::GraphicsPsoProxy& proxy)
        {
            proxy.DebugName = "GeometryPass";
            proxy.VsFileName = "geometry_pass.hlsl";
            proxy.PsFileName = "geometry_pass.hlsl";
            proxy.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle;


            proxy.RasterizerState.CullMode = benzin::CullMode::None; // TODO: Create different PSOs for left-handed and right-handed meshes (generated and GLTF meshes)
            proxy.RasterizerState.TriangleOrder = benzin::TriangleOrder::CounterClockwise;

            proxy.RenderTargetFormats.reserve(5);
            proxy.RenderTargetFormats.push_back(g_GBufferColor0Format);
            proxy.RenderTargetFormats.push_back(g_GBufferColor1Format);
            proxy.RenderTargetFormats.push_back(g_GBufferColor2Format);
            proxy.RenderTargetFormats.push_back(g_GBufferColor3Format);
            proxy.RenderTargetFormats.push_back(g_GBufferColor4Format);

            proxy.DepthStencilFormat = g_DepthStencilFormat;
        });
    }

    GeometryPass::~GeometryPass()
    {
        ms_PsoManager->DestroyPso(+Pso::GeometryPass);

        ms_Resources->DestroyTexture(+Texture::AlbedoAndRoughness);
        ms_Resources->DestroyTexture(+Texture::EmissiveAndMetallic);
        ms_Resources->DestroyTexture(+Texture::WorldNormal);
        ms_Resources->DestroyTexture(+Texture::Mv);
        ms_Resources->DestroyTexture(+Texture::ViewDepth);
        ms_Resources->DestroyTexture(+Texture::DepthStencil);
    }

    void GeometryPass::OnRenderViewportResize()
    {
        const auto createGBufferTexture = [&](
            Texture textureIndex,
            benzin::GraphicsFormat format,
            benzin::TextureAccessFlag accessFlag
        )
        {
            ms_Resources->CreateTexture(+textureIndex, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(textureIndex),
                .Format = format,
                .Width = GetRenderViewportWidth(),
                .Height = GetRenderViewportHeight(),
                .MipCount = 1,
                .AccessFlags = accessFlag,
            });
        };

        createGBufferTexture(Texture::AlbedoAndRoughness, g_GBufferColor0Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(Texture::EmissiveAndMetallic, g_GBufferColor1Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(Texture::WorldNormal, g_GBufferColor2Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(Texture::Mv, g_GBufferColor3Format, benzin::TextureAccessFlag::AllowRenderTarget);
        createGBufferTexture(Texture::DepthStencil, g_DepthStencilFormat, benzin::TextureAccessFlag::AllowDepthStencil);

        ms_Resources->CreateTexture(+Texture::ViewDepth, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(Texture::ViewDepth),
            .Format = g_GBufferColor4Format,
            .Width = GetRenderViewportWidth(),
            .Height = GetRenderViewportHeight(),
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowRenderTarget | benzin::TextureAccessFlag::AllowUnorderedAccess,
            .ClearValueVariant = DirectX::XMFLOAT4{ std::numeric_limits<float>::max(), 0.0f, 0.0f, 0.0f }, // R32 max value
        });
    }

    void GeometryPass::OnUpdate()
    {
        m_IsFrustumCullingEnabled = ms_Settings->GetSection<GBufferSettings>().IsFrustumCullingEnabled;
    }

    void GeometryPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        BenzinGpuEvent(commandList, "Geometry");
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "Geometry");

        const auto& albedoAndRoughness = ms_Resources->GetTexture(+Texture::AlbedoAndRoughness);
        const auto& emissiveAndMetallic = ms_Resources->GetTexture(+Texture::EmissiveAndMetallic);
        const auto& worldNormal = ms_Resources->GetTexture(+Texture::WorldNormal);
        const auto& mv = ms_Resources->GetTexture(+Texture::Mv);
        const auto& viewDepth = ms_Resources->GetTexture(+Texture::ViewDepth);
        const auto& depthStencil = ms_Resources->GetTexture(+Texture::DepthStencil);

        commandList.SetViewport(ms_RenderViewport);
        commandList.SetScissorRect(ms_RenderScissorRect);

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ albedoAndRoughness, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ emissiveAndMetallic, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ worldNormal, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ mv, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ viewDepth, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ depthStencil, benzin::ResourceState::DepthWrite },
        );

        commandList.SetRenderTargets(
            {
                albedoAndRoughness.GetRtv(),
                emissiveAndMetallic.GetRtv(),
                worldNormal.GetRtv(),
                mv.GetRtv(),
                viewDepth.GetRtv(),
            },
            &ms_Resources->GetTexture(+Texture::DepthStencil).GetDsv()
        );

        commandList.ClearRenderTarget(albedoAndRoughness);
        commandList.ClearRenderTarget(emissiveAndMetallic);
        commandList.ClearRenderTarget(worldNormal);
        commandList.ClearRenderTarget(mv);
        commandList.ClearRenderTarget(viewDepth);
        commandList.ClearDepthStencil(depthStencil);

        m_Stats.MeshCount = 0;
        m_Stats.RenderedMeshCount = 0;
        m_Stats.RenderedTriangleCount = 0;

        m_TransformIndex = 0;

        commandList.SetPso(ms_PsoManager->GetPso(+Pso::GeometryPass));
        commandList.SetRootResource(+joint::Rc_Geometry::MeshTransforms, m_Scene.GetTransformBufferSrv());

        const auto view = m_Scene.GetEntityRegistry().view<benzin::MeshComponent, benzin::Transform>();
        for (const auto& [_, mc, transform] : view.each())
        {
            RenderMesh(mc.MeshHandle, transform.GetLocalToWorldMatrix());
        }

        const auto lightView = m_Scene.GetEntityRegistry().view<benzin::MeshComponent, benzin::SphericalLight>();
        for (const auto& [_, mc, light] : lightView.each())
        {
            if (!light.IsEnabled())
            {
                continue;
            }

            RenderMesh(mc.MeshHandle, light.GetTransform().GetLocalToWorldMatrix());
        }
    }

    void GeometryPass::RenderMesh(entt::entity meshHandle, const DirectX::XMMATRIX& localToWorldMatrix) const
    {
        using enum joint::Rc_Geometry;

        const auto& worldToViewMatrix = m_Scene.GetCamera().GetWorldToViewMatrix();
        const auto& cameraFrustum = m_Scene.GetPerspectiveProjection().GetBoundingFrustum();

        const auto& meshRegistry = m_Scene.GetMeshRegistry();
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        if (!benzin::IsValidEnum(meshHandle))
        {
            return;
        }

        const std::string_view meshName = meshRegistry.get<std::string>(meshHandle);
        BenzinGpuEvent(commandList, meshName);

        const auto& mesh = meshRegistry.get<benzin::Mesh>(meshHandle);
        const auto& meshGpuStorage = meshRegistry.get<benzin::MeshGpuStorage>(meshHandle);

        commandList.SetRootConstant(+MeshTransformIndex, m_TransformIndex++);
        commandList.SetRootResource(+MeshVertices, meshGpuStorage.VertexBuffer->GetSrv());
        commandList.SetRootResource(+MeshIndices, meshGpuStorage.IndexBuffer->GetSrv());
        commandList.SetRootResource(+SubMeshInfos, meshGpuStorage.MeshInfoBuffer->GetSrv());
        commandList.SetRootResource(+SubMeshInstances, meshGpuStorage.MeshInstanceBuffer->GetSrv());
        commandList.SetRootResource(+Materials, meshGpuStorage.MaterialBuffer->GetSrv());

        for (const auto i : std::views::iota(0u, mesh.SubMeshInstances.size()))
        {
            m_Stats.MeshCount++;

            const joint::MeshInstance& meshInstance = mesh.SubMeshInstances[i];
            const benzin::MeshData& subMesh = mesh.SubMeshes[meshInstance.SubMeshIndex];

            if (m_IsFrustumCullingEnabled && subMesh.BoundingBox.has_value())
            {
                const DirectX::XMMATRIX localToViewMatrix = meshInstance.Transform * localToWorldMatrix * worldToViewMatrix;
                const auto viewBoundingBox = benzin::TransformBoundingBox(*subMesh.BoundingBox, localToViewMatrix);

                if (cameraFrustum.Contains(viewBoundingBox) == DirectX::DISJOINT)
                {
                    continue;
                }
            }

            commandList.SetRootConstant(+SubMeshInstanceIndex, i);

            commandList.SetPrimitiveTopology(subMesh.PrimitiveTopology);
            commandList.DrawVertexed((uint32_t)subMesh.Indices.size());

            m_Stats.RenderedMeshCount++;
            m_Stats.RenderedTriangleCount += (uint32_t)(subMesh.Indices.size() / 3);
        }
    }

}
