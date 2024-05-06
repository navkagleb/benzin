#include "sandbox/bootstrap.hpp"
#include "sandbox/sandbox_runner.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/core/math.hpp>
#include <benzin/engine/camera.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
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

#include <shaders/joint/constant_buffer_types.hpp>
#include <shaders/joint/enum_types.hpp>
#include <shaders/joint/root_constants.hpp>
#include <shaders/joint/structured_buffer_types.hpp>

#include "sandbox/tools/render_pass_settings_tool.hpp"

namespace sandbox
{

    static Timings<SandboxTiming> g_CpuTimings;

    enum class RenderTextures : uint32_t
    {
        // GBuffer
        AlbedoAndRoughness,
        EmissiveAndMetallic,
        WorldNormal,
        VelocityBuffer,
        DepthStencil,
        ViewDepth = DepthStencil + 2,

        NoisyShadowVisibilityBuffer = ViewDepth + 2,

        TemporalAccumulationBuffer = NoisyShadowVisibilityBuffer + 2,
        ReprojectedHistoryTexture,
        DenoisedShadowVisibilityBuffer = ReprojectedHistoryTexture + 2,

        FinalOutputTexture,
    };
    BenzinEnableUnaryPlusForEnum(RenderTextures);

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

    static constexpr RenderPassesConfig g_RenderPassesConfig;
    static RenderPassSettings g_RenderPassSettings;

    class GlobalConstantBufferPass : public benzin::RenderPass
    {
    public:
        explicit GlobalConstantBufferPass(benzin::Scene& scene)
            : m_Scene{ scene }
        {
            benzin::MakeUniquePtr(m_FrameConstantBuffer, *ms_Device, "FrameConstantBuffer");
        }

        void OnUpdate(const benzin::TickTimer& tickTimer) override
        {
            m_FrameConstantBuffer->UpdateConstants(joint::FrameConstants
            {
                .RenderResolution{ (float)ms_SwapChain->GetViewportWidth(), (float)ms_SwapChain->GetViewportHeight() },
                .InvRenderResolution{ 1.0f / ms_SwapChain->GetViewportWidth(), 1.0f / ms_SwapChain->GetViewportHeight() },
                .CpuFrameIndex = (uint32_t)ms_Device->GetCpuFrameIndex(),
                .DeltaTime = tickTimer.GetDeltaTimeInMs(),
                .ElapsedTime = tickTimer.GetElapsedTimeInMs(),
                .IsRtShadowsEnabled = g_RenderPassSettings.IsRtShadowEnabled,
                .IsDenoiserEnabled = g_RenderPassSettings.IsDenoiserEnabled,
                .MaxTemporalAccumulationCount = g_RenderPassSettings.MaxTemporalAccumulationCount,
                .CurrentCamera = m_Scene.GetCurrentCameraConstants(),
                .PreviousCamera = m_Scene.GetPreviousCameraConstants(),
            });
        }

        void OnRender() const override
        {
            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

            {
                // Before updating TopLevel AccelerationStructure the TransformComponents must be updated

                auto& gpuTimer = ms_Device->GetGpuTimer();

                BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::BuildTopLevelAs]);
                BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::BuildTopLevelAs);
                BenzinPushGpuEvent(commandList, "BuildTopLevelAs");

                m_Scene.BuildTopLevelAccelerationStructure();
            }

            commandList.SetCbv(benzin::UnifiedRootParameter::FrameConstantBuffer, m_FrameConstantBuffer->GetActiveGpuVirtualAddress());

            if (m_Scene.HasMeshes())
            {
                commandList.SetSrv(benzin::UnifiedRootParameter::TopLevelAs, m_Scene.GetActiveTopLevelAs().GetBuffer().GetGpuVirtualAddress());
            }
        }

    private:
        benzin::Scene& m_Scene;

        using FrameConstantBuffer = benzin::ConstantBuffer<joint::FrameConstants>;
        std::unique_ptr<FrameConstantBuffer> m_FrameConstantBuffer;
    };

    class GeometryPass : public benzin::RenderPass
    {
    public:
        GeometryPass(const benzin::Scene& scene)
            : m_Scene{ scene }
        {
            benzin::MakeUniquePtr(m_Pso, *ms_Device, benzin::GraphicsPipelineStateCreation
            {
                .DebugName = "GeometryPass",
                .VertexShader{ "geometry_pass.hlsl", "VsMain" },
                .PixelShader{ "geometry_pass.hlsl", "PsMain" },
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
        }

        void OnResize(uint32_t width, uint32_t height) override
        {
            const auto createGBufferTexture = [&](
                std::unique_ptr<benzin::Texture>& gbufferTexture,
                std::string_view debugName,
                benzin::GraphicsFormat format,
                benzin::TextureFlag flag
            )
            {
                benzin::MakeUniquePtr(gbufferTexture, *ms_Device, benzin::TextureCreation
                {
                    .DebugName = debugName,
                    .Format = format,
                    .Width = width,
                    .Height = height,
                    .MipCount = 1,
                    .Flags = flag,
                });
            };

            createGBufferTexture(ms_RenderResources->GetTexture(+RenderTextures::AlbedoAndRoughness), "GBuffer_AlbedoAndRoughness", g_RenderPassesConfig.GBufferColor0Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(ms_RenderResources->GetTexture(+RenderTextures::EmissiveAndMetallic), "GBuffer_EmissiveAndMetallic", g_RenderPassesConfig.GBufferColor1Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(ms_RenderResources->GetTexture(+RenderTextures::WorldNormal), "GBuffer_WorldNormal", g_RenderPassesConfig.GBufferColor2Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(ms_RenderResources->GetTexture(+RenderTextures::VelocityBuffer), "GBuffer_VelocityBuffer", g_RenderPassesConfig.GBufferColor3Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(ms_RenderResources->GetTexture(+RenderTextures::DepthStencil), "GBuffer_DepthStencil", g_RenderPassesConfig.DepthStencilFormat, benzin::TextureFlag::AllowDepthStencil);

            ms_RenderResources->ForEachFlippableTexture(+RenderTextures::ViewDepth, [&](uint32_t i, auto& outTexture)
            {
                benzin::MakeUniquePtr(outTexture, *ms_Device, benzin::TextureCreation
                {
                    .DebugName = std::format("GBuffer_ViewDepth{}", i),
                    .Format = g_RenderPassesConfig.GBufferColor4Format,
                    .Width = width,
                    .Height = height,
                    .MipCount = 5,
                    .Flags = benzin::TextureFlag::AllowRenderTarget | benzin::TextureFlag::AllowUnorderedAccess,
                });
            });
        }

        void OnRender() const override
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::GeometryPass]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::GeometryPass);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "GeometryPass");

            const auto& albedoAndRoughness = *ms_RenderResources->GetTexture(+RenderTextures::AlbedoAndRoughness);
            const auto& emissiveAndMetallic = *ms_RenderResources->GetTexture(+RenderTextures::EmissiveAndMetallic);
            const auto& worldNormal = *ms_RenderResources->GetTexture(+RenderTextures::WorldNormal);
            const auto& velocity = *ms_RenderResources->GetTexture(+RenderTextures::VelocityBuffer);
            const auto& viewDepth = *ms_RenderResources->GetTexture(+RenderTextures::ViewDepth);
            const auto& depthStencil = *ms_RenderResources->GetTexture(+RenderTextures::DepthStencil);

            commandList.SetViewport(ms_SwapChain->GetViewport());
            commandList.SetScissorRect(ms_SwapChain->GetScissorRect());

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
                &ms_RenderResources->GetTexture(+RenderTextures::DepthStencil)->GetDsv()
            );

            commandList.ClearRenderTarget(albedoAndRoughness.GetRtv());
            commandList.ClearRenderTarget(emissiveAndMetallic.GetRtv());
            commandList.ClearRenderTarget(worldNormal.GetRtv());
            commandList.ClearRenderTarget(velocity.GetRtv());
            commandList.ClearRenderTarget(viewDepth.GetRtv());
            commandList.ClearDepthStencil(depthStencil.GetDsv());

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
                    // if (IsMeshCulled(meshCollection, i, tc.GetWorldMatrix()))
                    // {
                    //     continue;
                    // }

                    commandList.SetRootConstant(joint::GeometryPassRc_MeshInstanceIndex, i);

                    const auto& meshInstance = meshCollection.MeshInstances[i];
                    const auto& mesh = meshCollection.Meshes[meshInstance.MeshIndex];

                    commandList.SetPrimitiveTopology(mesh.PrimitiveTopology);
                    commandList.DrawVertexed((uint32_t)mesh.Indices.size());
                }
            }
        }

    private:
        bool IsMeshCulled(const benzin::MeshCollection& meshCollection, uint32_t meshInstanceIndex, const DirectX::XMMATRIX& worldMatrix) const
        {
            const auto& camera = m_Scene.GetCamera();
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

    private:
        const benzin::Scene& m_Scene;

        std::unique_ptr<benzin::PipelineState> m_Pso;
    };

    class RtShadowPass : public benzin::RenderPass
    {
    public:
        RtShadowPass(const benzin::Scene& scene)
            : m_Scene{ scene }
        {
            CreatePipelineStateObject();
            CreateShaderTable();

            benzin::MakeUniquePtr(m_PassConstantBuffer, *ms_Device, "RtShadowPassConstantBuffer");
        }

        void OnResize(uint32_t width, uint32_t height) override
        {
            ms_RenderResources->ForEachFlippableTexture(+RenderTextures::NoisyShadowVisibilityBuffer, [&](uint32_t i, auto& outTexture)
            {
                benzin::MakeUniquePtr(outTexture, *ms_Device, benzin::TextureCreation
                {
                    .DebugName = std::format("RtShadowPass_VisibilityBuffer", i),
                    .Format = benzin::GraphicsFormat::R32Float,
                    .Width = width,
                    .Height = height,
                    .MipCount = 5,
                    .Flags = benzin::TextureFlag::AllowUnorderedAccess,
                });
            });
        }

        void OnUpdate() override
        {
            m_PassConstantBuffer->UpdateConstants(joint::RtShadowPassConstants
            {
                .RaysPerPixel = g_RenderPassSettings.RaysPerPixel,
            });
        }

        void OnRender() const override
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::RtShadowPass]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::RtShadowPass);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            auto* d3d12CommandList = commandList.GetD3D12GraphicsCommandList();
            BenzinPushGpuEvent(commandList, "RtShadowPass");

            const auto& visibilityBuffer = *ms_RenderResources->GetTexture(+RenderTextures::NoisyShadowVisibilityBuffer);

            d3d12CommandList->SetPipelineState1(m_D3D12RaytracingStateObject.Get());

            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstantBuffer->GetActiveGpuVirtualAddress());
            commandList.SetRootResource(joint::RtShadowRc_GBufferWorldNormalTexture, ms_RenderResources->GetTexture(+RenderTextures::WorldNormal)->GetSrv());
            commandList.SetRootResource(joint::RtShadowRc_GBufferDepthTexture, ms_RenderResources->GetTexture(+RenderTextures::DepthStencil)->GetSrv({ .Format = g_RenderPassesConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::RtShadowRc_PointLightBuffer, m_Scene.GetPointLightBufferStructuredSrv());
            commandList.SetRootResource(joint::RtShadowRc_VisiblityBuffer, visibilityBuffer.GetUav());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ visibilityBuffer, benzin::ResourceState::UnorderedAccess },
            );

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
                .pGlobalRootSignature = ms_Device->GetD3D12UnifiedRootSignature(),
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

            BenzinEnsure(ms_Device->GetD3D12Device()->CreateStateObject(&d3d12StateObjectDesc, IID_PPV_ARGS(&m_D3D12RaytracingStateObject)));
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

                return std::make_unique<benzin::Buffer>(*ms_Device, benzin::BufferCreation
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
        const benzin::Scene& m_Scene;

        ComPtr<ID3D12StateObject> m_D3D12RaytracingStateObject;

        std::unique_ptr<benzin::Buffer> m_RayGenShaderTable;
        std::unique_ptr<benzin::Buffer> m_MissShaderTable;
        std::unique_ptr<benzin::Buffer> m_HitGroupShaderTable;

        using PassConstantBuffer = benzin::ConstantBuffer<joint::RtShadowPassConstants>;
        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
    };

    class DenoiserPass : public benzin::RenderPass
    {
    public:
        DenoiserPass()
        {
            benzin::MakeUniquePtr(m_TemporalAccumulationPso, *ms_Device, benzin::ComputePipelineStateCreation
            {
                .DebugName = "DenoiserTemporalAccumulation",
                .ComputeShader{ "denoiser_temporal_accumulation_pass.hlsl", "CsMain" },
            });

            benzin::MakeUniquePtr(m_MipGenerationPso, *ms_Device, benzin::ComputePipelineStateCreation
            {
                .DebugName = "DenoiserMipGeneration",
                .ComputeShader{ "mip_generation_pass.hlsl", "CsMain" },
            });

            benzin::MakeUniquePtr(m_HistoryFixPso, *ms_Device, benzin::ComputePipelineStateCreation
            {
                .DebugName = "DenoiserHistoryFix",
                .ComputeShader{ "denoiser_history_fix_pass.hlsl", "CsMain" },
            });

            benzin::MakeUniquePtr(m_BlurPso, *ms_Device, benzin::ComputePipelineStateCreation
            {
                .DebugName = "DenoiserBlur",
                .ComputeShader{ "denoiser_blur_pass.hlsl", "CsMain" },
            });

            benzin::MakeUniquePtr(m_MipGenerationConstantBuffer, *ms_Device, "MipGenerationConstantBuffer");
            benzin::MakeUniquePtr(m_BlurConstantBuffer, *ms_Device, "BlurConstantBuffer");
        }

        void OnResize(uint32_t width, uint32_t height) override
        {
            ms_RenderResources->ForEachFlippableTexture(+RenderTextures::TemporalAccumulationBuffer, [&](uint32_t i, auto& outTexture)
            {
                benzin::MakeUniquePtr(outTexture, *ms_Device, benzin::TextureCreation
                {
                    .DebugName = std::format("TemporalAccumulationBuffer{}", i),
                    .Format = benzin::GraphicsFormat::R32Float,
                    .Width = width,
                    .Height = height,
                    .MipCount = 1,
                    .Flags = benzin::TextureFlag::AllowUnorderedAccess,
                });
            });

            benzin::MakeUniquePtr(ms_RenderResources->GetTexture(+RenderTextures::ReprojectedHistoryTexture), *ms_Device, benzin::TextureCreation
            {
                .DebugName = "ReprojectedHistory",
                .Format = benzin::GraphicsFormat::R32Float,
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowUnorderedAccess,
            });

            ms_RenderResources->ForEachFlippableTexture(+RenderTextures::DenoisedShadowVisibilityBuffer, [&](uint32_t i, auto& outTexture)
            {
                benzin::MakeUniquePtr(outTexture, *ms_Device, benzin::TextureCreation
                {
                    .DebugName = std::format("DenoisedVisibilityBuffer{}", i),
                    .Format = benzin::GraphicsFormat::R32Float,
                    .Width = width,
                    .Height = height,
                    .MipCount = 1,
                    .Flags = benzin::TextureFlag::AllowUnorderedAccess,
                });
            });
        }

        void OnUpdate()
        {
            m_BlurConstantBuffer->UpdateConstants(joint::DenoiserBlurConstants
            {
                .IsGeometryWeightUsed = g_RenderPassSettings.IsGeometryWeightUsed,
                .IsNormalWeightUsed = g_RenderPassSettings.IsNormalWeightUsed,
                .IsRoughnessWeightUsed = g_RenderPassSettings.IsRoughnessWeightUsed,
                .GeometryWeightSensitivity = g_RenderPassSettings.GeometryWeightSensitivity,
            });
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

            // - Diffuse denoiser
            // - HistoryFrames
            // - AO Ranges
            // - BlurRadius (px)
            // - AdaptiveRadiusScale

            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::DenoiserPass]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::DenoiserPass);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "DenoiserPass");

            RunTemporalAccumulationSubPass();
            RunMipGenerationSubPass();
            RunHistoryFixSubPass();
            RunBlurSubPass();
            RunPostBlurSubPass();
        }

    private:
        void RunTemporalAccumulationSubPass() const
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::DenoiserPass_Accumulation]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::DenoiserPass_Accumulation);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "Denoiser_TemporalAccumulation");

            const auto& previousViewDepth = *ms_RenderResources->GetPreviousTexture(+RenderTextures::ViewDepth);
            const auto& previousAccumulationBuffer = *ms_RenderResources->GetPreviousTexture(+RenderTextures::TemporalAccumulationBuffer);
            const auto& previousDenoisedVisibilityBuffer = *ms_RenderResources->GetPreviousTexture(+RenderTextures::DenoisedShadowVisibilityBuffer);
            const auto& accumulationBuffer = *ms_RenderResources->GetTexture(+RenderTextures::TemporalAccumulationBuffer);
            const auto& reprojectedHistoryTexture = *ms_RenderResources->GetTexture(+RenderTextures::ReprojectedHistoryTexture);

            commandList.SetPipelineState(*m_TemporalAccumulationPso);

            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_WorldNormalTexture, ms_RenderResources->GetTexture(+RenderTextures::WorldNormal)->GetSrv());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_VelocityBuffer, ms_RenderResources->GetTexture(+RenderTextures::VelocityBuffer)->GetSrv());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_DepthBuffer, ms_RenderResources->GetTexture(+RenderTextures::DepthStencil)->GetSrv({ .Format = g_RenderPassesConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_PreviousViewDepthBuffer, previousViewDepth.GetSrv({ .MipRange{ 0, 1 } }));
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_PreviousTemporalAccumulationBuffer, previousAccumulationBuffer.GetSrv());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_PreviousDenoisedVisibilityBuffer, previousDenoisedVisibilityBuffer.GetSrv());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_TemporalAccumulationBuffer, accumulationBuffer.GetUav());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_ReprojectedHistoryTexture, reprojectedHistoryTexture.GetUav());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ accumulationBuffer, benzin::ResourceState::UnorderedAccess },
                benzin::TransitionBarrier{ reprojectedHistoryTexture, benzin::ResourceState::UnorderedAccess },
            );

            const DirectX::XMUINT3 dimensions{ ms_SwapChain->GetViewportWidth(), ms_SwapChain->GetViewportHeight(), 1 };
            const DirectX::XMUINT3 threadPerGroupCount{ joint::ThreadCount881_X, joint::ThreadCount881_Y, joint::ThreadCount881_Z };
            commandList.Dispatch(dimensions, threadPerGroupCount);
        }

        void RunMipGenerationSubPass() const
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::DenoiserPass_Mips]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::DenoiserPass_Mips);

            const auto& viewDepth = *ms_RenderResources->GetTexture(+RenderTextures::ViewDepth);
            const auto& visibilityBuffer = *ms_RenderResources->GetTexture(+RenderTextures::NoisyShadowVisibilityBuffer);

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

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

            BenzinPushGpuEvent(commandList, "Denoiser_MipGeneration");

            commandList.SetPipelineState(*m_MipGenerationPso);
            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_MipGenerationConstantBuffer->GetActiveGpuVirtualAddress());

            const auto dispatchTexture = [&](const benzin::Texture& texture)
            {
                commandList.SetRootResource(joint::MipGenerationRc_SourceMip, texture.GetSrv({ .MipRange{ 0, 1 } }));
                commandList.SetRootResource(joint::MipGenerationRc_DestinationMip0, texture.GetUav({ .MipIndex = 1 }));
                commandList.SetRootResource(joint::MipGenerationRc_DestinationMip1, texture.GetUav({ .MipIndex = 2 }));
                commandList.SetRootResource(joint::MipGenerationRc_DestinationMip2, texture.GetUav({ .MipIndex = 3 }));
                commandList.SetRootResource(joint::MipGenerationRc_DestinationMip3, texture.GetUav({ .MipIndex = 4 }));

                BenzinMakeScopedResourceBarriers(
                    commandList,
                    benzin::TransitionBarrier{ texture, benzin::ResourceState::UnorderedAccess },
                );

                const DirectX::XMUINT3 dimensions{ dispatchMipWidth, dispatchMipHeight, 1 };
                const DirectX::XMUINT3 threadPerGroupCount{ joint::ThreadCount881_X, joint::ThreadCount881_Y, joint::ThreadCount881_Z };
                commandList.Dispatch(dimensions, threadPerGroupCount);
            };

            dispatchTexture(viewDepth);
            dispatchTexture(visibilityBuffer);
        }

        void RunHistoryFixSubPass() const
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::DenoiserPass_HistoryFix]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::DenoiserPass_HistoryFix);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "Denoiser_HistoryFix");

            const auto& temporalAccumulationBuffer = *ms_RenderResources->GetTexture(+RenderTextures::TemporalAccumulationBuffer);
            const auto& viewDepth = *ms_RenderResources->GetTexture(+RenderTextures::ViewDepth);
            const auto& noisyVisibilityBuffer = *ms_RenderResources->GetTexture(+RenderTextures::NoisyShadowVisibilityBuffer);
            const auto& reprojectedHistoryTexture = *ms_RenderResources->GetTexture(+RenderTextures::ReprojectedHistoryTexture);

            commandList.SetPipelineState(*m_HistoryFixPso);

            commandList.SetRootResource(joint::DenoiserHistoryFixRc_GBufferAlbedoAndRoughness, ms_RenderResources->GetTexture(+RenderTextures::AlbedoAndRoughness)->GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_TemporalAccumulationBuffer, temporalAccumulationBuffer.GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_ViewDepthBuffer, viewDepth.GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_NoisyVisibilityBuffer, noisyVisibilityBuffer.GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_ReprojectedHistoryTexture, reprojectedHistoryTexture.GetUav());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ reprojectedHistoryTexture, benzin::ResourceState::UnorderedAccess },
            );

            const DirectX::XMUINT3 dimensions{ reprojectedHistoryTexture.GetWidth(), reprojectedHistoryTexture.GetHeight(), 1 };
            const DirectX::XMUINT3 threadPerGroupCount{ joint::ThreadCount881_X, joint::ThreadCount881_Y, joint::ThreadCount881_Z };
            commandList.Dispatch(dimensions, threadPerGroupCount);
        }

        void RunBlurSubPass() const
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::DenoiserPass_Blur]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::DenoiserPass_Blur);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "Denoiser_Blur");

            const auto& noisyVisibilityBuffer = *ms_RenderResources->GetTexture(+RenderTextures::NoisyShadowVisibilityBuffer);
            const auto& denoisedVisibilityBuffer = *ms_RenderResources->GetTexture(+RenderTextures::DenoisedShadowVisibilityBuffer);

            commandList.SetPipelineState(*m_BlurPso);

            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_BlurConstantBuffer->GetActiveGpuVirtualAddress());
            commandList.SetRootResource(joint::DenoiserBlurRc_AlbedoAndRoughnessTexture, ms_RenderResources->GetTexture(+RenderTextures::AlbedoAndRoughness)->GetSrv());
            commandList.SetRootResource(joint::DenoiserBlurRc_WorldNormalTexture, ms_RenderResources->GetTexture(+RenderTextures::WorldNormal)->GetSrv());
            commandList.SetRootResource(joint::DenoiserBlurRc_DepthBuffer, ms_RenderResources->GetTexture(+RenderTextures::DepthStencil)->GetSrv({ .Format = g_RenderPassesConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::DenoiserBlurRc_NoisyVisibilityBuffer, noisyVisibilityBuffer.GetSrv({ .MipRange{ 0, 1 } }));
            commandList.SetRootResource(joint::DenoiserBlurRc_TemporalAccumulationBuffer, ms_RenderResources->GetTexture(+RenderTextures::TemporalAccumulationBuffer)->GetSrv());
            commandList.SetRootResource(joint::DenoiserBlurRc_ReprojectedHistoryTexture, ms_RenderResources->GetTexture(+RenderTextures::ReprojectedHistoryTexture)->GetSrv());
            commandList.SetRootResource(joint::DenoiserBlurRc_DenoisedVisibilityBuffer, denoisedVisibilityBuffer.GetUav());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ denoisedVisibilityBuffer, benzin::ResourceState::UnorderedAccess },
            );

            const DirectX::XMUINT3 dimensions{ denoisedVisibilityBuffer.GetWidth(), denoisedVisibilityBuffer.GetHeight(), 1 };
            const DirectX::XMUINT3 threadPerGroupCount{ joint::ThreadCount881_X, joint::ThreadCount881_Y, joint::ThreadCount881_Z };
            commandList.Dispatch(dimensions, threadPerGroupCount);
        }

        void RunPostBlurSubPass() const
        {

        }

    private:
        std::unique_ptr<benzin::PipelineState> m_TemporalAccumulationPso;
        std::unique_ptr<benzin::PipelineState> m_MipGenerationPso;
        std::unique_ptr<benzin::PipelineState> m_HistoryFixPso;
        std::unique_ptr<benzin::PipelineState> m_BlurPso;

        using MipGenerationConstantBuffer = benzin::ConstantBuffer<joint::MipGenerationConstants>;
        std::unique_ptr<MipGenerationConstantBuffer> m_MipGenerationConstantBuffer;

        using BlurConstantBuffer = benzin::ConstantBuffer<joint::DenoiserBlurConstants>;
        std::unique_ptr<BlurConstantBuffer> m_BlurConstantBuffer;
    };

    class DeferredLightingPass : public benzin::RenderPass
    {
    public:
        DeferredLightingPass(const benzin::Scene& scene)
            : m_Scene{ scene }
        {
            benzin::MakeUniquePtr(m_Pso, *ms_Device, benzin::GraphicsPipelineStateCreation
            {
                .DebugName = "DeferredLightingPass",
                .VertexShader{ "fullscreen_triangle.hlsl", "VsMain" },
                .PixelShader{ "deferred_lighting_pass.hlsl", "PsMain" },
                .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
                .DepthState
                {
                    .IsEnabled = false,
                    .IsWriteEnabled = false,
                },
                .RenderTargetFormats{ benzin::GraphicsFormat::Rgba8Unorm },
            });

            benzin::MakeUniquePtr(m_PassConstantBuffer, *ms_Device, "DeferredLightingPassConstantBuffer");
        }

        void OnResize(uint32_t width, uint32_t height) override
        {
            benzin::MakeUniquePtr(ms_RenderResources->GetTexture(+RenderTextures::FinalOutputTexture), *ms_Device, benzin::TextureCreation
            {
                .DebugName = "DeferredLightingPass_OutputTexture",
                .Format = benzin::CommandLineArgs::GetBackBufferFormat(),
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowRenderTarget,
            });
        }

        void OnUpdate() override
        {
            m_IsRenderingEnabled = g_RenderPassSettings.DebugOutputType == joint::DebugOutputType_None;

            m_PassConstantBuffer->UpdateConstants(joint::DeferredLightingPassConstants
            {
                .SunColor = g_RenderPassSettings.SunColor,
                .SunIntensity = g_RenderPassSettings.SunIntensity,
                .SunDirection = g_RenderPassSettings.SunDirection,
                .ActivePointLightCount = m_Scene.GetStats().PointLightCount,
            });
        }

        void OnRender() const override
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::DeferredLightingPass]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::DeferredLightingPass);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "DeferredLightingPass");

            const auto& denoisedShadowVisibilityBuffer = *ms_RenderResources->GetTexture(+RenderTextures::DenoisedShadowVisibilityBuffer);
            const auto& finalOutputTexture = *ms_RenderResources->GetTexture(+RenderTextures::FinalOutputTexture);

            commandList.SetViewport(ms_SwapChain->GetViewport());
            commandList.SetScissorRect(ms_SwapChain->GetScissorRect());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::RenderTarget },
            );

            commandList.SetRenderTargets({ finalOutputTexture.GetRtv() });
            commandList.ClearRenderTarget(finalOutputTexture.GetRtv());

            commandList.SetPipelineState(*m_Pso);

            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstantBuffer->GetActiveGpuVirtualAddress());
            commandList.SetRootResource(joint::DeferredLightingPassRc_AlbedoAndRoughnessTexture, ms_RenderResources->GetTexture(+RenderTextures::AlbedoAndRoughness)->GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_EmissiveAndMetallicTexture, ms_RenderResources->GetTexture(+RenderTextures::EmissiveAndMetallic)->GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_WorldNormalTexture, ms_RenderResources->GetTexture(+RenderTextures::WorldNormal)->GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_VelocityBuffer, ms_RenderResources->GetTexture(+RenderTextures::VelocityBuffer)->GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_DepthStencilTexture, ms_RenderResources->GetTexture(+RenderTextures::DepthStencil)->GetSrv({ .Format = g_RenderPassesConfig.DepthStencilSrvFormat }));
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

    class EnvironmentPass : public benzin::RenderPass
    {
    public:
        EnvironmentPass()
        {
            benzin::MakeUniquePtr(m_EquirectangularToCubePso, *ms_Device, benzin::ComputePipelineStateCreation
            {
                .DebugName = "EquirectangularToCube",
                .ComputeShader{ "equirectangular_to_cube_pass.hlsl", "CsMain" },
            });

            benzin::MakeUniquePtr(m_Pso, *ms_Device, benzin::GraphicsPipelineStateCreation
            {
                .DebugName = "EnvironmentPass",
                .VertexShader{ "fullscreen_triangle.hlsl", "VsMainDepth1" },
                .PixelShader{ "environment_pass.hlsl", "PsMain" },
                .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
                .DepthState
                {
                    .IsWriteEnabled = false,
                    .ComparisonFunction = benzin::ComparisonFunction::Equal,
                },
                .RenderTargetFormats{ benzin::GraphicsFormat::Rgba8Unorm },
                .DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint,
            });
        }

        void OnZeroFrameInit() override
        {
            LoadEquirectangularTexture();
            ComputeCubeMapTexture();
        }

        void OnUpdate() override
        {
            m_IsRenderingEnabled = g_RenderPassSettings.DebugOutputType == joint::DebugOutputType_None;
        }

        void OnRender() const override
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::EnvironmentPass]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::EnvironmentPass);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "EnvironmentPass");

            const auto& finalOutputTexture = *ms_RenderResources->GetTexture(+RenderTextures::FinalOutputTexture);
            const auto& depthStencilBuffer = *ms_RenderResources->GetTexture(+RenderTextures::DepthStencil);

            commandList.SetViewport(ms_SwapChain->GetViewport());
            commandList.SetScissorRect(ms_SwapChain->GetScissorRect());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::RenderTarget },
                benzin::TransitionBarrier{ depthStencilBuffer, benzin::ResourceState::DepthRead },
            );

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
            BenzinAssertExpr(benzin::LoadTextureImageFromHdrFile("scythian_tombs_2_4k.hdr", equirectangularTextureImage));

            benzin::MakeUniquePtr(m_EquirectangularTexture, *ms_Device, benzin::TextureCreation
            {
                .DebugName = equirectangularTextureImage.DebugName,
                .Format = equirectangularTextureImage.Format,
                .Width = equirectangularTextureImage.Width,
                .Height = equirectangularTextureImage.Height,
                .MipCount = 1,
            });

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList(m_EquirectangularTexture->GetSizeInBytes());
            commandList.UploadToTextureTopMip(*m_EquirectangularTexture, std::as_bytes(std::span{ equirectangularTextureImage.ImageData }));
        }

        void ComputeCubeMapTexture()
        {
            const uint32_t cubeMapSize = 1024;
            benzin::MakeUniquePtr(m_CubeTexture, *ms_Device, benzin::TextureCreation
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

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

            commandList.SetPipelineState(*m_EquirectangularToCubePso);

            commandList.SetRootResource(joint::EquirectangularToCubeRc_EquirectangularTexture, m_EquirectangularTexture->GetSrv());
            commandList.SetRootResource(joint::EquirectangularToCubeRc_OutCubeTexture, m_CubeTexture->GetUav());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ *m_CubeTexture, benzin::ResourceState::UnorderedAccess },
            );

            const DirectX::XMUINT3 dimensions{ cubeMapSize, cubeMapSize, m_CubeTexture->GetDepth() };
            const DirectX::XMUINT3 threadPerGroupCount{ joint::ThreadCount881_X, joint::ThreadCount881_Y, joint::ThreadCount881_Z };
            commandList.Dispatch(dimensions, threadPerGroupCount);
        }

    private:
        std::unique_ptr<benzin::PipelineState> m_EquirectangularToCubePso;
        std::unique_ptr<benzin::Texture> m_EquirectangularTexture;

        std::unique_ptr<benzin::PipelineState> m_Pso;
        std::unique_ptr<benzin::Texture> m_CubeTexture;
    };

    class FullScreenDebugPass : public benzin::RenderPass
    {
    public:
        FullScreenDebugPass()
        {
            benzin::MakeUniquePtr(m_Pso, *ms_Device, benzin::GraphicsPipelineStateCreation
            {
                .DebugName = "FullScreenDebugPass",
                .VertexShader{ "fullscreen_triangle.hlsl", "VsMain" },
                .PixelShader{ "fullscreen_debug_pass.hlsl", "PsMain" },
                .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
                .DepthState
                {
                    .IsEnabled = false,
                    .IsWriteEnabled = false,
                },
                .RenderTargetFormats{ benzin::GraphicsFormat::Rgba8Unorm },
            });

            benzin::MakeUniquePtr(m_PassConstantBuffer, *ms_Device, "FullScreenDebugConstantBuffer");
        }

        void OnUpdate() override
        {
            m_IsRenderingEnabled = g_RenderPassSettings.DebugOutputType != joint::DebugOutputType_None;

            m_PassConstantBuffer->UpdateConstants(joint::FullScreenDebugConstants
            {
                .OutputType = magic_enum::enum_integer(g_RenderPassSettings.DebugOutputType),
                .ViewDepthMipIndex = g_RenderPassSettings.ViewDepthMipIndex,
                .MinViewDepth = g_RenderPassSettings.MinViewDepth,
                .MaxViewDepth = g_RenderPassSettings.MaxViewDepth,
            });
        }

        void OnRender() const override
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::FullScreenDebugPass]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::FullScreenDebugPass);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "FullScreenDebugPass");

            const auto& viewDepth = *ms_RenderResources->GetTexture(+RenderTextures::ViewDepth);
            const auto& noisyShadowVisibilityBuffer = *ms_RenderResources->GetTexture(+RenderTextures::NoisyShadowVisibilityBuffer);
            const auto& temporalAccumulationBuffer = *ms_RenderResources->GetTexture(+RenderTextures::TemporalAccumulationBuffer);
            const auto& finalOutputTexture = *ms_RenderResources->GetTexture(+RenderTextures::FinalOutputTexture);

            commandList.SetViewport(ms_SwapChain->GetViewport());
            commandList.SetScissorRect(ms_SwapChain->GetScissorRect());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::RenderTarget },
            );

            commandList.SetRenderTargets({ finalOutputTexture.GetRtv() });
            commandList.ClearRenderTarget(finalOutputTexture.GetRtv());

            commandList.SetPipelineState(*m_Pso);

            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstantBuffer->GetActiveGpuVirtualAddress());
            commandList.SetRootResource(joint::FullScreenDebugRc_AlbedoAndRoughnessTexture, ms_RenderResources->GetTexture(+RenderTextures::AlbedoAndRoughness)->GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_EmissiveAndMetallicTexture, ms_RenderResources->GetTexture(+RenderTextures::EmissiveAndMetallic)->GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_WorldNormalTexture, ms_RenderResources->GetTexture(+RenderTextures::WorldNormal)->GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_VelocityBuffer, ms_RenderResources->GetTexture(+RenderTextures::VelocityBuffer)->GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_ViewDepthBuffer, viewDepth.GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_DepthBuffer, ms_RenderResources->GetTexture(+RenderTextures::DepthStencil)->GetSrv({ .Format = g_RenderPassesConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::FullScreenDebugRc_ShadowVisibilityBuffer, noisyShadowVisibilityBuffer.GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_TemporalAccumulationBuffer, temporalAccumulationBuffer.GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_ReprojectedHistoryTexture, ms_RenderResources->GetTexture(+RenderTextures::ReprojectedHistoryTexture)->GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_DenoisedShadowVisibilityBuffer, ms_RenderResources->GetTexture(+RenderTextures::DenoisedShadowVisibilityBuffer)->GetSrv());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

    private:
        using PassConstantBuffer = benzin::ConstantBuffer<joint::FullScreenDebugConstants>;

        std::unique_ptr<benzin::PipelineState> m_Pso;
        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
    };

    class BackBufferCopyPass : public benzin::RenderPass
    {
    public:
        void OnRender() const override
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::BackBufferCopy]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::BackBufferCopy);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "BackBufferCopy");

            const auto& currentBackBuffer = ms_SwapChain->GetCurrentBackBuffer();
            const auto& finalOutputTexture = *ms_RenderResources->GetTexture(+RenderTextures::FinalOutputTexture);

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::CopyDestination },
                benzin::TransitionBarrier{ finalOutputTexture, benzin::ResourceState::CopySource },
            );

            commandList.CopyResource(currentBackBuffer, finalOutputTexture);
        }
    };

    // SandboxRunner

    void SandboxRunner::Client_InitRenderPasses()
    {
        auto isRenderTextureFlippableCallback = [](uint32_t key)
        {
            const uint32_t maxKey = +magic_enum::enum_values<RenderTextures>().back();
            const uint32_t keyOfPreviousTexture = key - 1;

            return
                keyOfPreviousTexture <= maxKey && // Check for out of bounds
                !magic_enum::enum_contains<RenderTextures>(keyOfPreviousTexture); // There must be a gap between enum values, so the enum value must not exist
        };

        benzin::MakeUniquePtr(m_RenderResources, std::move(isRenderTextureFlippableCallback));

        benzin::RenderPass::SetContext(*m_Device, *m_SwapChain, *m_RenderResources);

        // The order in which render passes are added is important
        m_RenderPasses.push_back(std::make_unique<GlobalConstantBufferPass>(*m_Scene));
        m_RenderPasses.push_back(std::make_unique<GeometryPass>(*m_Scene));
        m_RenderPasses.push_back(std::make_unique<RtShadowPass>(*m_Scene));
        m_RenderPasses.push_back(std::make_unique<DenoiserPass>());
        m_RenderPasses.push_back(std::make_unique<DeferredLightingPass>(*m_Scene));
        m_RenderPasses.push_back(std::make_unique<EnvironmentPass>());
        m_RenderPasses.push_back(std::make_unique<FullScreenDebugPass>());
        m_RenderPasses.push_back(std::make_unique<benzin::ImGuiPass>(*m_ImGuiManager, +RenderTextures::FinalOutputTexture, +SandboxTiming::ImGuiPass));
        m_RenderPasses.push_back(std::make_unique<BackBufferCopyPass>());

        auto imGuiPassIt = std::next(m_RenderPasses.rbegin());
        m_ImGuiPass = dynamic_cast<benzin::ImGuiPass*>((*imGuiPassIt).get());
        BenzinEnsure(m_ImGuiPass != nullptr);
    }

    void SandboxRunner::Client_InitTools()
    {
        m_RenderPassSettingsTool = m_ImGuiManager->PushTool<RenderPassSettingsTool>(*m_Scene, g_RenderPassSettings);
        m_TimingsTool = m_ImGuiManager->PushTool<TimingsTool<SandboxTiming, SandboxTiming>>(*m_Device);
    }

    void SandboxRunner::Client_InitSceneEntities()
    {
        InitCamera();
        InitSceneEntities();
    }

    void SandboxRunner::InitCamera()
    {
        auto& perspectiveProjection = m_Scene->GetPerspectiveProjection();
        perspectiveProjection.SetLens(DirectX::XMConvertToRadians(60.0f), m_SwapChain->GetAspectRatio(), 0.1f, 1000.0f);

        auto& camera = m_Scene->GetCamera();
        camera.SetPosition({ -3.0f, 2.0f, -0.25f });
        camera.SetFrontDirection({ 1.0f, 0.0f, 0.0f });
    }

    void SandboxRunner::InitSceneEntities()
    {
        SceneMeshes sceneMeshes;

        {
            BenzinLogTimeOnScopeExit("Load and Create mesh collections");
            LoadAndCreateMeshes(sceneMeshes);
        }

        {
            BenzinLogTimeOnScopeExit("Create entities");
            CreateEntities(sceneMeshes);
        }
    }

    void SandboxRunner::Client_OnEvent(benzin::Event& event)
    {
        benzin::EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<benzin::KeyPressedEvent>([this](auto& event)
        {
            if (event.GetKeyCode() == benzin::KeyCode::F2)
            {
                m_IsAnimationEnabled = !m_IsAnimationEnabled;
            }

            return false;
        });
    }

    void SandboxRunner::Client_AfterEndFrame()
    {
        if (m_FrameRateCounter.IsIntervalPassed())
        {
            g_CpuTimings[+SandboxTiming::ImGuiPass] = m_ImGuiPass->GetRenderTime();
            m_TimingsTool->SetCpuTimings(g_CpuTimings);
        }
    }

    void SandboxRunner::LoadAndCreateMeshes(SceneMeshes& outSceneMeshes)
    {
        const auto createCylinderMeshCollection = []
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
        };

        const auto createSphereLightMeshCollection = []
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
        };

        const auto loadFromFile = [](std::string_view fileName)
        {
            BenzinLogTimeOnScopeExit("Loading MeshCollection from {}", fileName);

            benzin::MeshCollectionResource resource;
            BenzinAssertExpr(benzin::LoadMeshCollectionFromGltfFile(fileName, resource));

            return resource;
        };

        benzin::EnumArray<benzin::MeshCollectionResource, SceneMesh> meshCollectionResources;
        meshCollectionResources[+SceneMesh::Sponza] = loadFromFile("Sponza/glTF/Sponza.gltf");
        meshCollectionResources[+SceneMesh::BoomBox] = loadFromFile("BoomBox/glTF/BoomBox.gltf");
        meshCollectionResources[+SceneMesh::DamagedHelmet] = loadFromFile("DamagedHelmet/glTF/DamagedHelmet.gltf");
        meshCollectionResources[+SceneMesh::OrientationTest] = loadFromFile("OrientationTest/OrientationTest.gltf");
        meshCollectionResources[+SceneMesh::Cylinder] = createCylinderMeshCollection();
        meshCollectionResources[+SceneMesh::Sphere] = createSphereLightMeshCollection();

        for (auto&& [outSceneMesh, meshCollectionResource] : std::views::zip(outSceneMeshes, meshCollectionResources))
        {
            outSceneMesh = m_Scene->PushMeshCollection(std::move(meshCollectionResource));
        }
    }

    void SandboxRunner::CreateEntities(const SceneMeshes& sceneMeshes)
    {
        auto& entityRegistry = m_Scene->GetEntityRegistry();

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::Sponza];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetRotation({ 0.0f, DirectX::XM_PI, 0.0f });
            tc.SetTranslation({ 5.0f, 0.0f, 0.0f });
        }

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::BoomBox];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetRotation({ 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f });
            tc.SetScale({ 30.0f, 30.0f, 30.0f });
            tc.SetTranslation({ 0.0f, 0.6f, 0.0f });

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(entity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle, const benzin::TickTimer& tickTimer)
            {
                if (!m_IsAnimationEnabled)
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                auto rotation = tc.GetRotation();
                rotation.x += 0.0001f * tickTimer.GetDeltaTimeInMs();
                rotation.z += 0.0002f * tickTimer.GetDeltaTimeInMs();

                tc.SetRotation(rotation);
            };
        }

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::DamagedHelmet];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetRotation({ 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f });
            tc.SetScale({ 0.4f, 0.4f, 0.4f });
            tc.SetTranslation({ 1.0f, 0.5f, -0.5f });

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(entity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle, const benzin::TickTimer& tickTimer)
            {
                if (!m_IsAnimationEnabled)
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                auto rotation = tc.GetRotation();
                rotation.x += 0.0001f * tickTimer.GetDeltaTimeInMs();
                rotation.y -= 0.00015f * tickTimer.GetDeltaTimeInMs();

                tc.SetRotation(rotation);
            };
        }

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::OrientationTest];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetScale({ 0.05f, 0.05f, 0.05f });
            tc.SetTranslation({ 2.5f, 0.4f, -0.25f });
        }

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::Cylinder];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetScale({ 0.1f, 1.5f, 0.1f });
            tc.SetTranslation({ -1.5f, 0.4f, -0.25f });
        }

        {
            constexpr float sphereLightRadius = 0.05f;

            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::Sphere];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetScale({ sphereLightRadius, sphereLightRadius, sphereLightRadius });
            tc.SetTranslation({ 0.5f, 1.5f, -0.25f });

            auto& plc = entityRegistry.emplace<benzin::PointLightComponent>(entity);
            plc.Color = { 1.0f, 1.0f, 1.0f };
            plc.Intensity = 10.0f;
            plc.Range = 30.0f;
            plc.GeometryRadius = sphereLightRadius;

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(entity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle, const benzin::TickTimer& tickTimer)
            {
                static constexpr float travelRadius = 1.0f;
                static constexpr float travelSpeed = 0.0004f;

                static std::chrono::microseconds elapsedTime;

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                static const float startX = tc.GetTranslation().x;
                static const float startZ = tc.GetTranslation().z;

                if (m_IsAnimationEnabled)
                {
                    elapsedTime += tickTimer.GetDeltaTime();

                    auto translation = tc.GetTranslation();
                    translation.x = startX + travelRadius * std::cos(travelSpeed * benzin::ToFloatMs(elapsedTime));
                    translation.z = startZ + travelRadius * std::sin(travelSpeed * benzin::ToFloatMs(elapsedTime));

                    tc.SetTranslation(translation);
                }
            };

            BenzinAssert(m_RenderPassSettingsTool != nullptr);
            m_RenderPassSettingsTool->SetPointLightEntity(entity);
        }
    }

}
