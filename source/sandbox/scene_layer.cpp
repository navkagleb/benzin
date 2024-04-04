#include "bootstrap.hpp"
#include "scene_layer.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/core/logger.hpp>
#include <benzin/core/math.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/rt_acceleration_structures.hpp>
#include <benzin/graphics/shaders.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/system/key_event.hpp>
#include <benzin/utility/time_utils.hpp>

#include <shaders/joint/constant_buffer_types.hpp>
#include <shaders/joint/enum_types.hpp>
#include <shaders/joint/root_constants.hpp>
#include <shaders/joint/structured_buffer_types.hpp>

namespace sandbox
{

    enum class CpuTiming : uint32_t
    {
        _BuildTopLevelAs,
        _TotalRenderPasses,
        _GeometryPass,
        _RtShadowPass,
        _DenoiserPass,
        _DeferredLightingPass,
        _EnvironmentPass,
        _FullScreenDebugPass,
        _BackBufferCopy,
        _SceneLayerOnUpdate,
        _SceneLayerOnRender,
    };

    enum class GpuTiming : uint32_t
    {
        _BuildTopLevelAs,
        _TotalRenderPasses,
        _GeometryPass,
        _RtShadowPass,
        _DenoiserPass,
        _DeferredLightingPass,
        _EnvironmentPass,
        _FullScreenDebugPass,
        _BackBufferCopy,
        _Total,
    };

    static magic_enum::containers::array<CpuTiming, std::chrono::microseconds> g_CpuTimings;

    static void ImGuiDisplayTexture(const benzin::Texture& texture)
    {
        const float widgetWidth = ImGui::GetContentRegionAvail().x;

        const uint64_t gpuHandle = texture.GetSrv().GetGpuHandle();
        const float textureAspectRatio = (float)texture.GetWidth() / (float)texture.GetHeight();

        const ImVec2 textureSize{ widgetWidth, widgetWidth / textureAspectRatio };

        const ImVec2 uv0{ 0.0f, 0.0f };
        const ImVec2 uv1{ 1.0f, 1.0f };
        const ImVec4 tintColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        const ImVec4 borderColor{ 1.0f, 1.0f, 1.0f, 0.3f };

        ImGui::Image((ImTextureID)gpuHandle, textureSize, uv0, uv1, tintColor, borderColor);
    };

    static benzin::MeshCollectionResource CreateCylinderMeshCollection()
    {
        const benzin::Material material
        {
            .AlbedoFactor{ 0.7f, 0.7f, 0.7f, 1.0f },
        };

        const benzin::MeshInstance meshInstance
        {
            .MeshIndex = 0,
            .MaterialIndex = 0,
        };

        return benzin::MeshCollectionResource
        {
            .DebugName = "Cylinder",
            .Meshes{ benzin::GetDefaultCyliderMesh() },
            .MeshInstances{ meshInstance },
            .Materials{ material },
        };
    }

    static benzin::MeshCollectionResource CreateSphereLightMeshCollection()
    {
        const benzin::Material material
        {
            .AlbedoFactor{ 0.0f, 0.0f, 0.0f, 0.0f },
            .EmissiveFactor{ 1.0f, 1.0f, 1.0f },
        };

        const benzin::MeshInstance meshInstance
        {
            .MeshIndex = 0,
            .MaterialIndex = 0,
        };

        return benzin::MeshCollectionResource
        {
            .DebugName = "Cylinder",
            .Meshes{ benzin::GetDefaultGeoSphereMesh() },
            .MeshInstances{ meshInstance },
            .Materials{ material },
        };
    }

    static bool IsMeshCulled(const benzin::Camera& camera, const benzin::MeshCollection& meshCollection, uint32_t meshInstanceIndex, const DirectX::XMMATRIX& worldMatrix)
    {
        const auto& meshInstance = meshCollection.MeshInstances[meshInstanceIndex];
        const auto& mesh = meshCollection.Meshes[meshInstance.MeshIndex];

        if (!mesh.BoundingBox)
        {
            return false;
        }

        const auto localToViewSpaceTransformMatrix = meshInstance.Transform * worldMatrix * camera.GetViewMatrix();
        const auto viewSpaceMeshBoundingBox = benzin::TransformBoundingBox(*mesh.BoundingBox, localToViewSpaceTransformMatrix);

        return camera.GetProjection().GetBoundingFrustum().Contains(viewSpaceMeshBoundingBox) == DirectX::DISJOINT;
    }

    // RenderPasses

    struct RenderPassesConfig
    {
        const benzin::GraphicsFormat GBufferColor0Format = benzin::GraphicsFormat::Rgba8Unorm; // Albedo, Albedo, Albedo, Roughness
        const benzin::GraphicsFormat GBufferColor1Format = benzin::GraphicsFormat::Rgba8Unorm; // Emissive, Emissive, Emissive, Metallic
        const benzin::GraphicsFormat GBufferColor2Format = benzin::GraphicsFormat::Rgba16Float; // WorldNormal, WorldNormal, WorldNormal, None
        const benzin::GraphicsFormat GBufferColor3Format = benzin::GraphicsFormat::Rgba16Float; // UvMotionVector, UvMotionVector, DepthMotionVector, None
        const benzin::GraphicsFormat GBufferColor4Format = benzin::GraphicsFormat::R32Float; // ViewDepth

        const benzin::GraphicsFormat DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint;
        const benzin::GraphicsFormat DepthStencilSrvFormat = benzin::GraphicsFormat::D24Unorm_X8Typeless;

        const std::wstring_view RayGenShaderName = L"RayGen";
        const std::wstring_view MissShaderName = L"Miss";
        const std::wstring_view HitGroupName = L"HitGroup";
    };

    struct RenderPassesSettings
    {
        uint32_t RaysPerPixel = 1;
        uint32_t MaxTemporalAccumulationCount = 32;

        float SunIntensity = 0.0f;
        DirectX::XMFLOAT3 SunColor{ 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 SunDirection{ -0.5f, -0.5f, -0.5f };

        joint::DebugOutputType DebugOutputType = joint::DebugOutputType_None;
        uint32_t ViewDepthMipIndex = 0;
        float MinViewDepth = 0.0f;
        float MaxViewDepth = 20.0f;
    };

    static constexpr RenderPassesConfig g_RenderPassesConfig;
    static RenderPassesSettings g_RendePassesSettings;

    struct RenderPassCreation
    {
        benzin::Device& DeviceRef;
        benzin::SwapChain& SwapChainRef;
        RenderResources& RenderResourcesRef;
    };

    class RenderPass
    {
    public:
        explicit RenderPass(const RenderPassCreation & creation)
            : m_Device{ creation.DeviceRef }
            , m_SwapChain{ creation.SwapChainRef }
            , m_RenderResources{ creation.RenderResourcesRef }
        {}
        virtual ~RenderPass() = default;

    public:
        auto IsRenderingEnabled() const { return m_IsRenderingEnabled; }

        virtual void OnResize(uint32_t width, uint32_t height) {};

        virtual void OnUpdate(std::chrono::microseconds dt, std::chrono::milliseconds elapsedTime) {};
        virtual void OnRender() const = 0;

    protected:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;
        RenderResources& m_RenderResources;

        bool m_IsRenderingEnabled = true;
    };

    class GeometryPass : public RenderPass
    {
    public:
        GeometryPass(const RenderPassCreation& creation, const benzin::Scene& scene)
            : RenderPass{ creation }
            , m_Scene{ scene }
        {
            benzin::MakeUniquePtr(m_Pso, m_Device, benzin::GraphicsPipelineStateCreation
            {
                .DebugName = "GeometryPass",
                .VertexShader{ "geometry_pass.hlsl", "VS_Main" },
                .PixelShader{ "geometry_pass.hlsl", "PS_Main" },
                .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
                .RasterizerState
                {
                    .CullMode = benzin::CullMode::None, // #TODO: Create different PSOs for left-handed and right-handed meshes
                    .TriangleOrder = benzin::TriangleOrder::CounterClockwise,
                },
                .RenderTargetFormats
                {
                    g_RenderPassesConfig.GBufferColor0Format,
                    g_RenderPassesConfig.GBufferColor1Format,
                    g_RenderPassesConfig.GBufferColor2Format,
                    g_RenderPassesConfig.GBufferColor3Format,
                    g_RenderPassesConfig.GBufferColor4Format,
                },
                .DepthStencilFormat = g_RenderPassesConfig.DepthStencilFormat,
            });

            OnResize(m_SwapChain.GetViewportWidth(), m_SwapChain.GetViewportHeight());
        }

    public:
        void OnResize(uint32_t width, uint32_t height) override
        {
            const auto createGBufferTexture = [&](
                std::unique_ptr<benzin::Texture>& gbufferTexture,
                std::string_view debugName,
                benzin::GraphicsFormat format,
                benzin::TextureFlag flag
            )
            {
                benzin::MakeUniquePtr(gbufferTexture, m_Device, benzin::TextureCreation
                {
                    .DebugName = debugName,
                    .Format = format,
                    .Width = width,
                    .Height = height,
                    .MipCount = 1,
                    .Flags = flag,
                });
            };

            createGBufferTexture(m_RenderResources.AlbedoAndRoughness, "GBuffer_AlbedoAndRoughness", g_RenderPassesConfig.GBufferColor0Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(m_RenderResources.EmissiveAndMetallic, "GBuffer_EmissiveAndMetallic", g_RenderPassesConfig.GBufferColor1Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(m_RenderResources.WorldNormal, "GBuffer_WorldNormal", g_RenderPassesConfig.GBufferColor2Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(m_RenderResources.VelocityBuffer, "GBuffer_VelocityBuffer", g_RenderPassesConfig.GBufferColor3Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(m_RenderResources.DepthStencil, "GBuffer_DepthStencil", g_RenderPassesConfig.DepthStencilFormat, benzin::TextureFlag::AllowDepthStencil);

            for (auto&& [i, viewDepth] : std::span{ m_RenderResources.ViewDepths } | std::views::enumerate)
            {
                benzin::MakeUniquePtr(viewDepth, m_Device, benzin::TextureCreation
                {
                    .DebugName = std::format("GBuffer_ViewDepth{}", i),
                    .Format = g_RenderPassesConfig.GBufferColor4Format,
                    .Width = width,
                    .Height = height,
                    .MipCount = 5,
                    .Flags = benzin::TextureFlag::AllowRenderTarget | benzin::TextureFlag::AllowUnorderedAccess,
                });
            };
        }

        void OnRender() const override
        {
            auto& gpuTimer = m_Device.GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, magic_enum::enum_integer(GpuTiming::_GeometryPass));

            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, magic_enum::enum_name(GpuTiming::_GeometryPass));

            auto& viewDepth = *m_RenderResources.GetCurrentResource(m_RenderResources.ViewDepths);

            commandList.SetViewport(m_SwapChain.GetViewport());
            commandList.SetScissorRect(m_SwapChain.GetScissorRect());

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_RenderResources.AlbedoAndRoughness, benzin::ResourceState::RenderTarget });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_RenderResources.EmissiveAndMetallic, benzin::ResourceState::RenderTarget });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_RenderResources.WorldNormal, benzin::ResourceState::RenderTarget });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_RenderResources.VelocityBuffer, benzin::ResourceState::RenderTarget });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ viewDepth, benzin::ResourceState::RenderTarget });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_RenderResources.DepthStencil, benzin::ResourceState::DepthWrite });
            BenzinExecuteOnScopeExit([&]
            {
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_RenderResources.AlbedoAndRoughness, benzin::ResourceState::Common });
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_RenderResources.EmissiveAndMetallic, benzin::ResourceState::Common });
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_RenderResources.WorldNormal, benzin::ResourceState::Common });
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_RenderResources.VelocityBuffer, benzin::ResourceState::Common });
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ viewDepth, benzin::ResourceState::Common });
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_RenderResources.DepthStencil, benzin::ResourceState::Common });
            });

            commandList.SetRenderTargets(
                {
                    m_RenderResources.AlbedoAndRoughness->GetRtv(),
                    m_RenderResources.EmissiveAndMetallic->GetRtv(),
                    m_RenderResources.WorldNormal->GetRtv(),
                    m_RenderResources.VelocityBuffer->GetRtv(),
                    viewDepth.GetRtv(),
                },
                &m_RenderResources.DepthStencil->GetDsv()
            );

            commandList.ClearRenderTarget(m_RenderResources.AlbedoAndRoughness->GetRtv());
            commandList.ClearRenderTarget(m_RenderResources.EmissiveAndMetallic->GetRtv());
            commandList.ClearRenderTarget(m_RenderResources.WorldNormal->GetRtv());
            commandList.ClearRenderTarget(m_RenderResources.VelocityBuffer->GetRtv());
            commandList.ClearRenderTarget(viewDepth.GetRtv());
            commandList.ClearDepthStencil(m_RenderResources.DepthStencil->GetDsv());

            commandList.SetPipelineState(*m_Pso);

            const auto view = m_Scene.GetEntityRegistry().view<benzin::TransformComponent, benzin::MeshInstanceComponent>();
            for (const auto entityHandle : view)
            {
                const auto& tc = view.get<benzin::TransformComponent>(entityHandle);
                const auto& mic = view.get<benzin::MeshInstanceComponent>(entityHandle);

                const auto& meshCollection = m_Scene.GetMeshCollection(mic.MeshUnionIndex);
                const auto& meshCollectionGpuStorage = m_Scene.GetMeshCollectionGpuStorage(mic.MeshUnionIndex);

                commandList.SetRootResource(joint::GeometryPassRc_MeshVertexBuffer, meshCollectionGpuStorage.VertexBuffer->GetStructuredSrv());
                commandList.SetRootResource(joint::GeometryPassRc_MeshIndexBuffer, meshCollectionGpuStorage.IndexBuffer->GetStructuredSrv());
                commandList.SetRootResource(joint::GeometryPassRc_MeshInfoBuffer, meshCollectionGpuStorage.MeshInfoBuffer->GetStructuredSrv());
                commandList.SetRootResource(joint::GeometryPassRc_MeshInstanceBuffer, meshCollectionGpuStorage.MeshInstanceBuffer->GetStructuredSrv());
                commandList.SetRootResource(joint::GeometryPassRc_MaterialBuffer, meshCollectionGpuStorage.MaterialBuffer->GetStructuredSrv());
                commandList.SetRootResource(joint::GeometryPassRc_MeshTransformConstantBuffer, tc.GetActiveTransformCbv());

                const auto meshInstanceRange = mic.MeshInstanceRange.value_or(meshCollection.GetFullMeshInstanceRange());
                for (const auto i : benzin::IndexRangeToView(meshInstanceRange))
                {
                    if (IsMeshCulled(m_Scene.GetCamera(), meshCollection, i, tc.GetWorldMatrix()))
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

    private:
        const benzin::Scene& m_Scene;

        std::unique_ptr<benzin::PipelineState> m_Pso;
    };

    class RtShadowPass : public RenderPass
    {
    public:
        RtShadowPass(const RenderPassCreation& creation, const benzin::Scene& scene)
            : RenderPass{ creation }
            , m_Scene{ scene }
        {
            CreatePipelineStateObject();
            CreateShaderTable();

            benzin::MakeUniquePtr(m_PassConstantBuffer, m_Device, "RtShadowPassConstantBuffer");

            OnResize(m_SwapChain.GetViewportWidth(), m_SwapChain.GetViewportHeight());
        }

        void OnResize(uint32_t width, uint32_t height) override
        {
            for (auto&& [i, visibilityBuffer] : std::span{ m_RenderResources.NoisyShadowVisibilityBuffers } | std::views::enumerate)
            {
                benzin::MakeUniquePtr(visibilityBuffer, m_Device, benzin::TextureCreation
                {
                    .DebugName = std::format("RtShadowPass_VisibilityBuffer", i),
                    .Format = benzin::GraphicsFormat::R32Float,
                    .Width = width,
                    .Height = height,
                    .MipCount = 5,
                    .Flags = benzin::TextureFlag::AllowUnorderedAccess,
                });
            }
        }

        void OnUpdate(std::chrono::microseconds dt, std::chrono::milliseconds elapsedTime) override
        {
            m_PassConstantBuffer->UpdateConstants(joint::RtShadowPassConstants
            {
                .RaysPerPixel = g_RendePassesSettings.RaysPerPixel,
            });
        }

        void OnRender() const override
        {
            auto& gpuTimer = m_Device.GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, magic_enum::enum_integer(GpuTiming::_RtShadowPass));

            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, magic_enum::enum_name(GpuTiming::_RtShadowPass));

            auto* d3d12CommandList = commandList.GetD3D12GraphicsCommandList();

            auto& visibilityBuffer = *m_RenderResources.GetCurrentResource(m_RenderResources.NoisyShadowVisibilityBuffers);

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ visibilityBuffer, benzin::ResourceState::UnorderedAccess });
            BenzinExecuteOnScopeExit([&]
            {
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ visibilityBuffer, benzin::ResourceState::Common });
            });

            d3d12CommandList->SetPipelineState1(m_D3D12RaytracingStateObject.Get());

            const auto& activeTopLevelAs = m_Scene.GetActiveTopLevelAs();
            d3d12CommandList->SetComputeRootShaderResourceView(1, activeTopLevelAs.GetBuffer().GetGpuVirtualAddress());

            commandList.SetRootResource(joint::RtShadowRc_PassConstantBuffer, m_PassConstantBuffer->GetActiveCbv());
            commandList.SetRootResource(joint::RtShadowRc_GBufferWorldNormalTexture, m_RenderResources.WorldNormal->GetSrv());
            commandList.SetRootResource(joint::RtShadowRc_GBufferDepthTexture, m_RenderResources.DepthStencil->GetSrv({ .Format = g_RenderPassesConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::RtShadowRc_PointLightBuffer, m_Scene.GetPointLightBufferStructuredSrv());
            commandList.SetRootResource(joint::RtShadowRc_VisiblityBuffer, visibilityBuffer.GetUav());

            const D3D12_DISPATCH_RAYS_DESC d3d12DispatchRayDesc
            {
                .RayGenerationShaderRecord
                {
                    .StartAddress = m_RayGenShaderTable->GetGpuVirtualAddress(),
                    .SizeInBytes = m_RayGenShaderTable->GetNotAlignedSizeInBytes(),
                },
                .MissShaderTable
                {
                    .StartAddress = m_MissShaderTable->GetGpuVirtualAddress(),
                    .SizeInBytes = m_MissShaderTable->GetNotAlignedSizeInBytes(),
                    .StrideInBytes = m_MissShaderTable->GetElementSize(),
                },
                .HitGroupTable
                {
                    .StartAddress = m_HitGroupShaderTable->GetGpuVirtualAddress(),
                    .SizeInBytes = m_HitGroupShaderTable->GetNotAlignedSizeInBytes(),
                    .StrideInBytes = m_HitGroupShaderTable->GetElementSize(),
                },
                .CallableShaderTable
                {
                    .StartAddress = 0,
                    .SizeInBytes = 0,
                    .StrideInBytes = 0,
                },
                .Width = visibilityBuffer.GetWidth(),
                .Height = visibilityBuffer.GetHeight(),
                .Depth = 1,
            };

            d3d12CommandList->DispatchRays(&d3d12DispatchRayDesc);
        }

    private:
        void CreatePipelineStateObject()
        {
            // 1. D3D12_GLOBAL_ROOT_SIGNATURE
            const D3D12_GLOBAL_ROOT_SIGNATURE d3d12GlobalRootSignature
            {
                .pGlobalRootSignature = m_Device.GetD3D12BindlessRootSignature(),
            };

            // 2. D3D12_DXIL_LIBRARY_DESC
            const std::span<const std::byte> libraryBinary = benzin::GetShaderBinary(benzin::ShaderType::Library, { "rt_shadow_pass.hlsl" });

            const D3D12_DXIL_LIBRARY_DESC d3d12DXILLibraryDesc
            {
                .DXILLibrary
                {
                    .pShaderBytecode = libraryBinary.data(),
                    .BytecodeLength = libraryBinary.size(),
                },
                .NumExports = 0,
                .pExports = nullptr,
            };

            // 3. D3D12_HIT_GROUP_DESC
            const D3D12_HIT_GROUP_DESC d3d12HitGroupDesc
            {
                .HitGroupExport = g_RenderPassesConfig.HitGroupName.data(),
                .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES,
                .AnyHitShaderImport = nullptr,
                .ClosestHitShaderImport = nullptr,
                .IntersectionShaderImport = nullptr,
            };

            // 4. D3D12_RAYTRACING_SHADER_CONFIG
            const D3D12_RAYTRACING_SHADER_CONFIG d3d12RaytracingShaderConfig
            {
                .MaxPayloadSizeInBytes = std::max<uint32_t>(4, sizeof(joint::ShadowRayPayload)), // #TODO: Min size is 4 bytes
                .MaxAttributeSizeInBytes = sizeof(DirectX::XMFLOAT2), // Barycentrics
            };

            // 5. D3D12_RAYTRACING_PIPELINE_CONFIG
            const D3D12_RAYTRACING_PIPELINE_CONFIG d3d12RaytracingPipelineConfig
            {
                .MaxTraceRecursionDepth = 1,
            };

            // Create ID3D12StateObject
            const auto d3d12StateSubObjects = std::to_array(
                {
                    D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &d3d12GlobalRootSignature },
                    D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &d3d12DXILLibraryDesc },
                    D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &d3d12HitGroupDesc },
                    D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &d3d12RaytracingShaderConfig },
                    D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &d3d12RaytracingPipelineConfig },
                });

            const D3D12_STATE_OBJECT_DESC d3d12StateObjectDesc
            {
                .Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
                .NumSubobjects = (uint32_t)d3d12StateSubObjects.size(),
                .pSubobjects = d3d12StateSubObjects.data(),
            };

            BenzinEnsure(m_Device.GetD3D12Device()->CreateStateObject(&d3d12StateObjectDesc, IID_PPV_ARGS(&m_D3D12RaytracingStateObject)));
        }

        void CreateShaderTable()
        {
            BenzinEnsure(m_D3D12RaytracingStateObject.Get());

            ComPtr<ID3D12StateObjectProperties> d3d12StateObjectProperties;
            BenzinEnsure(m_D3D12RaytracingStateObject.As(&d3d12StateObjectProperties));

            const auto CreateShaderTable = [&](std::wstring_view identiferName)
            {
                const void* rawShaderIdentifier = d3d12StateObjectProperties->GetShaderIdentifier(identiferName.data());
                const auto shaderIdentifier = std::span{ (const std::byte*)rawShaderIdentifier, benzin::config::g_ShaderIdentifierSizeInBytes };

                return std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
                {
                    .DebugName = std::format("{}ShaderTable", benzin::ToNarrowString(identiferName)),
                    .ElementSize = benzin::config::g_RayTracingShaderRecordAlignment,
                    .ElementCount = 1,
                    .Flags = benzin::BufferFlag::UploadBuffer,
                    .InitialData = shaderIdentifier,
                });
            };

            m_RayGenShaderTable = CreateShaderTable(g_RenderPassesConfig.RayGenShaderName);
            m_MissShaderTable = CreateShaderTable(g_RenderPassesConfig.MissShaderName);
            m_HitGroupShaderTable = CreateShaderTable(g_RenderPassesConfig.HitGroupName);
        }

    private:
        using PassConstantBuffer = benzin::ConstantBuffer<joint::RtShadowPassConstants>;

        const benzin::Scene& m_Scene;

        ComPtr<ID3D12StateObject> m_D3D12RaytracingStateObject;

        std::unique_ptr<benzin::Buffer> m_RayGenShaderTable;
        std::unique_ptr<benzin::Buffer> m_MissShaderTable;
        std::unique_ptr<benzin::Buffer> m_HitGroupShaderTable;

        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
    };

    class DenoiserPass : public RenderPass
    {
    public:
        explicit DenoiserPass(const RenderPassCreation& creation)
            : RenderPass{ creation }
        {
            benzin::MakeUniquePtr(m_TemporalAccumulationPso, m_Device, benzin::ComputePipelineStateCreation
            {
                .DebugName = "DenoiserTemporalAccumulation",
                .ComputeShader{ "denoiser_temporal_accumulation_pass.hlsl", "CsMain" },
            });

            benzin::MakeUniquePtr(m_MipGenerationPso, m_Device, benzin::ComputePipelineStateCreation
            {
                .DebugName = "DenoiserMipGeneration",
                .ComputeShader{ "mip_generation.hlsl", "CsMain" },
            });

            benzin::MakeUniquePtr(m_HistoryFixPso, m_Device, benzin::ComputePipelineStateCreation
            {
                .DebugName = "DenoiserHistoryFix",
                .ComputeShader{ "denoiser_history_fix_pass.hlsl", "CsMain" },
            });

            benzin::MakeUniquePtr(m_MipGenerationConstantBuffer, m_Device, "MipGeneration");

            OnResize(m_SwapChain.GetViewportWidth(), m_SwapChain.GetViewportHeight());
        }

        void OnResize(uint32_t width, uint32_t height) override
        {
            for (auto&& [i, temporalAccumulationBuffer] : std::span{ m_RenderResources.TemporalAccumulationBuffers } | std::views::enumerate)
            {
                benzin::MakeUniquePtr(temporalAccumulationBuffer, m_Device, benzin::TextureCreation
                {
                    .DebugName = std::format("TemporalAccumulationBuffer{}", i),
                    .Format = benzin::GraphicsFormat::R32Float,
                    .Width = width,
                    .Height = height,
                    .MipCount = 1,
                    .Flags = benzin::TextureFlag::AllowUnorderedAccess,
                });
            }

            benzin::MakeUniquePtr(m_RenderResources.ReprojectedHistoryTexture, m_Device, benzin::TextureCreation
            {
                .DebugName = "ReprojectedHistory",
                .Format = benzin::GraphicsFormat::R32Float,
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowUnorderedAccess,
            });

            for (auto&& [i, denoisedVisibilityBuffer] : std::span{ m_RenderResources.DenoisedShadowVisibilityBuffers } | std::views::enumerate)
            {
                benzin::MakeUniquePtr(denoisedVisibilityBuffer, m_Device, benzin::TextureCreation
                {
                    .DebugName = std::format("DenoisedVisibilityBuffer{}", i),
                    .Format = benzin::GraphicsFormat::R32Float,
                    .Width = width,
                    .Height = height,
                    .MipCount = 1,
                    .Flags = benzin::TextureFlag::AllowUnorderedAccess,
                });
            }
        }

        void OnRender() const
        {
            // DenoiserPreBlurRenderPass +
            // DenoiserReprojectionRenderPass +
            // DenoiserMipGenerationRenderPass +
            // DenoiserGradientConstructionRenderPass ??
            // DenoiserGradientFilteringRenderPass ??
            // DenoiserGradientSamplesGenerationRenderPass ??
            // DenoiserHistoryFixRenderPass + 
            // DenoiserPostBlurRenderPass +
            // DenoiserMainRenderPass +

            // PreBlur pass
            // Accumulation pass
            // MipGeneration pass
            // HistoryFix pass (Hierarchical History Reconstruction)
            // Blur
            // PostBlur
            // TemporalStabilization

            // PreBlur
            // - Uses constant radius, pass is needed to fix outliers

            // Accumulation
            // - Linear weights, up to 32 frames

            // MipGeneration
            // - Ultra fast single pass generation of first 4 mips in shared memory (averaging)

            // HistoryFix
            // - History reconstruction in discarded regions

            // Blur
            // - Adaptive radius (depends on number of accumulated frames)

            // PostBlur
            // - Adaptive radius (depends on number of accumulated frames +
            //   adaptively scales if intensity delta between reprojected
            //   history and the final value is high)

            // TemporalStabilization
            // - No additional lag, TAA like filter but uses wider variance clamping if possible

            auto& gpuTimer = m_Device.GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, magic_enum::enum_integer(GpuTiming::_DenoiserPass));

            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, magic_enum::enum_name(GpuTiming::_DenoiserPass));

            RunTemporalAccumulationSubPass();
            RunMipGenerationSubPass();
            RunHistoryFixSubPass();
        }

    private:
        void RunTemporalAccumulationSubPass() const
        {
            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

            BenzinPushGpuEvent(commandList, "Denoiser_TemporalAccumulation");

            auto& previousViewDepth = *m_RenderResources.GetPreviousResource(m_RenderResources.ViewDepths);

            auto& previousTemporalAccumulationBuffer = *m_RenderResources.GetPreviousResource(m_RenderResources.TemporalAccumulationBuffers);
            auto& currentTemporalAccumulationBuffer = *m_RenderResources.GetCurrentResource(m_RenderResources.TemporalAccumulationBuffers);

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentTemporalAccumulationBuffer, benzin::ResourceState::UnorderedAccess });
            BenzinExecuteOnScopeExit([&]
            {
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentTemporalAccumulationBuffer, benzin::ResourceState::Common });
            });

            commandList.SetPipelineState(*m_TemporalAccumulationPso);

            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_WorldNormalTexture, m_RenderResources.WorldNormal->GetSrv());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_VelocityBuffer, m_RenderResources.VelocityBuffer->GetSrv());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_DepthBuffer, m_RenderResources.DepthStencil->GetSrv({ .Format = g_RenderPassesConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_PreviousViewDepthBuffer, previousViewDepth.GetSrv({ .MipRange{ 0, 1 } }));
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_PreviousTemporalAccumulationBuffer, previousTemporalAccumulationBuffer.GetSrv());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_CurrentTemporalAccumulationBuffer, currentTemporalAccumulationBuffer.GetUav());

            const DirectX::XMUINT3 dimensions{ m_SwapChain.GetViewportWidth(), m_SwapChain.GetViewportHeight(), 1 };
            const DirectX::XMUINT3 threadPerGroupCount{ joint::tc::DenoiserTemporalAccumulation_X, joint::tc::DenoiserTemporalAccumulation_Y, joint::tc::DenoiserTemporalAccumulation_Z };
            commandList.Dispatch(dimensions, threadPerGroupCount);
        }

        void RunMipGenerationSubPass() const
        {
            auto& viewDepth = *m_RenderResources.GetCurrentResource(m_RenderResources.ViewDepths);
            auto& visibilityBuffer = *m_RenderResources.GetCurrentResource(m_RenderResources.NoisyShadowVisibilityBuffers);

            BenzinAssert(viewDepth.GetWidth() == visibilityBuffer.GetWidth() && viewDepth.GetHeight() == visibilityBuffer.GetHeight());

            const uint32_t dispatchMipIndex = 1;
            const auto dispatchMipWidth = viewDepth.GetMipWidth(dispatchMipIndex);
            const auto dispatchMipHeight = viewDepth.GetMipHeight(dispatchMipIndex);

            m_MipGenerationConstantBuffer->UpdateConstants(joint::MipGenerationConstants
            {
                .InvDispatchDimensions{ 1.0f / dispatchMipWidth, 1.0f / dispatchMipHeight },
                .IsSourceWidthOdd = (viewDepth.GetWidth() & 1) == 1,
                .IsSourceHeightOdd = (viewDepth.GetHeight() & 1) == 1,
                .DestinationMipCount = 4,
            });

            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

            BenzinPushGpuEvent(commandList, "Denoiser_MipGeneration");

            commandList.SetPipelineState(*m_MipGenerationPso);
            commandList.SetRootResource(joint::MipGenerationRc_PassConstantBuffer, m_MipGenerationConstantBuffer->GetActiveCbv());

            const auto dispatchTexture = [&](benzin::Texture& texture)
            {
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ texture, benzin::ResourceState::UnorderedAccess });
                BenzinExecuteOnScopeExit([&]
                {
                    commandList.SetResourceBarrier(benzin::TransitionBarrier{ texture, benzin::ResourceState::Common });
                });

                commandList.SetRootResource(joint::MipGenerationRc_SourceMip, texture.GetSrv({ .MipRange{ 0, 1 } }));
                commandList.SetRootResource(joint::MipGenerationRc_DestinationMip0, texture.GetUav({ .MipIndex = 1 }));
                commandList.SetRootResource(joint::MipGenerationRc_DestinationMip1, texture.GetUav({ .MipIndex = 2 }));
                commandList.SetRootResource(joint::MipGenerationRc_DestinationMip2, texture.GetUav({ .MipIndex = 3 }));
                commandList.SetRootResource(joint::MipGenerationRc_DestinationMip3, texture.GetUav({ .MipIndex = 4 }));

                const DirectX::XMUINT3 dimensions{ dispatchMipWidth, dispatchMipHeight, 1 };
                const DirectX::XMUINT3 threadPerGroupCount{ joint::tc::MipGeneration_X, joint::tc::MipGeneration_Y, joint::tc::MipGeneration_Z };
                commandList.Dispatch(dimensions, threadPerGroupCount);
            };

            dispatchTexture(viewDepth);
            dispatchTexture(visibilityBuffer);
        }

        void RunHistoryFixSubPass() const
        {
            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

            BenzinPushGpuEvent(commandList, "Denoiser_HistoryFix");

            const auto& temporalAccumulationBuffer = *m_RenderResources.GetCurrentResource(m_RenderResources.TemporalAccumulationBuffers);
            const auto& viewDepth = *m_RenderResources.GetCurrentResource(m_RenderResources.ViewDepths);
            const auto& noisyVisibilityBuffer = *m_RenderResources.GetCurrentResource(m_RenderResources.NoisyShadowVisibilityBuffers);
            auto& denoisedVisibilityBuffer = *m_RenderResources.GetCurrentResource(m_RenderResources.DenoisedShadowVisibilityBuffers);

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ denoisedVisibilityBuffer, benzin::ResourceState::UnorderedAccess });
            BenzinExecuteOnScopeExit([&]
            {
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ denoisedVisibilityBuffer, benzin::ResourceState::Common });
            });

            commandList.SetPipelineState(*m_HistoryFixPso);

            commandList.SetRootResource(joint::DenoiserHistoryFixRc_GBufferAlbedoAndRoughness, m_RenderResources.AlbedoAndRoughness->GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_TemporalAccumulationBuffer, temporalAccumulationBuffer.GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_ViewDepthBuffer, viewDepth.GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_NoisyVisibilityBuffer, noisyVisibilityBuffer.GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_DenoisedVisibilityBuffer, denoisedVisibilityBuffer.GetUav());

            const DirectX::XMUINT3 dimensions{ m_SwapChain.GetViewportWidth(), m_SwapChain.GetViewportHeight(), 1 };
            const DirectX::XMUINT3 threadPerGroupCount{ joint::tc::DenoiserHistoryFix_X, joint::tc::DenoiserHistoryFix_Y, joint::tc::DenoiserHistoryFix_Z };
            commandList.Dispatch(dimensions, threadPerGroupCount);
        }

    private:
        using MipGenerationConstantBuffer = benzin::ConstantBuffer<joint::MipGenerationConstants>;

        std::unique_ptr<benzin::PipelineState> m_TemporalAccumulationPso;
        std::unique_ptr<benzin::PipelineState> m_MipGenerationPso;
        std::unique_ptr<benzin::PipelineState> m_HistoryFixPso;

        std::unique_ptr<MipGenerationConstantBuffer> m_MipGenerationConstantBuffer;
    };

    class DeferredLightingPass : public RenderPass
    {
    public:
        DeferredLightingPass(const RenderPassCreation& creation, const benzin::Scene& scene)
            : RenderPass{ creation }
            , m_Scene{ scene }
        {
            benzin::MakeUniquePtr(m_Pso, m_Device, benzin::GraphicsPipelineStateCreation
            {
                .DebugName = "DeferredLightingPass",
                .VertexShader{ "fullscreen_triangle.hlsl", "VS_Main" },
                .PixelShader{ "deferred_lighting_pass.hlsl", "PS_Main" },
                .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
                .DepthState
                {
                    .IsEnabled = false,
                    .IsWriteEnabled = false,
                },
                .RenderTargetFormats{ benzin::GraphicsFormat::Rgba8Unorm },
            });

            benzin::MakeUniquePtr(m_PassConstantBuffer, m_Device, "DeferredLightingPassConstantBuffer");

            OnResize(m_SwapChain.GetViewportWidth(), m_SwapChain.GetViewportHeight());
        }

        void OnResize(uint32_t width, uint32_t height) override
        {
            benzin::MakeUniquePtr(m_RenderResources.FinalOutputTexture, m_Device, benzin::TextureCreation
            {
                .DebugName = "DeferredLightingPass_OutputTexture",
                .Format = benzin::CommandLineArgs::GetBackBufferFormat(),
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowRenderTarget,
            });
        }

        void OnUpdate(std::chrono::microseconds dt, std::chrono::milliseconds elapsedTime) override
        {
            m_IsRenderingEnabled = g_RendePassesSettings.DebugOutputType == joint::DebugOutputType_None;

            m_PassConstantBuffer->UpdateConstants(joint::DeferredLightingPassConstants
            {
                .SunColor = g_RendePassesSettings.SunColor,
                .SunIntensity = g_RendePassesSettings.SunIntensity,
                .SunDirection = g_RendePassesSettings.SunDirection,
                .ActivePointLightCount = m_Scene.GetStats().PointLightCount,
            });
        }

        void OnRender() const override
        {
            auto& gpuTimer = m_Device.GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, magic_enum::enum_integer(GpuTiming::_DeferredLightingPass));

            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, magic_enum::enum_name(GpuTiming::_DeferredLightingPass));

            auto& finalOutputTexture = *m_RenderResources.FinalOutputTexture;
            auto& denoisedShadowVisibilityBuffer = *m_RenderResources.GetCurrentResource(m_RenderResources.DenoisedShadowVisibilityBuffers);

            commandList.SetViewport(m_SwapChain.GetViewport());
            commandList.SetScissorRect(m_SwapChain.GetScissorRect());

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::RenderTarget });
            BenzinExecuteOnScopeExit([&]
            {
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::Common });
            });

            commandList.SetRenderTargets({ finalOutputTexture.GetRtv() });
            commandList.ClearRenderTarget(finalOutputTexture.GetRtv());

            commandList.SetPipelineState(*m_Pso);

            commandList.SetRootResource(joint::DeferredLightingPassRc_PassConstantBuffer, m_PassConstantBuffer->GetActiveCbv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_AlbedoAndRoughnessTexture, m_RenderResources.AlbedoAndRoughness->GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_EmissiveAndMetallicTexture, m_RenderResources.EmissiveAndMetallic->GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_WorldNormalTexture, m_RenderResources.WorldNormal->GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_VelocityBuffer, m_RenderResources.VelocityBuffer->GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_DepthStencilTexture, m_RenderResources.DepthStencil->GetSrv({ .Format = g_RenderPassesConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::DeferredLightingPassRc_PointLightBuffer, m_Scene.GetPointLightBufferStructuredSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_ShadowVisibilityBuffer, denoisedShadowVisibilityBuffer.GetSrv());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

    private:
        using PassConstantBuffer = benzin::ConstantBuffer<joint::DeferredLightingPassConstants>;

        const benzin::Scene& m_Scene;

        std::unique_ptr<benzin::PipelineState> m_Pso;
        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
    };

    class EnvironmentPass : public RenderPass
    {
    public:
        explicit EnvironmentPass(const RenderPassCreation& creation)
            : RenderPass{ creation }
        {
            benzin::MakeUniquePtr(m_EquirectangularToCubePso, m_Device, benzin::ComputePipelineStateCreation
            {
                .DebugName = "EquirectangularToCube",
                .ComputeShader{ "equirectangular_to_cube.hlsl", "CsMain" },
            });

            benzin::MakeUniquePtr(m_Pso, m_Device, benzin::GraphicsPipelineStateCreation
            {
                .DebugName = "EnvironmentPass",
                .VertexShader{ "fullscreen_triangle.hlsl", "VS_MainDepth1" },
                .PixelShader{ "environment_pass.hlsl", "PS_Main" },
                .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
                .DepthState
                {
                    .IsWriteEnabled = false,
                    .ComparisonFunction = benzin::ComparisonFunction::Equal,
                },
                .RenderTargetFormats{ benzin::GraphicsFormat::Rgba8Unorm },
                .DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint,
            });

            LoadEquirectangularTexture();
            ComputeCubeMapTexture();
        }

        void OnUpdate(std::chrono::microseconds dt, std::chrono::milliseconds elapsedTime) override
        {
            m_IsRenderingEnabled = g_RendePassesSettings.DebugOutputType == joint::DebugOutputType_None;
        }

        void OnRender() const override
        {
            auto& gpuTimer = m_Device.GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, magic_enum::enum_integer(GpuTiming::_EnvironmentPass));

            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, magic_enum::enum_name(GpuTiming::_EnvironmentPass));

            auto& finalOutputTexture = *m_RenderResources.FinalOutputTexture;
            auto& depthStencilBuffer = *m_RenderResources.DepthStencil;

            commandList.SetViewport(m_SwapChain.GetViewport());
            commandList.SetScissorRect(m_SwapChain.GetScissorRect());

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::RenderTarget });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ depthStencilBuffer, benzin::ResourceState::DepthRead });
            BenzinExecuteOnScopeExit([&]
            {
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::Common });
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ depthStencilBuffer, benzin::ResourceState::Common });
            });

            commandList.SetRenderTargets({ finalOutputTexture.GetRtv() }, &depthStencilBuffer.GetDsv());

            commandList.SetPipelineState(*m_Pso);
            commandList.SetRootResource(joint::EnvironmentPassRc_CubeMapTexture, m_CubeTexture->GetSrv());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

    private:
        void LoadEquirectangularTexture()
        {
            benzin::TextureImage equirectangularTextureImage;
            BenzinAssert(benzin::LoadTextureImageFromHdrFile("scythian_tombs_2_4k.hdr", equirectangularTextureImage));

            benzin::MakeUniquePtr(m_EquirectangularTexture, m_Device, benzin::TextureCreation
            {
                .DebugName = equirectangularTextureImage.DebugName,
                .Format = equirectangularTextureImage.Format,
                .Width = equirectangularTextureImage.Width,
                .Height = equirectangularTextureImage.Height,
                .MipCount = 1,
            });

            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList(m_EquirectangularTexture->GetSizeInBytes());
            commandList.UploadToTextureTopMip(*m_EquirectangularTexture, std::as_bytes(std::span{ equirectangularTextureImage.ImageData }));
        }

        void ComputeCubeMapTexture()
        {
            const uint32_t cubeMapSize = 1024;
            benzin::MakeUniquePtr(m_CubeTexture, m_Device, benzin::TextureCreation
            {
                .DebugName = "EnvironmentCubeMap",
                .IsCubeMap = true,
                .Format = benzin::GraphicsFormat::Rgba32Float,
                .Width = cubeMapSize,
                .Height = cubeMapSize,
                .Depth = 6,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowUnorderedAccess,
            });

            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

            commandList.SetPipelineState(*m_EquirectangularToCubePso);

            commandList.SetRootResource(joint::EquirectangularToCubeRc_EquirectangularTexture, m_EquirectangularTexture->GetSrv());
            commandList.SetRootResource(joint::EquirectangularToCubeRc_OutCubeTexture, m_CubeTexture->GetUav());

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_CubeTexture, benzin::ResourceState::UnorderedAccess });

            commandList.Dispatch(
                { cubeMapSize, cubeMapSize, m_CubeTexture->GetDepth() },
                { joint::EquirectangularToCubeThreadCount_X, joint::EquirectangularToCubeThreadCount_Y, joint::EquirectangularToCubeThreadCount_Z }
            );

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_CubeTexture, benzin::ResourceState::Common });
        }

    private:
        std::unique_ptr<benzin::PipelineState> m_EquirectangularToCubePso;
        std::unique_ptr<benzin::Texture> m_EquirectangularTexture;

        std::unique_ptr<benzin::PipelineState> m_Pso;
        std::unique_ptr<benzin::Texture> m_CubeTexture;
    };

    class FullScreenDebugPass : public RenderPass
    {
    public:
        FullScreenDebugPass(const RenderPassCreation& creation)
            : RenderPass{ creation }
        {
            benzin::MakeUniquePtr(m_Pso, m_Device, benzin::GraphicsPipelineStateCreation
            {
                .DebugName = "FullScreenDebugPass",
                .VertexShader{ "fullscreen_triangle.hlsl", "VS_Main" },
                .PixelShader{ "fullscreen_debug_pass.hlsl", "PS_Main" },
                .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
                .DepthState
                {
                    .IsEnabled = false,
                    .IsWriteEnabled = false,
                },
                .RenderTargetFormats{ benzin::GraphicsFormat::Rgba8Unorm },
            });

            benzin::MakeUniquePtr(m_PassConstantBuffer, m_Device, "FullScreenDebugPass_PassConstantBuffer");
        }

        void OnUpdate(std::chrono::microseconds dt, std::chrono::milliseconds elapsedTime) override
        {
            m_IsRenderingEnabled = g_RendePassesSettings.DebugOutputType != joint::DebugOutputType_None;

            m_PassConstantBuffer->UpdateConstants(joint::FullScreenDebugConstants
            {
                .OutputType = magic_enum::enum_integer(g_RendePassesSettings.DebugOutputType),
                .ViewDepthMipIndex = g_RendePassesSettings.ViewDepthMipIndex,
                .MinViewDepth = g_RendePassesSettings.MinViewDepth,
                .MaxViewDepth = g_RendePassesSettings.MaxViewDepth,
            });
        }

        void OnRender() const override
        {
            auto& gpuTimer = m_Device.GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, magic_enum::enum_integer(GpuTiming::_FullScreenDebugPass));

            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, magic_enum::enum_name(GpuTiming::_FullScreenDebugPass));

            auto& finalOutputTexture = *m_RenderResources.FinalOutputTexture;
            auto& viewDepth = *m_RenderResources.GetCurrentResource(m_RenderResources.ViewDepths);
            auto& noisyShadowVisibilityBuffer = *m_RenderResources.GetCurrentResource(m_RenderResources.NoisyShadowVisibilityBuffers);
            auto& temporalAccumulationBuffer = *m_RenderResources.GetCurrentResource(m_RenderResources.TemporalAccumulationBuffers);

            commandList.SetViewport(m_SwapChain.GetViewport());
            commandList.SetScissorRect(m_SwapChain.GetScissorRect());

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::RenderTarget });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ noisyShadowVisibilityBuffer, benzin::ResourceState::PixelShaderResource });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ temporalAccumulationBuffer, benzin::ResourceState::PixelShaderResource });
            BenzinExecuteOnScopeExit([&]
            {
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::Common });
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ noisyShadowVisibilityBuffer, benzin::ResourceState::Common });
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ temporalAccumulationBuffer, benzin::ResourceState::Common });
            });

            commandList.SetRenderTargets({ finalOutputTexture.GetRtv() });
            commandList.ClearRenderTarget(finalOutputTexture.GetRtv());

            commandList.SetPipelineState(*m_Pso);

            commandList.SetRootResource(joint::FullScreenDebugRc_PassConstantBuffer, m_PassConstantBuffer->GetActiveCbv());
            commandList.SetRootResource(joint::FullScreenDebugRc_AlbedoAndRoughnessTexture, m_RenderResources.AlbedoAndRoughness->GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_EmissiveAndMetallicTexture, m_RenderResources.EmissiveAndMetallic->GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_WorldNormalTexture, m_RenderResources.WorldNormal->GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_VelocityBuffer, m_RenderResources.VelocityBuffer->GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_ViewDepthBuffer, viewDepth.GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_DepthBuffer, m_RenderResources.DepthStencil->GetSrv({ .Format = g_RenderPassesConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::FullScreenDebugRc_ShadowVisibilityBuffer, noisyShadowVisibilityBuffer.GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_TemporalAccumulationBuffer, temporalAccumulationBuffer.GetSrv());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

    private:
        using PassConstantBuffer = benzin::ConstantBuffer<joint::FullScreenDebugConstants>;

        std::unique_ptr<benzin::PipelineState> m_Pso;
        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
    };

    // SceneLayer

    SceneLayer::SceneLayer(const benzin::GraphicsRefs& graphicsRefs)
        : m_Window{ graphicsRefs.WindowRef }
        , m_Device{ graphicsRefs.DeviceRef }
        , m_SwapChain{ graphicsRefs.SwapChainRef }
    {
        {
            auto& perspectiveProjection = m_Scene.GetPerspectiveProjection();
            perspectiveProjection.SetLens(DirectX::XMConvertToRadians(60.0f), m_SwapChain.GetAspectRatio(), 0.1f, 1000.0f);

            auto& camera = m_Scene.GetCamera();
            camera.SetPosition({ -3.0f, 2.0f, -0.25f });
            camera.SetFrontDirection({ 1.0f, 0.0f, 0.0f });
        }

        {
            BenzinLogTimeOnScopeExit("Load and create entities");
            LoadAndCreateEntities();
        }

        {
            BenzinLogTimeOnScopeExit("Upload scene data to GPU");
            m_Scene.UploadMeshCollections();
        }

        {
            BenzinLogTimeOnScopeExit("Build scene RT BottomLevel ASs");
            m_Scene.BuildBottomLevelAccelerationStructures();
        }

        benzin::MakeUniquePtr(m_FrameConstantBuffer, m_Device, "FrameConstantBuffer");

        const RenderPassCreation creation
        {
            .DeviceRef = m_Device,
            .SwapChainRef = m_SwapChain,
            .RenderResourcesRef = m_RenderResources,
        };

        m_RenderPasses.push_back(std::make_unique<GeometryPass>(creation, m_Scene));
        m_RenderPasses.push_back(std::make_unique<RtShadowPass>(creation, m_Scene));
        m_RenderPasses.push_back(std::make_unique<DenoiserPass>(creation));
        m_RenderPasses.push_back(std::make_unique<DeferredLightingPass>(creation, m_Scene));
        m_RenderPasses.push_back(std::make_unique<EnvironmentPass>(creation));
        m_RenderPasses.push_back(std::make_unique<FullScreenDebugPass>(creation));
    }

    SceneLayer::~SceneLayer() = default;

    void SceneLayer::OnEvent(benzin::Event& event)
    {
        m_FlyCameraController.OnEvent(event);

        benzin::EventDispatcher dispatcher{ event };
        {
            dispatcher.Dispatch<benzin::KeyPressedEvent>([&](auto& event)
            {
                if (event.GetKeyCode() == benzin::KeyCode::F2)
                {
                    m_IsAnimationEnabled = !m_IsAnimationEnabled;
                }

                return false;
            });
        }
    }

    void SceneLayer::OnUpdate()
    {
        BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_SceneLayerOnUpdate]);

        const auto dt = s_FrameTimer.GetDeltaTime();
        const auto elapsedTime = s_FrameTimer.GetElapsedTime();

        m_FrameConstantBuffer->UpdateConstants(joint::FrameConstants
        {
            .RenderResolution{ (float)m_SwapChain.GetViewportWidth(), (float)m_SwapChain.GetViewportHeight() },
            .InvRenderResolution{ 1.0f / m_SwapChain.GetViewportWidth(), 1.0f / m_SwapChain.GetViewportHeight() },
            .CpuFrameIndex = (uint32_t)m_Device.GetCpuFrameIndex(),
            .DeltaTime = benzin::ToFloatMs(dt),
            .MaxTemporalAccumulationCount = g_RendePassesSettings.MaxTemporalAccumulationCount,
        });

        m_FlyCameraController.OnUpdate(dt);
        m_Scene.OnUpdate(dt);

        m_RenderResources.FlipResources();

        for (auto& renderPass : m_RenderPasses)
        {
            renderPass->OnUpdate(dt, elapsedTime);
        }
    }

    void SceneLayer::OnRender()
    {
        auto& gpuTimer = m_Device.GetGpuTimer();
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_SceneLayerOnRender]);
        BenzinGrabGpuTimeOnScopeExit(gpuTimer, magic_enum::enum_integer(GpuTiming::_Total));

        commandList.SetRootResource(joint::GlobalRc_FrameConstantBuffer, m_FrameConstantBuffer->GetActiveCbv());
        commandList.SetRootResource(joint::GlobalRc_CameraConstantBuffer, m_Scene.GetCameraConstantBufferActiveCbv());

        {
            // Before updating TopLevel AccelerationStructure the TransformComponents must be updated

            BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_BuildTopLevelAs]);
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, magic_enum::enum_integer(GpuTiming::_BuildTopLevelAs));
            BenzinPushGpuEvent(commandList, magic_enum::enum_name(GpuTiming::_BuildTopLevelAs));

            m_Scene.BuildTopLevelAccelerationStructure();
        }

        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_TotalRenderPasses]);
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, magic_enum::enum_integer(GpuTiming::_TotalRenderPasses));
            BenzinPushGpuEvent(commandList, "RenderPasses");

            for (auto& renderPass : m_RenderPasses)
            {
                if (renderPass->IsRenderingEnabled())
                {
                    renderPass->OnRender();
                }
            }
        }

        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_BackBufferCopy]);
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, magic_enum::enum_integer(GpuTiming::_BackBufferCopy));
            BenzinPushGpuEvent(commandList, magic_enum::enum_name(GpuTiming::_BackBufferCopy));

            auto& currentBackBuffer = m_SwapChain.GetCurrentBackBuffer();
            auto& finalOutputTexture = *m_RenderResources.FinalOutputTexture;

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::CopyDestination });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::CopySource });
            BenzinExecuteOnScopeExit([&]
            {
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::Common });
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::Common });
            });

            commandList.CopyResource(currentBackBuffer, finalOutputTexture);
        }
    }

    void SceneLayer::OnResize(uint32_t width, uint32_t height)
    {
        for (auto& renderPass : m_RenderPasses)
        {
            renderPass->OnResize(width, height);
        }
    }

    void SceneLayer::OnImGuiRender()
    {
        m_FlyCameraController.OnImGuiRender();

        ImGui::Begin("Misc");
        {
            const ImVec4 titleColor{ 0.72f, 39.0f, 0.0f, 1.0f };

            {
                ImGui::TextColored(titleColor, "RtShadowParams");

                ImGui::SliderInt("RtShadows RaysPerPixel", (int*)&g_RendePassesSettings.RaysPerPixel, 0, 100);
                ImGui::DragInt("MaxTemporalAccumulationCount", (int*)&g_RendePassesSettings.MaxTemporalAccumulationCount, 1.0f, 1, 64);

                ImGui::Separator();
                ImGui::NewLine();
            }

            {
                ImGui::TextColored(titleColor, "DeferredLightingParams");

                ImGui::DragFloat("SunIntensity", &g_RendePassesSettings.SunIntensity, 0.1f, 0.0f, 100.0f);
                ImGui::ColorEdit3("SunColor", reinterpret_cast<float*>(&g_RendePassesSettings.SunColor));

                if (ImGui::DragFloat3("SunDirection", reinterpret_cast<float*>(&g_RendePassesSettings.SunDirection), 0.01f, -1.0f, 1.0f))
                {
                    DirectX::XMStoreFloat3(&g_RendePassesSettings.SunDirection, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&g_RendePassesSettings.SunDirection)));
                }

                ImGui::Separator();
                ImGui::NewLine();
            }

            {
                ImGui::TextColored(titleColor, "FullScreenDebugParams");

                ImGui::SliderInt("ViewDepthMipIndex", (int*)&g_RendePassesSettings.ViewDepthMipIndex, 0, 4);
                ImGui::SliderFloat("MinViewDepth", &g_RendePassesSettings.MinViewDepth, 0.001f, 2.0f, "%.4f");
                ImGui::SliderFloat("MaxViewDepth", &g_RendePassesSettings.MaxViewDepth, 0.001f, 30.0f);

                ImGui::Text("DebugOutputType");
                if (ImGui::BeginListBox("##emtpy", ImVec2{ -FLT_MIN, 200.0f })) // #TODO: Calculate item height
                {
                    for (const auto i : std::views::iota(0u, joint::DebugOutputType_Count))
                    {
                        const bool isSelected = g_RendePassesSettings.DebugOutputType == i;

                        const auto name = magic_enum::enum_name((joint::DebugOutputType)i).substr("DebugOutputType_"sv.size());
                        if (ImGui::Selectable(name.data(), isSelected))
                        {
                            g_RendePassesSettings.DebugOutputType = (joint::DebugOutputType)i;
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndListBox();
                }
            }
        }
        ImGui::End();

        ImGui::Begin("Scene Stats");
        {
            struct ThoudandSeperatorApostrophe3 : std::numpunct<char>
            {
                char do_thousands_sep() const override { return '\''; }
            
                std::string do_grouping() const override { return "\3"; }
            };

            static const std::locale customLocale{ std::locale::classic(), new ThoudandSeperatorApostrophe3 };

            std::locale::global(customLocale);
            BenzinExecuteOnScopeExit([]{ std::locale::global(std::locale::classic()); });

            const auto& sceneStats = m_Scene.GetStats();
            ImGui::Text(BenzinFormatCstr("VertexCount: {:L}", sceneStats.VertexCount));
            ImGui::Text(BenzinFormatCstr("TriangleCount: {:L}", sceneStats.TriangleCount));
            ImGui::Text(BenzinFormatCstr("PointLightCount: {:L}", sceneStats.PointLightCount));
        }
        ImGui::End();

        ImGui::Begin("CPU Timings");
        {
            static auto cpuTimings = g_CpuTimings;

            if (s_IsUpdateStatsIntervalPassed)
            {
                cpuTimings = g_CpuTimings;
            }

            for (const auto [i, timing] : cpuTimings | std::views::enumerate)
            {
                ImGui::Text(BenzinFormatCstr("{}: {:.4f} ms", magic_enum::enum_name((CpuTiming)i).substr(1), benzin::ToFloatMs(timing)));
            }
        }
        ImGui::End();

        ImGui::Begin("GPU Timings");
        {
            static magic_enum::containers::array<GpuTiming, std::chrono::microseconds> gpuTimings;

            if (s_IsUpdateStatsIntervalPassed)
            {
                for (const auto& [i, timing] : gpuTimings | std::views::enumerate)
                {
                    timing = m_Device.GetGpuTimer().GetElapsedTime((uint32_t)i);
                }
            }

            for (const auto [i, timing] : gpuTimings | std::views::enumerate)
            {
                ImGui::Text(BenzinFormatCstr("{}: {:.4f} ms", magic_enum::enum_name((GpuTiming)i).substr(1), benzin::ToFloatMs(timing)));
            }
        }
        ImGui::End();
    }

// #TODO: Application is hang when using std::async
#define SANDBOX_IS_PIX_WORKAROUND_ENABLED 1

    void SceneLayer::LoadAndCreateEntities()
    {
        auto& entityRegistry = m_Scene.GetEntityRegistry();

        benzin::MeshCollectionResource sponzaMeshCollection;
#if !SANDBOX_IS_PIX_WORKAROUND_ENABLED
        const auto sponzaFuture = std::async(std::launch::async, [&]
        {
            const std::string_view fileName = "Sponza/glTF/Sponza.gltf";

            BenzinLogTimeOnScopeExit("Loading MeshCollection from {}", fileName);
            BenzinAssert(benzin::LoadMeshCollectionFromGltfFile(fileName, sponzaMeshCollection));
        });
#else
        {
            const std::string_view fileName = "Sponza/glTF/Sponza.gltf";

            BenzinLogTimeOnScopeExit("Loading MeshCollection from {}", fileName);
            BenzinAssert(benzin::LoadMeshCollectionFromGltfFile(fileName, sponzaMeshCollection));
        }
#endif

        benzin::MeshCollectionResource boomBooxMeshCollection;
        {
            const std::string_view fileName = "BoomBox/glTF/BoomBox.gltf";

            BenzinLogTimeOnScopeExit("Loading MeshCollection from {}", fileName);
            BenzinAssert(benzin::LoadMeshCollectionFromGltfFile(fileName, boomBooxMeshCollection));
        }

        benzin::MeshCollectionResource damagedHelmetMeshCollection;
        {
            const std::string_view fileName = "DamagedHelmet/glTF/DamagedHelmet.gltf";

            BenzinLogTimeOnScopeExit("Loading MeshCollection from {}", fileName);
            BenzinAssert(benzin::LoadMeshCollectionFromGltfFile(fileName, damagedHelmetMeshCollection));
        }

        benzin::MeshCollectionResource cylinderMeshCollection = CreateCylinderMeshCollection();
        benzin::MeshCollectionResource sphereLightMeshCollection = CreateSphereLightMeshCollection();

#if !SANDBOX_IS_PIX_WORKAROUND_ENABLED
        sponzaFuture.wait();
#endif

        const uint32_t sponzaMeshUnionIndex = m_Scene.PushMeshCollection(std::move(sponzaMeshCollection));
        const uint32_t boomBooxMeshUnionIndex = m_Scene.PushMeshCollection(std::move(boomBooxMeshCollection));
        const uint32_t damagedHelmetMeshUnionIndex = m_Scene.PushMeshCollection(std::move(damagedHelmetMeshCollection));
        const uint32_t cylinderMeshUnionIndex = m_Scene.PushMeshCollection(std::move(cylinderMeshCollection));
        const uint32_t sphereLightMeshUnionIndex = m_Scene.PushMeshCollection(std::move(sphereLightMeshCollection));

        // Sponza
        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sponzaMeshUnionIndex;

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetRotation({ 0.0f, DirectX::XM_PI, 0.0f });
            tc.SetTranslation({ 5.0f, 0.0f, 0.0f });
        }

        // BoomBox
        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = boomBooxMeshUnionIndex;

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetRotation({ 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f });
            tc.SetScale({ 30.0f, 30.0f, 30.0f });
            tc.SetTranslation({ 0.0f, 0.6f, 0.0f });

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(entity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle, std::chrono::microseconds dt)
            {
                if (!m_IsAnimationEnabled)
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                auto rotation = tc.GetRotation();
                rotation.x += 0.0001f * benzin::ToFloatMs(dt);
                rotation.z += 0.0002f * benzin::ToFloatMs(dt);

                tc.SetRotation(rotation);
            };
        }

        // DamagedHelmet
        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = damagedHelmetMeshUnionIndex;

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetRotation({ 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f });
            tc.SetScale({ 0.4f, 0.4f, 0.4f });
            tc.SetTranslation({ 1.0f, 0.5f, -0.5f });

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(entity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle, std::chrono::microseconds dt)
            {
                if (!m_IsAnimationEnabled)
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                auto rotation = tc.GetRotation();
                rotation.x += 0.0001f * benzin::ToFloatMs(dt);
                rotation.y -= 0.00015f * benzin::ToFloatMs(dt);

                tc.SetRotation(rotation);
            };
        }

        // Cylinder
        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = cylinderMeshUnionIndex;

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetScale({ 0.1f, 1.5f, 0.1f });
            tc.SetTranslation({ -1.5f, 0.4f, -0.25f });
        }

        // SphereLight
        {
            constexpr float sphereLightRadius = 0.05f;

            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sphereLightMeshUnionIndex;

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetScale({ sphereLightRadius, sphereLightRadius, sphereLightRadius });
            tc.SetTranslation({ 0.5f, 1.5f, -0.25f });

            auto& plc = entityRegistry.emplace<benzin::PointLightComponent>(entity);
            plc.Color = { 1.0f, 1.0f, 1.0f };
            plc.Intensity = 10.0f;
            plc.Range = 30.0f;
            plc.GeometryRadius = sphereLightRadius;

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(entity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle, std::chrono::microseconds dt)
            {
                static constexpr float travelRadius = 1.0f;
                static constexpr float travelSpeed = 0.0004f;

                static std::chrono::microseconds elapsedTime;

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                static const float startX = tc.GetTranslation().x;
                static const float startZ = tc.GetTranslation().z;

                if (m_IsAnimationEnabled)
                {
                    elapsedTime += dt;

                    auto translation = tc.GetTranslation();
                    translation.x = startX + travelRadius * std::cos(travelSpeed * benzin::ToFloatMs(elapsedTime));
                    translation.z = startZ + travelRadius * std::sin(travelSpeed * benzin::ToFloatMs(elapsedTime));

                    tc.SetTranslation(translation);
                }
            };
        }
    }

} // namespace sandbox
