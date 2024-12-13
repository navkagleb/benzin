#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/geometry_pass.hpp"

#include <benzin/core/engine_math.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state_manager.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

#include <shaders/joint/root_constants.hpp>

#include "sandbox/resources.hpp"

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
        ms_Resources->DestroyTexture(+Texture::DepthStencil);
        ms_Resources->DestroyTexture(+Texture::ViewDepth);
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

        const auto view = m_Scene.GetEntityRegistry().view<benzin::TransformComponent, benzin::MeshInstanceComponent>();
        for (const auto entityHandle : view)
        {
            const auto& tc = view.get<benzin::TransformComponent>(entityHandle);
            const auto& mic = view.get<benzin::MeshInstanceComponent>(entityHandle);

            const std::string_view meshCollectionDebugName = m_Scene.GetMeshCollectionDebugName(mic.MeshUnionIndex);
            BenzinPushGpuEvent(commandList, meshCollectionDebugName);

            const auto& meshCollection = m_Scene.GetMeshCollection(mic.MeshUnionIndex);
            const auto& meshCollectionGpuStorage = m_Scene.GetMeshCollectionGpuStorage(mic.MeshUnionIndex);

            commandList.SetRootResource(joint::GeometryPassRc_MeshVertexBuffer, meshCollectionGpuStorage.VertexBuffer->GetSrv());
            commandList.SetRootResource(joint::GeometryPassRc_MeshIndexBuffer, meshCollectionGpuStorage.IndexBuffer->GetSrv());
            commandList.SetRootResource(joint::GeometryPassRc_MeshInfoBuffer, meshCollectionGpuStorage.MeshInfoBuffer->GetSrv());
            commandList.SetRootResource(joint::GeometryPassRc_MeshInstanceBuffer, meshCollectionGpuStorage.MeshInstanceBuffer->GetSrv());
            commandList.SetRootResource(joint::GeometryPassRc_MaterialBuffer, meshCollectionGpuStorage.MaterialBuffer->GetSrv());
            commandList.SetRootResource(joint::GeometryPassRc_MeshTransformConstantBuffer, tc.GetActiveTransformCbv());

            const auto meshInstanceRange = mic.MeshInstanceRange.value_or(meshCollection.GetFullMeshInstanceRange());
            for (const auto i : benzin::IndexRangeToView(meshInstanceRange))
            {
                if (IsMeshCulled(meshCollection, i, tc.GetLocalToWorldMatrix()))
                {
                    continue;
                }

                commandList.SetRootConstant(joint::GeometryPassRc_MeshInstanceIndex, i);

                const auto& meshInstance = meshCollection.MeshInstances[i];
                const auto& mesh = meshCollection.Meshes[meshInstance.MeshIndex];

                commandList.SetPrimitiveTopology(mesh.PrimitiveTopology);
                commandList.DrawVertexed((uint32_t)mesh.Indices.size());
            }
        }
    }

    bool GeometryPass::IsMeshCulled(const benzin::MeshCollection& meshCollection, uint32_t meshInstanceIndex, const DirectX::XMMATRIX& localToWorldMatrix) const
    {
        const auto& camera = m_Scene.GetCamera();
        const auto& meshInstance = meshCollection.MeshInstances[meshInstanceIndex];
        const auto& mesh = meshCollection.Meshes[meshInstance.MeshIndex];

        if (!mesh.BoundingBox)
        {
            return false;
        }

        const auto localToViewSpaceTransformMatrix = meshInstance.Transform * localToWorldMatrix * camera.GetWorldToViewMatrix();
        const auto viewSpaceMeshBoundingBox = benzin::TransformBoundingBox(*mesh.BoundingBox, localToViewSpaceTransformMatrix);

        return camera.GetProjection().GetBoundingFrustum().Contains(viewSpaceMeshBoundingBox) == DirectX::DISJOINT;
    }

}
