#include "sandbox/bootstrap.hpp"
#include "sandbox/sandbox_runner.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/core/engine_math.hpp>
#include <benzin/core/logger.hpp>
#include <benzin/core/math.hpp>
#include <benzin/engine/camera.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/pipeline_state_manager.hpp>
#include <benzin/graphics/rt_acceleration_structures.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/tools/render_settings_tool.hpp>
#include <benzin/tools/render_viewport_tool.hpp>

#include <shaders/joint/constant_buffer_types.hpp>
#include <shaders/joint/enum_types.hpp>
#include <shaders/joint/root_constants.hpp>
#include <shaders/joint/structured_buffer_types.hpp>

#include "sandbox/sandbox_render_settings.hpp"

namespace sandbox
{

    static uint32_t GetTimingIndent(SandboxTiming timing)
    {
        switch (timing)
        {
            case SandboxTiming::DenoiserPass_Accumulation:
            case SandboxTiming::DenoiserPass_Mips:
            case SandboxTiming::DenoiserPass_HistoryFix:
            case SandboxTiming::DenoiserPass_Blur: return 2;
        }

        return 0;
    }

    static Timings<SandboxTiming> g_CpuTimings;

    enum class Texture : uint32_t
    {
        // GBuffer
        AlbedoAndRoughness,
        EmissiveAndMetallic,
        WorldNormal,
        VelocityBuffer,
        DepthStencil,
        ViewDepth = DepthStencil + 2,

        // RtShadows
        NoisyShadowVisibility = ViewDepth + 2,

        // Denoiser
        TemporalAccumulation = NoisyShadowVisibility + 2,
        ReprojectedShadowHistory,
        DenoisedShadowVisibility = ReprojectedShadowHistory + 2,

        Final,
        ImGui,

        Count,
    };
    BenzinEnableUnaryPlusForEnum(Texture);

    struct RenderPassConfig
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

    static constexpr RenderPassConfig g_RenderPassConfig;

    class GlobalConstantBufferPass : public benzin::RenderPass
    {
    public:
        explicit GlobalConstantBufferPass(benzin::Scene& scene)
            : m_Scene{ scene }
        {
            benzin::MakeUniquePtr(m_FrameConstantBuffer, *ms_Device, "FrameConstantBuffer");
        }

        bool IsDependentOnViewport() const override { return false; }

        void OnUpdate(const benzin::TickTimer& tickTimer) override
        {
            const auto& rtShadowSettings = ms_Settings->GetSection<RtShadowsSettings>();
            const auto& denoiserSettings = ms_Settings->GetSection<DenoiserSettings>();

            m_FrameConstantBuffer->UpdateConstants(joint::FrameConstants
            {
                .RenderResolution{ (float)GetRenderViewportWidth(), (float)GetRenderViewportHeight() },
                .InvRenderResolution{ 1.0f / (float)GetRenderViewportWidth(), 1.0f / (float)GetRenderViewportHeight() },
                .CpuFrameIndex = (uint32_t)ms_Device->GetCpuFrameIndex(),
                .FrameTimeInSec = tickTimer.GetDeltaTimeInSec(),
                .ElapsedTimeInSec = tickTimer.GetElapsedTimeInSec(),
                .IsRtShadowsEnabled = rtShadowSettings.IsRtShadowEnabled,
                .IsDenoiserEnabled = denoiserSettings.IsDenoiserEnabled,
                .MaxTemporalAccumulationCount = denoiserSettings.MaxTemporalAccumulationCount,
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
                    g_RenderPassConfig.GBufferColor0Format,
                    g_RenderPassConfig.GBufferColor1Format,
                    g_RenderPassConfig.GBufferColor2Format,
                    g_RenderPassConfig.GBufferColor3Format,
                    g_RenderPassConfig.GBufferColor4Format,
                },
                .DepthStencilFormat = g_RenderPassConfig.DepthStencilFormat,
            });
        }

        ~GeometryPass()
        {
            ms_Device->GetPipelineStateManager().DestroyPipelineState(m_Pso);

            ms_Resources->DestroyTexture(+Texture::AlbedoAndRoughness);
            ms_Resources->DestroyTexture(+Texture::EmissiveAndMetallic);
            ms_Resources->DestroyTexture(+Texture::WorldNormal);
            ms_Resources->DestroyTexture(+Texture::VelocityBuffer);
            ms_Resources->DestroyTexture(+Texture::DepthStencil);
            ms_Resources->DestroyTexture(+Texture::ViewDepth);
        }

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize(uint32_t width, uint32_t height) override
        {
            const auto createGBufferTexture = [&](
                Texture textureIndex,
                benzin::GraphicsFormat format,
                benzin::TextureFlag flag
            )
            {
                ms_Resources->CreateTexture(+textureIndex, benzin::TextureCreation
                {
                    .DebugName = magic_enum::enum_name(textureIndex),
                    .Format = format,
                    .Width = width,
                    .Height = height,
                    .MipCount = 1,
                    .Flags = flag,
                });
            };

            createGBufferTexture(Texture::AlbedoAndRoughness, g_RenderPassConfig.GBufferColor0Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(Texture::EmissiveAndMetallic, g_RenderPassConfig.GBufferColor1Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(Texture::WorldNormal, g_RenderPassConfig.GBufferColor2Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(Texture::VelocityBuffer, g_RenderPassConfig.GBufferColor3Format, benzin::TextureFlag::AllowRenderTarget);
            createGBufferTexture(Texture::DepthStencil, g_RenderPassConfig.DepthStencilFormat, benzin::TextureFlag::AllowDepthStencil);

            ms_Resources->CreateTexture(+Texture::ViewDepth, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(Texture::ViewDepth),
                .Format = g_RenderPassConfig.GBufferColor4Format,
                .Width = width,
                .Height = height,
                .MipCount = 5,
                .Flags = benzin::TextureFlag::AllowRenderTarget | benzin::TextureFlag::AllowUnorderedAccess,
            });
        }

        void OnRender() const override
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::GeometryPass]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::GeometryPass);

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

        benzin::PipelineState* m_Pso = nullptr;
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

        ~RtShadowPass()
        {
            ms_Resources->DestroyTexture(+Texture::NoisyShadowVisibility);
        }

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize(uint32_t width, uint32_t height) override
        {
            ms_Resources->CreateTexture(+Texture::NoisyShadowVisibility, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(Texture::NoisyShadowVisibility),
                .Format = benzin::GraphicsFormat::R32Float,
                .Width = width,
                .Height = height,
                .MipCount = 5,
                .Flags = benzin::TextureFlag::AllowUnorderedAccess,
            });
        }

        void OnUpdate() override
        {
            const auto& settings = ms_Settings->GetSection<RtShadowsSettings>();

            m_PassConstantBuffer->UpdateConstants(joint::RtShadowPassConstants
            {
                .RaysPerPixel = settings.RaysPerPixel,
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

            const auto& visibilityTexture = ms_Resources->GetTexture(+Texture::NoisyShadowVisibility);

            d3d12CommandList->SetPipelineState1(m_D3D12RaytracingStateObject.Get());

            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstantBuffer->GetActiveGpuVirtualAddress());
            commandList.SetRootResource(joint::RtShadowRc_GBufferWorldNormalTexture, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(joint::RtShadowRc_GBufferDepthTexture, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv({ .Format = g_RenderPassConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::RtShadowRc_PointLightBuffer, m_Scene.GetPointLightBufferStructuredSrv());
            commandList.SetRootResource(joint::RtShadowRc_VisiblityBuffer, visibilityTexture.GetUav());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ visibilityTexture, benzin::ResourceState::UnorderedAccess },
            );

            const D3D12_DISPATCH_RAYS_DESC d3d12DispatchRayDesc
            {
                .RayGenerationShaderRecord
                {
                    .StartAddress = m_RayGenShaderTable->GetGpuVirtualAddress(),
                    .SizeInBytes = m_RayGenShaderTable->GetNotAlignedSize(),
                },
                .MissShaderTable
                {
                    .StartAddress = m_MissShaderTable->GetGpuVirtualAddress(),
                    .SizeInBytes = m_MissShaderTable->GetNotAlignedSize(),
                    .StrideInBytes = m_MissShaderTable->GetElementSize(),
                },
                .HitGroupTable
                {
                    .StartAddress = m_HitGroupShaderTable->GetGpuVirtualAddress(),
                    .SizeInBytes = m_HitGroupShaderTable->GetNotAlignedSize(),
                    .StrideInBytes = m_HitGroupShaderTable->GetElementSize(),
                },
                .CallableShaderTable
                {
                    .StartAddress = 0,
                    .SizeInBytes = 0,
                    .StrideInBytes = 0,
                },
                .Width = visibilityTexture.GetWidth(),
                .Height = visibilityTexture.GetHeight(),
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
            const std::span libraryDxil = ms_Device->GetBackend().GetShaderManager().GetShaderDxil(benzin::ShaderInfo{ benzin::ShaderType::Library, "rt_shadow_pass.hlsl" });

            const D3D12_DXIL_LIBRARY_DESC d3d12DXILLibraryDesc
            {
                .DXILLibrary
                {
                    .pShaderBytecode = libraryDxil.data(),
                    .BytecodeLength = libraryDxil.size(),
                },
                .NumExports = 0,
                .pExports = nullptr,
            };

            // 3. D3D12_HIT_GROUP_DESC
            const D3D12_HIT_GROUP_DESC d3d12HitGroupDesc
            {
                .HitGroupExport = g_RenderPassConfig.HitGroupName.data(),
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
                const auto shaderIdentifier = std::span{ (const std::byte*)rawShaderIdentifier, benzin::GfxConfig::s_ShaderIdentifierSize };

                return std::make_unique<benzin::Buffer>(*ms_Device, benzin::BufferCreation
                {
                    .DebugName = std::format("{}ShaderTable", benzin::ToNarrowString(identiferName)),
                    .ElementSize = benzin::GfxConfig::s_RayTracingShaderRecordAlignment,
                    .ElementCount = 1,
                    .Flags = benzin::BufferFlag::UploadBuffer,
                    .InitialData = shaderIdentifier,
                });
            };

            m_RayGenShaderTable = CreateShaderTable(g_RenderPassConfig.RayGenShaderName);
            m_MissShaderTable = CreateShaderTable(g_RenderPassConfig.MissShaderName);
            m_HitGroupShaderTable = CreateShaderTable(g_RenderPassConfig.HitGroupName);
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
            auto& pipelineStateManager = ms_Device->GetPipelineStateManager();

            m_TemporalAccumulationPso = pipelineStateManager.CreatePipelineState(benzin::ComputePipelineStateCreation
            {
                .DebugName = "DenoiserTemporalAccumulation",
                .CsFileName = "denoiser_temporal_accumulation_pass.hlsl",
            });

            m_MipGenerationPso = pipelineStateManager.CreatePipelineState(benzin::ComputePipelineStateCreation
            {
                .DebugName = "DenoiserMipGeneration",
                .CsFileName = "mip_generation_pass.hlsl",
            });

            m_HistoryFixPso = pipelineStateManager.CreatePipelineState(benzin::ComputePipelineStateCreation
            {
                .DebugName = "DenoiserHistoryFix",
                .CsFileName = "denoiser_history_fix_pass.hlsl",
            });

            m_BlurPso = pipelineStateManager.CreatePipelineState(benzin::ComputePipelineStateCreation
            {
                .DebugName = "DenoiserBlur",
                .CsFileName = "denoiser_blur_pass.hlsl",
            });

            benzin::MakeUniquePtr(m_TemporalAccumulationConstantBuffer, *ms_Device, "TemporalAccumulationConstants");
            benzin::MakeUniquePtr(m_DepthMipGenerationConstantBuffer, *ms_Device, "DepthMipGenerationConstantBuffer");
            benzin::MakeUniquePtr(m_ColorMipGenerationConstantBuffer, *ms_Device, "ColorMipGenerationConstantBuffer");
            benzin::MakeUniquePtr(m_HistoryFixConstantBuffer, *ms_Device, "HistoryFixConstantBuffer");
            benzin::MakeUniquePtr(m_BlurConstantBuffer, *ms_Device, "BlurConstantBuffer");
        }

        ~DenoiserPass()
        {
            auto& pipelineStateManager = ms_Device->GetPipelineStateManager();
            pipelineStateManager.DestroyPipelineState(m_TemporalAccumulationPso);
            pipelineStateManager.DestroyPipelineState(m_MipGenerationPso);
            pipelineStateManager.DestroyPipelineState(m_HistoryFixPso);
            pipelineStateManager.DestroyPipelineState(m_BlurPso);

            ms_Resources->DestroyTexture(+Texture::TemporalAccumulation);
            ms_Resources->DestroyTexture(+Texture::ReprojectedShadowHistory);
            ms_Resources->DestroyTexture(+Texture::DenoisedShadowVisibility);
        }

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize(uint32_t width, uint32_t height) override
        {
            ms_Resources->CreateTexture(+Texture::TemporalAccumulation, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(Texture::TemporalAccumulation),
                .Format = benzin::GraphicsFormat::R32Float,
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowUnorderedAccess,
            });

            ms_Resources->CreateTexture(+Texture::ReprojectedShadowHistory, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(Texture::ReprojectedShadowHistory),
                .Format = benzin::GraphicsFormat::R32Float,
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowUnorderedAccess,
            });

            ms_Resources->CreateTexture(+Texture::DenoisedShadowVisibility, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(Texture::DenoisedShadowVisibility),
                .Format = benzin::GraphicsFormat::R32Float,
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowUnorderedAccess,
            });
        }

        void OnUpdate()
        {
            m_TemporalAccumulationConstantBuffer->UpdateConstants(
            {
                .IsAccumulationEnabled = (ms_Device->GetCpuFrameIndex() % 30) == 0,
            });

            const auto& settings = ms_Settings->GetSection<DenoiserBlurSettings>();
            m_BlurConstantBuffer->UpdateConstants(joint::DenoiserBlurConstants
            {
                .SpecularAccumulationCurve = settings.SpecularAccumulationCurve,
                .SpecularAccumulationBasePower = settings.SpecularAccumulationBasePower,
                .IsDenoiserAntilagEnabled = settings.IsDenoiserAntilagEnabled,
                .IsGeometryWeightUsed = settings.IsGeometryWeightUsed,
                .IsNormalWeightUsed = settings.IsNormalWeightUsed,
                .IsRoughnessWeightUsed = settings.IsRoughnessWeightUsed,
                .GeometryWeightSensitivity = settings.GeometryWeightSensitivity,
                .MinBlurRadius = settings.MinBlurRadius,
                .MaxBlurRadius = settings.MaxBlurRadius,
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
        }

    private:
        void RunTemporalAccumulationSubPass() const
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::DenoiserPass_Accumulation]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::DenoiserPass_Accumulation);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "Denoiser_TemporalAccumulation");

            const auto& previousViewDepth = ms_Resources->GetPreviousTexture(+Texture::ViewDepth);
            const auto& previousAccumulationTexture = ms_Resources->GetPreviousTexture(+Texture::TemporalAccumulation);
            const auto& previousDenoisedVisibilityTexture = ms_Resources->GetPreviousTexture(+Texture::DenoisedShadowVisibility);
            const auto& accumulationTexture = ms_Resources->GetTexture(+Texture::TemporalAccumulation);
            const auto& reprojectedHistoryTexture = ms_Resources->GetTexture(+Texture::ReprojectedShadowHistory);

            commandList.SetPipelineState(*m_TemporalAccumulationPso);

            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_TemporalAccumulationConstantBuffer->GetActiveGpuVirtualAddress());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_WorldNormalTexture, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_VelocityBuffer, ms_Resources->GetTexture(+Texture::VelocityBuffer).GetSrv());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_DepthBuffer, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv({ .Format = g_RenderPassConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_PreviousViewDepthBuffer, previousViewDepth.GetSrv({ .MipRange{ 0, 1 } }));
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_PreviousTemporalAccumulationBuffer, previousAccumulationTexture.GetSrv());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_PreviousDenoisedVisibilityBuffer, previousDenoisedVisibilityTexture.GetSrv());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_TemporalAccumulationBuffer, accumulationTexture.GetUav());
            commandList.SetRootResource(joint::DenoiserTemporalAccumulationRc_ReprojectedHistoryTexture, reprojectedHistoryTexture.GetUav());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ accumulationTexture, benzin::ResourceState::UnorderedAccess },
                benzin::TransitionBarrier{ reprojectedHistoryTexture, benzin::ResourceState::UnorderedAccess },
            );

            const DirectX::XMUINT3 dimensions{ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 };
            commandList.Dispatch(dimensions, joint::g_ThreadPerGroupCount881);
        }

        void RunMipGenerationSubPass() const
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::DenoiserPass_Mips]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::DenoiserPass_Mips);

            const auto& viewDepth = ms_Resources->GetTexture(+Texture::ViewDepth);
            const auto& visibilityTexture = ms_Resources->GetTexture(+Texture::NoisyShadowVisibility);

            BenzinAssert(viewDepth.GetWidth() == visibilityTexture.GetWidth() && viewDepth.GetHeight() == visibilityTexture.GetHeight());

            const uint32_t dispatchMipIndex = 1;
            const auto dispatchMipWidth = viewDepth.GetMipWidth(dispatchMipIndex);
            const auto dispatchMipHeight = viewDepth.GetMipHeight(dispatchMipIndex);

            const auto& settings = ms_Settings->GetSection<DenoiserMipGenerationSettings>();

            m_DepthMipGenerationConstantBuffer->UpdateConstants(joint::MipGenerationConstants
            {
                .InvDispatchDimensions{ 1.0f / dispatchMipWidth, 1.0f / dispatchMipHeight },
                .IsSourceWidthOdd = benzin::IsOddQuickly(viewDepth.GetWidth()),
                .IsSourceHeightOdd = benzin::IsOddQuickly(viewDepth.GetHeight()),
                .DestinationMipCount = 4,
                .FilterType = settings.DepthFilterType,
            });

            m_ColorMipGenerationConstantBuffer->UpdateConstants(joint::MipGenerationConstants
            {
                .InvDispatchDimensions{ 1.0f / dispatchMipWidth, 1.0f / dispatchMipHeight },
                .IsSourceWidthOdd = benzin::IsOddQuickly(viewDepth.GetWidth()),
                .IsSourceHeightOdd = benzin::IsOddQuickly(viewDepth.GetHeight()),
                .DestinationMipCount = 4,
                .FilterType = joint::MipGenerationFilterType_Average, // #TODO
            });

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

            BenzinPushGpuEvent(commandList, "Denoiser_MipGeneration");

            commandList.SetPipelineState(*m_MipGenerationPso);

            const auto dispatchTexture = [&](const benzin::Texture& texture, const MipGenerationConstantBuffer& constantBuffer)
            {
                commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, constantBuffer.GetActiveGpuVirtualAddress());

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
                commandList.Dispatch(dimensions, joint::g_ThreadPerGroupCount881);
            };

            dispatchTexture(viewDepth, *m_DepthMipGenerationConstantBuffer);
            dispatchTexture(visibilityTexture, *m_ColorMipGenerationConstantBuffer);
        }

        void RunHistoryFixSubPass() const
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::DenoiserPass_HistoryFix]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::DenoiserPass_HistoryFix);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "Denoiser_HistoryFix");

            const auto& settings = ms_Settings->GetSection<DenoiserHistoryFixSettings>();

            m_HistoryFixConstantBuffer->UpdateConstants(joint::DenoiserHistoryFixConstants
            {
                .IsHistoryFixEnabled = settings.IsHistoryFixEnabled,
                .IsViewDepthUsedForWeights = settings.IsViewDepthUsedForWeights,
            });

            const auto& accumulationTexture = ms_Resources->GetTexture(+Texture::TemporalAccumulation);
            const auto& viewDepth = ms_Resources->GetTexture(+Texture::ViewDepth);
            const auto& noisyVisibilityTexture = ms_Resources->GetTexture(+Texture::NoisyShadowVisibility);
            const auto& reprojectedHistoryTexture = ms_Resources->GetTexture(+Texture::ReprojectedShadowHistory);

            commandList.SetPipelineState(*m_HistoryFixPso);
            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_HistoryFixConstantBuffer->GetActiveGpuVirtualAddress());

            commandList.SetRootResource(joint::DenoiserHistoryFixRc_GBufferAlbedoAndRoughness, ms_Resources->GetTexture(+Texture::AlbedoAndRoughness).GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_TemporalAccumulationBuffer, accumulationTexture.GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_ViewDepthBuffer, viewDepth.GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_NoisyVisibilityBuffer, noisyVisibilityTexture.GetSrv());
            commandList.SetRootResource(joint::DenoiserHistoryFixRc_ReprojectedHistoryTexture, reprojectedHistoryTexture.GetUav());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ reprojectedHistoryTexture, benzin::ResourceState::UnorderedAccess },
            );

            const DirectX::XMUINT3 dimensions{ reprojectedHistoryTexture.GetWidth(), reprojectedHistoryTexture.GetHeight(), 1 };
            commandList.Dispatch(dimensions, joint::g_ThreadPerGroupCount881);
        }

        void RunBlurSubPass() const
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::DenoiserPass_Blur]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::DenoiserPass_Blur);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "Denoiser_Blur");

            const auto& noisyVisibilityTexture = ms_Resources->GetTexture(+Texture::NoisyShadowVisibility);
            const auto& temporalAccumulationTexture = ms_Resources->GetTexture(+Texture::TemporalAccumulation);
            const auto& denoisedVisibilityTexture = ms_Resources->GetTexture(+Texture::DenoisedShadowVisibility);

            commandList.SetPipelineState(*m_BlurPso);

            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_BlurConstantBuffer->GetActiveGpuVirtualAddress());
            commandList.SetRootResource(joint::DenoiserBlurRc_AlbedoAndRoughnessTexture, ms_Resources->GetTexture(+Texture::AlbedoAndRoughness).GetSrv());
            commandList.SetRootResource(joint::DenoiserBlurRc_WorldNormalTexture, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(joint::DenoiserBlurRc_DepthBuffer, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv({ .Format = g_RenderPassConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::DenoiserBlurRc_VelocityTexture, ms_Resources->GetTexture(+Texture::VelocityBuffer).GetSrv());
            commandList.SetRootResource(joint::DenoiserBlurRc_NoisyVisibilityBuffer, noisyVisibilityTexture.GetSrv({ .MipRange{ 0, 1 } }));
            commandList.SetRootResource(joint::DenoiserBlurRc_ReprojectedHistoryTexture, ms_Resources->GetTexture(+Texture::ReprojectedShadowHistory).GetSrv());
            commandList.SetRootResource(joint::DenoiserBlurRc_TemporalAccumulationBuffer, temporalAccumulationTexture.GetUav());
            commandList.SetRootResource(joint::DenoiserBlurRc_DenoisedVisibilityBuffer, denoisedVisibilityTexture.GetUav());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ temporalAccumulationTexture, benzin::ResourceState::UnorderedAccess },
                benzin::TransitionBarrier{ denoisedVisibilityTexture, benzin::ResourceState::UnorderedAccess },
            );

            const DirectX::XMUINT3 dimensions{ denoisedVisibilityTexture.GetWidth(), denoisedVisibilityTexture.GetHeight(), 1 };
            commandList.Dispatch(dimensions, joint::g_ThreadPerGroupCount881);
        }

    private:
        benzin::PipelineState* m_TemporalAccumulationPso = nullptr;
        benzin::PipelineState* m_MipGenerationPso = nullptr;
        benzin::PipelineState* m_HistoryFixPso = nullptr;
        benzin::PipelineState* m_BlurPso = nullptr;

        using TemporalAccumulationConstantBuffer = benzin::ConstantBuffer<joint::DenoiserTemporalAccumulationConstants>;
        using MipGenerationConstantBuffer = benzin::ConstantBuffer<joint::MipGenerationConstants>;
        using HistoryFixConstantBuffer = benzin::ConstantBuffer<joint::DenoiserHistoryFixConstants>;
        using BlurConstantBuffer = benzin::ConstantBuffer<joint::DenoiserBlurConstants>;

        std::unique_ptr<TemporalAccumulationConstantBuffer> m_TemporalAccumulationConstantBuffer;
        std::unique_ptr<MipGenerationConstantBuffer> m_DepthMipGenerationConstantBuffer;
        std::unique_ptr<MipGenerationConstantBuffer> m_ColorMipGenerationConstantBuffer;
        std::unique_ptr<HistoryFixConstantBuffer> m_HistoryFixConstantBuffer;
        std::unique_ptr<BlurConstantBuffer> m_BlurConstantBuffer;
    };

    class DeferredLightingPass : public benzin::RenderPass
    {
    public:
        DeferredLightingPass(const benzin::Scene& scene)
            : m_Scene{ scene }
        {
            m_Pso = ms_Device->GetPipelineStateManager().CreatePipelineState(benzin::GraphicsPipelineStateCreation
            {
                .DebugName = "DeferredLightingPass",
                .VsFileName = "fullscreen_triangle.hlsl",
                .PsFileName = "deferred_lighting_pass.hlsl",
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

        ~DeferredLightingPass()
        {
            ms_Device->GetPipelineStateManager().DestroyPipelineState(m_Pso);

            ms_Resources->DestroyTexture(+Texture::Final);
        }

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize(uint32_t width, uint32_t height) override
        {
            ms_Resources->CreateTexture(+Texture::Final, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(Texture::Final),
                .Format = (benzin::GraphicsFormat)benzin::CommandLineArgs::GetU32("BackBufferFormat"),
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowRenderTarget,
            });
        }

        void OnUpdate() override
        {
            const auto& deferredLightingSettings = ms_Settings->GetSection<DeferredLightingSettings>();
            const auto& fullScreenDebugSettings = ms_Settings->GetSection<FullScreenDebugSettings>();

            m_IsRenderingEnabled = fullScreenDebugSettings.DebugOutputType == joint::DebugOutputType_None;

            m_PassConstantBuffer->UpdateConstants(joint::DeferredLightingPassConstants
            {
                .SunColor = deferredLightingSettings.SunColor,
                .SunIntensity = deferredLightingSettings.SunIntensity,
                .SunDirection = deferredLightingSettings.SunDirection,
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

            const auto& denoisedShadowVisibilityTexture = ms_Resources->GetTexture(+Texture::DenoisedShadowVisibility);
            const auto& finalTexture = ms_Resources->GetTexture(+Texture::Final);

            commandList.SetViewport(ms_RenderViewport);
            commandList.SetScissorRect(ms_RenderScissorRect);

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ finalTexture, benzin::ResourceState::RenderTarget },
            );

            commandList.SetRenderTargets({ finalTexture.GetRtv() });
            commandList.ClearRenderTarget(finalTexture);

            commandList.SetPipelineState(*m_Pso);

            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstantBuffer->GetActiveGpuVirtualAddress());
            commandList.SetRootResource(joint::DeferredLightingPassRc_AlbedoAndRoughnessTexture, ms_Resources->GetTexture(+Texture::AlbedoAndRoughness).GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_EmissiveAndMetallicTexture, ms_Resources->GetTexture(+Texture::EmissiveAndMetallic).GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_WorldNormalTexture, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_VelocityBuffer, ms_Resources->GetTexture(+Texture::VelocityBuffer).GetSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_DepthStencilTexture, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv({ .Format = g_RenderPassConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::DeferredLightingPassRc_PointLightBuffer, m_Scene.GetPointLightBufferStructuredSrv());
            commandList.SetRootResource(joint::DeferredLightingPassRc_ShadowVisibilityBuffer, denoisedShadowVisibilityTexture.GetSrv());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

    private:
        const benzin::Scene& m_Scene;

        benzin::PipelineState* m_Pso = nullptr;

        using PassConstantBuffer = benzin::ConstantBuffer<joint::DeferredLightingPassConstants>;
        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
    };

    class EnvironmentPass : public benzin::RenderPass
    {
    public:
        EnvironmentPass()
        {
            m_Pso = ms_Device->GetPipelineStateManager().CreatePipelineState(benzin::GraphicsPipelineStateCreation
            {
                .DebugName = "EnvironmentPass",
                .VsFileName = "fullscreen_triangle.hlsl",
                .VsEntryPoint = "VsMainDepth1",
                .PsFileName = "environment_pass.hlsl",
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

        ~EnvironmentPass()
        {
            ms_Device->GetPipelineStateManager().DestroyPipelineState(m_Pso);
        }

        bool IsDependentOnViewport() const override { return true; }

        void OnZeroFrameInit() override
        {
            std::unique_ptr equirectangularTexture = LoadEquirectangularTexture();
            ComputeCubeMapTexture(*equirectangularTexture);
        }

        void OnUpdate() override
        {
            const auto& settings = ms_Settings->GetSection<FullScreenDebugSettings>();

            m_IsRenderingEnabled = settings.DebugOutputType == joint::DebugOutputType_None;
        }

        void OnRender() const override
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::EnvironmentPass]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::EnvironmentPass);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "EnvironmentPass");

            const auto& finalTexture = ms_Resources->GetTexture(+Texture::Final);
            const auto& depthStencilBuffer = ms_Resources->GetTexture(+Texture::DepthStencil);

            commandList.SetViewport(ms_RenderViewport);
            commandList.SetScissorRect(ms_RenderScissorRect);

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ finalTexture, benzin::ResourceState::RenderTarget },
                benzin::TransitionBarrier{ depthStencilBuffer, benzin::ResourceState::DepthRead },
            );

            commandList.SetRenderTargets({ finalTexture.GetRtv() }, &depthStencilBuffer.GetDsv());

            commandList.SetPipelineState(*m_Pso);
            commandList.SetRootResource(joint::EnvironmentPassRc_CubeMapTexture, m_CubeTexture->GetSrv());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

    private:
        std::unique_ptr<benzin::Texture> LoadEquirectangularTexture()
        {
            benzin::TextureImage equirectangularTextureImage;
            BenzinAssertExpr(benzin::LoadTextureImageFromHdrFile("scythian_tombs_2_4k.hdr", equirectangularTextureImage));

            auto equirectangularTexture = std::make_unique<benzin::Texture>(*ms_Device, benzin::TextureCreation
            {
                .DebugName = equirectangularTextureImage.DebugName,
                .Format = equirectangularTextureImage.Format,
                .Width = equirectangularTextureImage.Width,
                .Height = equirectangularTextureImage.Height,
                .MipCount = 1,
            });

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList(equirectangularTexture->GetSize());
            commandList.UploadToTextureTopMip(*equirectangularTexture, std::as_bytes(std::span{ equirectangularTextureImage.ImageData }));
        
            return equirectangularTexture;
        }

        void ComputeCubeMapTexture(benzin::Texture& equirectangularTexture)
        {
            auto& pipelineStateManager = ms_Device->GetPipelineStateManager();

            auto* equirectangularToCubePso = pipelineStateManager.CreatePipelineState(benzin::ComputePipelineStateCreation
            {
                .DebugName = "EquirectangularToCube",
                .CsFileName = "equirectangular_to_cube_pass.hlsl",
            });
            BenzinExecuteOnScopeExit([&]
            {
                pipelineStateManager.DestroyPipelineState(equirectangularToCubePso);
            });

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

            commandList.SetPipelineState(*equirectangularToCubePso);

            commandList.SetRootResource(joint::EquirectangularToCubeRc_EquirectangularTexture, equirectangularTexture.GetSrv());
            commandList.SetRootResource(joint::EquirectangularToCubeRc_OutCubeTexture, m_CubeTexture->GetUav());

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ *m_CubeTexture, benzin::ResourceState::UnorderedAccess },
            );

            const DirectX::XMUINT3 dimensions{ cubeMapSize, cubeMapSize, m_CubeTexture->GetDepth() };
            commandList.Dispatch(dimensions, joint::g_ThreadPerGroupCount881);
        }

    private:
        benzin::PipelineState* m_Pso = nullptr;
        std::unique_ptr<benzin::Texture> m_CubeTexture;
    };

    class FullScreenDebugPass : public benzin::RenderPass
    {
    public:
        FullScreenDebugPass()
        {
            m_Pso = ms_Device->GetPipelineStateManager().CreatePipelineState(benzin::GraphicsPipelineStateCreation
            {
                .DebugName = "FullScreenDebugPass",
                .VsFileName = "fullscreen_triangle.hlsl",
                .PsFileName = "fullscreen_debug_pass.hlsl",
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

        ~FullScreenDebugPass()
        {
            ms_Device->GetPipelineStateManager().DestroyPipelineState(m_Pso);
        }

        bool IsDependentOnViewport() const override { return true; }

        void OnUpdate() override
        {
            const auto& settings = ms_Settings->GetSection<FullScreenDebugSettings>();

            m_IsRenderingEnabled = settings.DebugOutputType != joint::DebugOutputType_None;

            m_PassConstantBuffer->UpdateConstants(joint::FullScreenDebugConstants
            {
                .OutputType = magic_enum::enum_integer(settings.DebugOutputType),
                .ViewDepthMipIndex = settings.ViewDepthMipIndex,
                .MinViewDepth = settings.MinViewDepth,
                .MaxViewDepth = settings.MaxViewDepth,
            });
        }

        void OnRender() const override
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::FullScreenDebugPass]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::FullScreenDebugPass);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "FullScreenDebugPass");

            const auto& viewDepth = ms_Resources->GetTexture(+Texture::ViewDepth);
            const auto& noisyShadowVisibilityTexture = ms_Resources->GetTexture(+Texture::NoisyShadowVisibility);
            const auto& temporalAccumulationTexture = ms_Resources->GetTexture(+Texture::TemporalAccumulation);
            const auto& finalTexture = ms_Resources->GetTexture(+Texture::Final);

            commandList.SetViewport(ms_RenderViewport);
            commandList.SetScissorRect(ms_RenderScissorRect);

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ finalTexture, benzin::ResourceState::RenderTarget },
            );

            commandList.SetRenderTargets({ finalTexture.GetRtv() });
            commandList.ClearRenderTarget(finalTexture);

            commandList.SetPipelineState(*m_Pso);

            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstantBuffer->GetActiveGpuVirtualAddress());
            commandList.SetRootResource(joint::FullScreenDebugRc_AlbedoAndRoughnessTexture, ms_Resources->GetTexture(+Texture::AlbedoAndRoughness).GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_EmissiveAndMetallicTexture, ms_Resources->GetTexture(+Texture::EmissiveAndMetallic).GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_WorldNormalTexture, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_VelocityBuffer, ms_Resources->GetTexture(+Texture::VelocityBuffer).GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_ViewDepthBuffer, viewDepth.GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_DepthBuffer, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv({ .Format = g_RenderPassConfig.DepthStencilSrvFormat }));
            commandList.SetRootResource(joint::FullScreenDebugRc_ShadowVisibilityBuffer, noisyShadowVisibilityTexture.GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_TemporalAccumulationBuffer, temporalAccumulationTexture.GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_ReprojectedHistoryTexture, ms_Resources->GetTexture(+Texture::ReprojectedShadowHistory).GetSrv());
            commandList.SetRootResource(joint::FullScreenDebugRc_DenoisedShadowVisibilityBuffer, ms_Resources->GetTexture(+Texture::DenoisedShadowVisibility).GetSrv());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

    private:
        using PassConstantBuffer = benzin::ConstantBuffer<joint::FullScreenDebugConstants>;

        benzin::PipelineState* m_Pso = nullptr;
        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
    };

    class BackBufferCopyPass : public benzin::RenderPass
    {
    public:
        bool IsDependentOnViewport() const override { return false; }

        void OnRender() const override
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[+SandboxTiming::BackBufferCopy]);

            auto& gpuTimer = ms_Device->GetGpuTimer();
            BenzinGrabGpuTimeOnScopeExit(gpuTimer, +SandboxTiming::BackBufferCopy);

            auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
            BenzinPushGpuEvent(commandList, "BackBufferCopy");

            const auto& currentBackBuffer = ms_SwapChain->GetCurrentBackBuffer();
            const auto& imGuiTexture = ms_Resources->GetTexture(+Texture::ImGui);

            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::CopyDestination },
                benzin::TransitionBarrier{ imGuiTexture, benzin::ResourceState::CopySource },
            );

            commandList.CopyResource(currentBackBuffer, imGuiTexture);
        }
    };

    // SandboxRunner

    SandboxRunner::SandboxRunner()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::SandboxRunner");

        InitRenderPasses();
        InitTools();
        
        InitCamera();
        InitSceneEntities();

        m_1SecIntervalTimer.PushCallback([this]
        {
            g_CpuTimings[+SandboxTiming::ImGuiPass] = m_ImGuiPass->GetCpuRenderTime();

            m_TimingsTool->SetRunnerTimings(m_RunnerTimings);
            m_TimingsTool->SetCpuTimings(g_CpuTimings);

            g_CpuTimings = {}; // TODO: Reset it every frame
        });
    }

    void SandboxRunner::InitRenderPasses()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitRenderPasses");

        auto isRenderTextureFlippableCallback = [](uint32_t key)
        {
            const uint32_t maxKey = +magic_enum::enum_values<Texture>().back();
            const uint32_t previousTextureKey = key - 1;

            const bool isInBounds = previousTextureKey <= maxKey;
            const bool isGapExists = !magic_enum::enum_contains<Texture>(previousTextureKey); // There must be a gap between enum values, so the enum value must not exist

            return isInBounds && isGapExists;
        };

        m_RenderResources->SetMaxTextureCount(+Texture::Count);
        m_RenderResources->SetIsTextureFlippableCallback(std::move(isRenderTextureFlippableCallback));

        // The order in which render passes are added is important
        m_RenderPasses.reserve(9);

        m_RenderPasses.push_back(std::make_unique<GlobalConstantBufferPass>(*m_Scene));
        m_RenderPasses.push_back(std::make_unique<GeometryPass>(*m_Scene));
        m_RenderPasses.push_back(std::make_unique<RtShadowPass>(*m_Scene));
        m_RenderPasses.push_back(std::make_unique<DenoiserPass>());
        m_RenderPasses.push_back(std::make_unique<DeferredLightingPass>(*m_Scene));
        m_RenderPasses.push_back(std::make_unique<EnvironmentPass>());
        m_RenderPasses.push_back(std::make_unique<FullScreenDebugPass>());

        m_RenderPasses.push_back(std::make_unique<benzin::ImGuiPass>(*m_ImGuiManager, +Texture::ImGui, +SandboxTiming::ImGuiPass));
        m_ImGuiPass = (benzin::ImGuiPass*)m_RenderPasses.back().get();

        m_RenderPasses.push_back(std::make_unique<BackBufferCopyPass>());
    }

    void SandboxRunner::InitTools()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitTools");

        m_RenderViewportTool->SetFinalTextureIndex(+Texture::Final);

        m_TimingsTool = m_ImGuiManager->PushTool<TimingsTool>(*m_Device);

        BenzinAssert(m_RenderSettingsTool != nullptr);

        m_RenderSettingsTool->RegisterSectionImGuiSpawnCallback<RtShadowsSettings>("RtShadows", [](RtShadowsSettings& settings)
        {
            ImGui::Checkbox("IsRtShadowsEnabled", &settings.IsRtShadowEnabled);
            ImGui::SliderInt("RaysPerPixel", (int*)&settings.RaysPerPixel, 0, 100);
        });

        m_RenderSettingsTool->RegisterSectionImGuiSpawnCallback<DenoiserSettings>("Denoiser", [](DenoiserSettings& settings)
        {
            ImGui::Checkbox("IsDenoiserEnabled", &settings.IsDenoiserEnabled);
            ImGui::DragInt("MaxTemporalAccumulationCount", (int*)&settings.MaxTemporalAccumulationCount, 0.2f, 1, 64);
        });

        m_RenderSettingsTool->RegisterSectionImGuiSpawnCallback<DenoiserMipGenerationSettings>("DenoiserMipGeneration", [](DenoiserMipGenerationSettings& settings)
        {
            static const auto names = magic_enum::enum_names<joint::MipGenerationFilterType>() |
                std::views::transform([](std::string_view name) { return name.substr("MipGenerationFilterType_"sv.size()).data(); }) |
                std::ranges::to<std::vector>();

            ImGui::Combo("MipGenerationFilterType", (int*)&settings.DepthFilterType, names.data(), (int)names.size());
        });

        m_RenderSettingsTool->RegisterSectionImGuiSpawnCallback<DenoiserHistoryFixSettings>("DenoiserHistoryFix", [](DenoiserHistoryFixSettings& settings)
        {
            ImGui::Checkbox("IsHistoryFixEnabled", &settings.IsHistoryFixEnabled);
            ImGui::Checkbox("IsViewDepthUsedForWeights", &settings.IsViewDepthUsedForWeights);
        });

        m_RenderSettingsTool->RegisterSectionImGuiSpawnCallback<DenoiserBlurSettings>("DenoiserBlur", [](DenoiserBlurSettings& settings)
        {
            ImGui::DragFloat("SpecularAccumulationCurve", &settings.SpecularAccumulationCurve, 0.001f, 0.0f, 1.0f);
            ImGui::DragFloat("SpecularAccumulationBasePower", &settings.SpecularAccumulationBasePower, 0.001f, 0.0f, 1.0f);
            ImGui::Checkbox("IsDenoiserAntilagEnabled", &settings.IsDenoiserAntilagEnabled);
            ImGui::Separator();

            ImGui::Checkbox("IsGeometryWeightUsed", &settings.IsGeometryWeightUsed);
            ImGui::Checkbox("IsNormalWeightUsed", &settings.IsNormalWeightUsed);
            ImGui::Checkbox("IsRoughnessWeightUsed", &settings.IsRoughnessWeightUsed);
            ImGui::DragFloat("GeometryWeightSensitivity", &settings.GeometryWeightSensitivity, 0.2f, 1.0f, 50.0f);
            ImGui::DragFloat("MinBlurRadius", &settings.MinBlurRadius, 0.001f, 0.001f, 0.5f);
            ImGui::DragFloat("MaxBlurRadius", &settings.MaxBlurRadius, 0.001f, 0.001f, 0.5f);
        });

        m_RenderSettingsTool->RegisterSectionImGuiSpawnCallback<DeferredLightingSettings>("DeferredLighting", [](DeferredLightingSettings& settings)
        {
            ImGui::DragFloat("SunIntensity", &settings.SunIntensity, 0.1f, 0.0f, 100.0f);
            ImGui::ColorEdit3("SunColor", reinterpret_cast<float*>(&settings.SunColor));

            if (ImGui::DragFloat3("SunDirection", reinterpret_cast<float*>(&settings.SunDirection), 0.01f, -1.0f, 1.0f))
            {
                DirectX::XMStoreFloat3(&settings.SunDirection, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&settings.SunDirection)));
            }
        });

        m_RenderSettingsTool->RegisterSectionImGuiSpawnCallback<FullScreenDebugSettings>("FullScreenDebug", [](FullScreenDebugSettings& settings)
        {
            ImGui::SliderInt("ViewDepthMipIndex", (int*)&settings.ViewDepthMipIndex, 0, 4);
            ImGui::SliderFloat("MinViewDepth", &settings.MinViewDepth, 0.001f, 2.0f, "%.4f");
            ImGui::SliderFloat("MaxViewDepth", &settings.MaxViewDepth, 0.001f, 30.0f);

            static const auto debugOutputTypeNames = magic_enum::enum_names<joint::DebugOutputType>() |
                std::views::transform([](std::string_view name) { return name.substr("DebugOutputType_"sv.size()).data(); }) |
                std::ranges::to<std::vector>();

            ImGui::Combo("DebugOutputType", (int*)&settings.DebugOutputType, debugOutputTypeNames.data(), (int)debugOutputTypeNames.size());
        });
    }

    void SandboxRunner::InitCamera()
    {
        auto& perspectiveProjection = m_Scene->GetPerspectiveProjection();
        perspectiveProjection.SetLens(DirectX::XMConvertToRadians(60.0f), 16.0f / 9.0f, 0.1f, 1000.0f);

        auto& camera = m_Scene->GetCamera();
        camera.SetPosition({ -3.0f, 2.0f, -0.25f });
        camera.SetFrontDirection({ 1.0f, 0.0f, 0.0f });
    }

    void SandboxRunner::InitSceneEntities()
    {
        SceneMeshes sceneMeshes;
        LoadAndCreateMeshes(sceneMeshes);
        CreateEntities(sceneMeshes);
    }

    void SandboxRunner::LoadAndCreateMeshes(SceneMeshes& outSceneMeshes)
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::LoadAndCreateMeshes");

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
        BenzinLogTimeOnScopeExit("SandboxRunner::CreateEntities");

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
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle)
            {
                if (m_AnimationTimer.IsPaused())
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                auto rotation = tc.GetRotation();
                rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                rotation.z += 0.0002f * m_AnimationTimer.GetDeltaTimeInMs();

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
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle)
            {
                if (m_AnimationTimer.IsPaused())
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                auto rotation = tc.GetRotation();
                rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                rotation.y -= 0.00015f * m_AnimationTimer.GetDeltaTimeInMs();

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
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle)
            {
                if (m_AnimationTimer.IsPaused())
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                static constexpr float travelRadius = 1.0f;
                static constexpr float travelSpeed = 0.001f;

                static const float startX = tc.GetTranslation().x;
                static const float startZ = tc.GetTranslation().z;

                auto translation = tc.GetTranslation();
                translation.x = startX + travelRadius * std::cos(travelSpeed * m_AnimationTimer.GetElapsedTimeInMs());
                translation.z = startZ + travelRadius * std::sin(travelSpeed * m_AnimationTimer.GetElapsedTimeInMs());

                tc.SetTranslation(translation);
            };
        }
    }

}
