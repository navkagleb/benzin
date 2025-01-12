#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/geometry_pass.hpp"

#include <benzin/core/engine_math.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state_manager.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

#include <shaders/joint/mesh_types.hpp>
#include <shaders/joint/root_constants.hpp>

#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

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
        m_Pso = ms_Device->GetPipelineStateManager().CreatePipelineState(benzin::GraphicsPipelineStateCreation
        {
            .DebugName = "GeometryPass",
            .VsFileName = "geometry_pass.hlsl",
            .PsFileName = "geometry_pass.hlsl",
            .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
            .RasterizerState
            {
                .CullMode = benzin::CullMode::None, // #TODO: Create different PSOs for left-handed and right-handed meshes
                .TriangleOrder = benzin::TriangleOrder::CounterClockwise,
            },
            .RenderTargetFormats
            {
                g_GBufferColor0Format,
                g_GBufferColor1Format,
                g_GBufferColor2Format,
                g_GBufferColor3Format,
                g_GBufferColor4Format,
            },
            .DepthStencilFormat = g_DepthStencilFormat,
        });
    }

    GeometryPass::~GeometryPass()
    {
        ms_Device->GetPipelineStateManager().DestroyPipelineState(m_Pso);

        ms_Resources->DestroyTexture(+Texture::AlbedoAndRoughness);
        ms_Resources->DestroyTexture(+Texture::EmissiveAndMetallic);
        ms_Resources->DestroyTexture(+Texture::WorldNormal);
        ms_Resources->DestroyTexture(+Texture::VelocityBuffer);
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
        createGBufferTexture(Texture::VelocityBuffer, g_GBufferColor3Format, benzin::TextureAccessFlag::AllowRenderTarget);
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

    void GeometryPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        BenzinPushGpuEvent(commandList, "GeometryPass");

        const auto& albedoAndRoughness = ms_Resources->GetTexture(+Texture::AlbedoAndRoughness);
        const auto& emissiveAndMetallic = ms_Resources->GetTexture(+Texture::EmissiveAndMetallic);
        const auto& worldNormal = ms_Resources->GetTexture(+Texture::WorldNormal);
        const auto& velocity = ms_Resources->GetTexture(+Texture::VelocityBuffer);
        const auto& viewDepth = ms_Resources->GetTexture(+Texture::ViewDepth);
        const auto& depthStencil = ms_Resources->GetTexture(+Texture::DepthStencil);

        commandList.SetViewport(ms_RenderViewport);
        commandList.SetScissorRect(ms_RenderScissorRect);

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ albedoAndRoughness, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ emissiveAndMetallic, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ worldNormal, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ velocity, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ viewDepth, benzin::ResourceState::RenderTarget },
            benzin::TransitionBarrier{ depthStencil, benzin::ResourceState::DepthWrite },
        );

        commandList.SetRenderTargets(
            {
                albedoAndRoughness.GetRtv(),
                emissiveAndMetallic.GetRtv(),
                worldNormal.GetRtv(),
                velocity.GetRtv(),
                viewDepth.GetRtv(),
            },
            &ms_Resources->GetTexture(+Texture::DepthStencil).GetDsv()
        );

        commandList.ClearRenderTarget(albedoAndRoughness);
        commandList.ClearRenderTarget(emissiveAndMetallic);
        commandList.ClearRenderTarget(worldNormal);
        commandList.ClearRenderTarget(velocity);
        commandList.ClearRenderTarget(viewDepth);
        commandList.ClearDepthStencil(depthStencil);

        commandList.SetPipelineState(*m_Pso);

        const bool isFrustumCullingEnabled = ms_Settings->GetSection<GBufferSettings>().IsFrustumCullingEnabled;

        auto& stats = ms_Settings->GetSection<GBufferSettings>().Stats;
        stats.MeshCount = 0;
        stats.RenderedMeshCount = 0;
        stats.RenderedTriangleCount = 0;

        const auto& worldToViewMatrix = m_Scene.GetCamera().GetWorldToViewMatrix();
        const auto& cameraFrustum = m_Scene.GetCamera().GetProjection().GetBoundingFrustum();

        const auto& meshRegistry = m_Scene.GetMeshRegistry();

        const auto view = m_Scene.GetEntityRegistry().view<benzin::TransformComponent, benzin::MeshComponent>();
        for (const auto& [_, tc, mc] : view.each())
        {
            const std::string_view meshName = meshRegistry.get<std::string>(mc.MeshHandle);
            BenzinPushGpuEvent(commandList, meshName);

            const auto& mesh = meshRegistry.get<benzin::Mesh>(mc.MeshHandle);
            const auto& meshGpuStorage = meshRegistry.get<benzin::MeshGpuStorage>(mc.MeshHandle);

            commandList.SetRootResource(joint::GeometryPassRc_MeshVertexBuffer, meshGpuStorage.VertexBuffer->GetSrv());
            commandList.SetRootResource(joint::GeometryPassRc_MeshIndexBuffer, meshGpuStorage.IndexBuffer->GetSrv());
            commandList.SetRootResource(joint::GeometryPassRc_MeshInfoBuffer, meshGpuStorage.MeshInfoBuffer->GetSrv());
            commandList.SetRootResource(joint::GeometryPassRc_MeshInstanceBuffer, meshGpuStorage.MeshInstanceBuffer->GetSrv());
            commandList.SetRootResource(joint::GeometryPassRc_MaterialBuffer, meshGpuStorage.MaterialBuffer->GetSrv());
            commandList.SetRootResource(joint::GeometryPassRc_MeshTransformConstantBuffer, tc.GetActiveTransformCbv());

            for (const auto i : std::views::iota(0u, mesh.SubMeshInstances.size()))
            {
                ++stats.MeshCount;

                const joint::MeshInstance& meshInstance = mesh.SubMeshInstances[i];
                const benzin::MeshData& subMesh = mesh.SubMeshes[meshInstance.SubMeshIndex];

                if (isFrustumCullingEnabled && subMesh.BoundingBox.has_value())
                {
                    const DirectX::XMMATRIX localToViewMatrix = meshInstance.Transform * tc.GetLocalToWorldMatrix() * worldToViewMatrix;
                    const auto viewBoundingBox = benzin::TransformBoundingBox(*subMesh.BoundingBox, localToViewMatrix);

                    if (cameraFrustum.Contains(viewBoundingBox) == DirectX::DISJOINT)
                    {
                        continue;
                    }
                }

                commandList.SetRootConstant(joint::GeometryPassRc_MeshInstanceIndex, i);

                commandList.SetPrimitiveTopology(subMesh.PrimitiveTopology);
                commandList.DrawVertexed((uint32_t)subMesh.Indices.size());

                ++stats.RenderedMeshCount;
                stats.RenderedTriangleCount += (uint32_t)(subMesh.Indices.size() / 3);
            }
        }
    }

}
