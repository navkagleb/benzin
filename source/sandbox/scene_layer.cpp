#include "bootstrap.hpp"
#include "scene_layer.hpp"

#include <benzin/core/math.hpp>
#include <benzin/core/logger.hpp>
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
#include <benzin/utility/random.hpp>
#include <benzin/utility/time_utils.hpp>

#include <shaders/joint/constant_buffer_types.hpp>
#include <shaders/joint/root_constants.hpp>
#include <shaders/joint/structured_buffer_types.hpp>

namespace sandbox
{

    enum class CpuTiming : uint32_t
    {
        _BuildTopLevelAs,
        _GeometryPass,
        _RtShadowPass,
        _RtShadowDenoisingPass,
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
        _GeometryPass,
        _RtShadowPass,
        _RtShadowDenoisingPass,
        _DeferredLightingPass,
        _EnvironmentPass,
        _FullScreenDebugPass,
        _BackBufferCopy,
        _Total,
    };

    static void ImGuiDisplayTexture(const benzin::Texture& texture)
    {
        BenzinAssert(texture.HasShaderResourceView());

        const float widgetWidth = ImGui::GetContentRegionAvail().x;

        const uint64_t gpuHandle = texture.GetShaderResourceView().GetGpuHandle();
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

    struct GBufferConfig
    {
        const benzin::GraphicsFormat Color0Format = benzin::GraphicsFormat::Rgba8Unorm; // Albedo, Albedo, Albedo, Roughness
        const benzin::GraphicsFormat Color1Format = benzin::GraphicsFormat::Rgba8Unorm; // Emissive, Emissive, Emissive, Metallic
        const benzin::GraphicsFormat Color2Format = benzin::GraphicsFormat::Rgba16Float; // WorldNormal, WorldNormal, WorldNormal, None
        const benzin::GraphicsFormat Color3Format = benzin::GraphicsFormat::Rgba16Float; // UvMotionVector, UvMotionVector, DepthMotionVector, None
        const benzin::GraphicsFormat Color4Format = benzin::GraphicsFormat::R32Float; // ViewDepth
        const benzin::GraphicsFormat DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint;
    };

    struct RtShadowConfig
    {
        const std::wstring_view RayGenShaderName = L"RayGen";
        const std::wstring_view MissShaderName = L"Miss";
        const std::wstring_view HitGroupName = L"HitGroup";
    };

    struct RtShadowParams
    {
        const uint32_t TextureSlotCount = 2;

        uint32_t PreviousTextureSlot = 0;
        uint32_t CurrentTextureSlot = 1;

        uint32_t RaysPerPixel = 1;
        uint32_t MaxTemporalAccumulationCount = 32;
    };

    struct DeferredLightingParams
    {
        float SunIntensity = 0.0f;
        DirectX::XMFLOAT3 SunColor{ 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 SunDirection{ -0.5f, -0.5f, -0.5f };
    };

    struct FullScreenDebugParams
    {
        joint::DebugOutputType OutputType = joint::DebugOutputType_None;
    };

    static constexpr GBufferConfig g_GBufferConfig;
    static constexpr RtShadowConfig g_RtShadowConfig;

    static RtShadowParams g_RtShadowParams;
    static DeferredLightingParams g_DeferredLightingParams;
    static FullScreenDebugParams g_FullScreenDebugParams;

    static magic_enum::containers::array<CpuTiming, std::chrono::microseconds> g_CpuTimings;

    // GeometryPass

    GeometryPass::GeometryPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
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
                g_GBufferConfig.Color0Format,
                g_GBufferConfig.Color1Format,
                g_GBufferConfig.Color2Format,
                g_GBufferConfig.Color3Format,
                g_GBufferConfig.Color4Format,
            },
            .DepthStencilFormat = g_GBufferConfig.DepthStencilFormat,
        });

        OnResize(m_SwapChain.GetViewportWidth(), m_SwapChain.GetViewportHeight());
    }

    void GeometryPass::OnUpdate()
    {
        m_GBuffer.ViewDepths.GoToNext();
    }

    void GeometryPass::OnRender(const benzin::Scene& scene) const
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        auto& currentViewDepth = m_GBuffer.ViewDepths.GetCurrent();

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.AlbedoAndRoughness, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.EmissiveAndMetallic, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.WorldNormal, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.VelocityBuffer, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *currentViewDepth, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.DepthStencil, benzin::ResourceState::DepthWrite });
        BenzinExecuteOnScopeExit([&]
        {
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.AlbedoAndRoughness, benzin::ResourceState::Common });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.EmissiveAndMetallic, benzin::ResourceState::Common });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.WorldNormal, benzin::ResourceState::Common });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.VelocityBuffer, benzin::ResourceState::Common });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *currentViewDepth, benzin::ResourceState::Common });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.DepthStencil, benzin::ResourceState::Common });
        });

        commandList.SetRenderTargets(
            {
                m_GBuffer.AlbedoAndRoughness->GetRenderTargetView(),
                m_GBuffer.EmissiveAndMetallic->GetRenderTargetView(),
                m_GBuffer.WorldNormal->GetRenderTargetView(),
                m_GBuffer.VelocityBuffer->GetRenderTargetView(),
                currentViewDepth->GetRenderTargetView(),
            },
            &m_GBuffer.DepthStencil->GetDepthStencilView()
        );

        commandList.ClearRenderTarget(m_GBuffer.AlbedoAndRoughness->GetRenderTargetView());
        commandList.ClearRenderTarget(m_GBuffer.EmissiveAndMetallic->GetRenderTargetView());
        commandList.ClearRenderTarget(m_GBuffer.WorldNormal->GetRenderTargetView());
        commandList.ClearRenderTarget(m_GBuffer.VelocityBuffer->GetRenderTargetView());
        commandList.ClearRenderTarget(currentViewDepth->GetRenderTargetView());
        commandList.ClearDepthStencil(m_GBuffer.DepthStencil->GetDepthStencilView());

        commandList.SetPipelineState(*m_Pso);

        const auto view = scene.GetEntityRegistry().view<benzin::TransformComponent, benzin::MeshInstanceComponent>();
        for (const auto entityHandle : view)
        {
            const auto& tc = view.get<benzin::TransformComponent>(entityHandle);
            const auto& mic = view.get<benzin::MeshInstanceComponent>(entityHandle);

            const auto& meshCollection = scene.GetMeshCollection(mic.MeshUnionIndex);
            const auto& meshCollectionGpuStorage = scene.GetMeshCollectionGpuStorage(mic.MeshUnionIndex);

            commandList.SetRootResource(joint::GeometryPassRc_MeshVertexBuffer, meshCollectionGpuStorage.VertexBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRc_MeshIndexBuffer, meshCollectionGpuStorage.IndexBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRc_MeshInfoBuffer, meshCollectionGpuStorage.MeshInfoBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRc_MeshInstanceBuffer, meshCollectionGpuStorage.MeshInstanceBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRc_MaterialBuffer, meshCollectionGpuStorage.MaterialBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRc_MeshTransformConstantBuffer, tc.GetActiveTransformCbv());

            const auto meshInstanceRange = mic.MeshInstanceRange.value_or(meshCollection.GetFullMeshInstanceRange());
            for (const auto i : benzin::IndexRangeToView(meshInstanceRange))
            {
                if (IsMeshCulled(scene.GetCamera(), meshCollection, i, tc.GetWorldMatrix()))
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

    void GeometryPass::OnResize(uint32_t width, uint32_t height)
    {
        const auto CreateGBufferTexture = [this](
            std::unique_ptr<benzin::Texture>& outGBufferTexture,
            benzin::GraphicsFormat format,
            uint32_t width,
            uint32_t height,
            benzin::TextureFlag flag,
            std::string_view debugName
        )
        {
            benzin::MakeUniquePtr(outGBufferTexture, m_Device, benzin::TextureCreation
            {
                .DebugName = debugName,
                .Type = benzin::TextureType::Texture2D,
                .Format = format,
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = flag,
            });

            switch (flag)
            {
                case benzin::TextureFlag::AllowRenderTarget:
                {
                    outGBufferTexture->PushShaderResourceView();
                    outGBufferTexture->PushRenderTargetView();

                    break;
                }
                case benzin::TextureFlag::AllowDepthStencil:
                {
                    outGBufferTexture->PushShaderResourceView({ .Format{ benzin::GraphicsFormat::D24Unorm_X8Typeless } });
                    outGBufferTexture->PushDepthStencilView();

                    break;
                }
            }
        };

        CreateGBufferTexture(m_GBuffer.AlbedoAndRoughness, g_GBufferConfig.Color0Format, width, height, benzin::TextureFlag::AllowRenderTarget, "GBuffer_AlbedoAndRoughness");
        CreateGBufferTexture(m_GBuffer.EmissiveAndMetallic, g_GBufferConfig.Color1Format, width, height, benzin::TextureFlag::AllowRenderTarget, "GBuffer_EmissiveAndMetallic");
        CreateGBufferTexture(m_GBuffer.WorldNormal, g_GBufferConfig.Color2Format, width, height, benzin::TextureFlag::AllowRenderTarget, "GBuffer_WorldNormal");
        CreateGBufferTexture(m_GBuffer.VelocityBuffer, g_GBufferConfig.Color3Format, width, height, benzin::TextureFlag::AllowRenderTarget, "GBuffer_VelocityBuffer");
        CreateGBufferTexture(m_GBuffer.DepthStencil, g_GBufferConfig.DepthStencilFormat, width, height, benzin::TextureFlag::AllowDepthStencil, "GBuffer_DepthStencil");

        m_GBuffer.ViewDepths.InitializeAll([&](uint32_t i, std::unique_ptr<benzin::Texture>& viewDepth)
        {
            CreateGBufferTexture(viewDepth, g_GBufferConfig.Color4Format, width, height, benzin::TextureFlag::AllowRenderTarget, std::format("GBuffer_ViewDepth{}", i));
        });
    }

    // RtShadowPass

    RtShadowPass::RtShadowPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
    {
        CreatePipelineStateObject();
        CreateShaderTable();

        benzin::MakeUniquePtr(m_PassConstantBuffer, m_Device, "RtShadowPassConstantBuffer");

        OnResize((uint32_t)swapChain.GetViewport().Width, (uint32_t)swapChain.GetViewport().Height);
    }

    void RtShadowPass::OnUpdate(std::chrono::microseconds dt, std::chrono::milliseconds elapsedTime)
    {
        g_RtShadowParams.PreviousTextureSlot = g_RtShadowParams.CurrentTextureSlot;
        g_RtShadowParams.CurrentTextureSlot = (g_RtShadowParams.CurrentTextureSlot + 1) % g_RtShadowParams.TextureSlotCount;

        m_PassConstantBuffer->UpdateConstants(joint::RtShadowPassConstants
        {
            .CurrentTextureSlot = g_RtShadowParams.CurrentTextureSlot,
            .RaysPerPixel = g_RtShadowParams.RaysPerPixel,
        });
    }

    void RtShadowPass::OnRender(const benzin::Scene& scene, const GeometryPass::GBuffer& gbuffer) const
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();
        auto* d3d12CommandList = commandList.GetD3D12GraphicsCommandList();

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_VisibilityBuffer, benzin::ResourceState::UnorderedAccess });
        BenzinExecuteOnScopeExit([&]
        {
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_VisibilityBuffer, benzin::ResourceState::Common });
        });

        d3d12CommandList->SetPipelineState1(m_D3D12RaytracingStateObject.Get());

        const auto& activeTopLevelAS = scene.GetActiveTopLevelAS();

        commandList.SetRootResource(joint::RtShadowPassRc_PassConstantBuffer, m_PassConstantBuffer->GetActiveCbv());
        commandList.SetRootResource(joint::RtShadowPassRc_GBufferWorldNormalTexture, gbuffer.WorldNormal->GetShaderResourceView());
        commandList.SetRootResource(joint::RtShadowPassRc_GBufferDepthTexture, gbuffer.DepthStencil->GetShaderResourceView());
        commandList.SetRootResource(joint::RtShadowPassRc_PointLightBuffer, scene.GetPointLightBufferSRV());
        commandList.SetRootResource(joint::RtShadowPassRc_VisiblityBuffer, m_VisibilityBuffer->GetUnorderedAccessView());

        d3d12CommandList->SetComputeRootShaderResourceView(1, activeTopLevelAS.GetBuffer().GetGpuVirtualAddress());

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
            .Width = m_VisibilityBuffer->GetWidth(),
            .Height = m_VisibilityBuffer->GetHeight(),
            .Depth = 1,
        };

        d3d12CommandList->DispatchRays(&d3d12DispatchRayDesc);
    }

    void RtShadowPass::OnResize(uint32_t width, uint32_t height)
    {
        benzin::MakeUniquePtr(m_VisibilityBuffer, m_Device, benzin::TextureCreation
        {
            .DebugName = "RtShadowPass_VisibilityBuffer",
            .Type = benzin::TextureType::Texture2D,
            .Format = benzin::GraphicsFormat::Rgba16Float,
            .Width = width,
            .Height = height,
            .MipCount = 1,
            .Flags = benzin::TextureFlag::AllowUnorderedAccess,
            .IsNeedShaderResourceView = true, // For debugging
            .IsNeedUnorderedAccessView = true,
        });
    }

    void RtShadowPass::CreatePipelineStateObject()
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
            .HitGroupExport = g_RtShadowConfig.HitGroupName.data(),
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

        BenzinAssert(m_Device.GetD3D12Device()->CreateStateObject(&d3d12StateObjectDesc, IID_PPV_ARGS(&m_D3D12RaytracingStateObject)));
    }

    void RtShadowPass::CreateShaderTable()
    {
        BenzinAssert(m_D3D12RaytracingStateObject.Get());

        ComPtr<ID3D12StateObjectProperties> d3d12StateObjectProperties;
        BenzinAssert(m_D3D12RaytracingStateObject.As(&d3d12StateObjectProperties));

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

        m_RayGenShaderTable = CreateShaderTable(g_RtShadowConfig.RayGenShaderName);
        m_MissShaderTable = CreateShaderTable(g_RtShadowConfig.MissShaderName);
        m_HitGroupShaderTable = CreateShaderTable(g_RtShadowConfig.HitGroupName);
    }

    // RtShadowDenoisingPass

    RtShadowDenoisingPass::RtShadowDenoisingPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
    {
        benzin::MakeUniquePtr(m_Pso, m_Device, benzin::ComputePipelineStateCreation
        {
            .DebugName = "RTShadowDenoisingPass",
            .ComputeShader{ "rt_shadow_denoising_pass.hlsl", "CS_Main" },
        });

        benzin::MakeUniquePtr(m_PassConstantBuffer, m_Device, "RtShadowDenoisingPassConstantBuffer");

        OnResize((uint32_t)swapChain.GetViewportWidth(), (uint32_t)swapChain.GetViewportHeight());
    }

    void RtShadowDenoisingPass::OnUpdate()
    {
        m_PassConstantBuffer->UpdateConstants(joint::RtShadowDenoisingPassConstants
        {
            .CurrentTextureSlot = g_RtShadowParams.CurrentTextureSlot,
            .PreviousTextureSlot = g_RtShadowParams.PreviousTextureSlot,
        });

        m_TemporalAccumulationBuffers.GoToNext();
    }

    void RtShadowDenoisingPass::OnRender(benzin::Texture& shadowVisiblityBuffer, const GeometryPass::GBuffer& gbuffer) const
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        auto& previousTemporalAccumulationBuffer = *m_TemporalAccumulationBuffers.GetPrevious();
        auto& currentTemporalAccumulationBuffer = *m_TemporalAccumulationBuffers.GetCurrent();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ shadowVisiblityBuffer, benzin::ResourceState::UnorderedAccess });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentTemporalAccumulationBuffer, benzin::ResourceState::UnorderedAccess });
        BenzinExecuteOnScopeExit([&]
        {
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ shadowVisiblityBuffer, benzin::ResourceState::Common });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentTemporalAccumulationBuffer, benzin::ResourceState::Common });
        });

        commandList.SetPipelineState(*m_Pso);

        commandList.SetRootResource(joint::RtShadowDenoisingRc_PassConstantBuffer, m_PassConstantBuffer->GetActiveCbv());
        commandList.SetRootResource(joint::RtShadowDenoisingRc_WorldNormalTexture, gbuffer.WorldNormal->GetShaderResourceView());
        commandList.SetRootResource(joint::RtShadowDenoisingRc_VelocityBuffer, gbuffer.VelocityBuffer->GetShaderResourceView());
        commandList.SetRootResource(joint::RtShadowDenoisingRc_DepthBuffer, gbuffer.DepthStencil->GetShaderResourceView());
        commandList.SetRootResource(joint::RtShadowDenoisingRc_PreviousViewDepthBuffer, gbuffer.ViewDepths.GetPrevious()->GetShaderResourceView());
        commandList.SetRootResource(joint::RtShadowDenoisingRc_ShadowVisibilityBuffer, shadowVisiblityBuffer.GetUnorderedAccessView());
        commandList.SetRootResource(joint::RtShadowDenoisingRc_PreviousTemporalAccumulationBuffer, previousTemporalAccumulationBuffer.GetUnorderedAccessView());
        commandList.SetRootResource(joint::RtShadowDenoisingRc_CurrentTemporalAccumulationBuffer, currentTemporalAccumulationBuffer.GetUnorderedAccessView());

        const DirectX::XMUINT3 dimensions{ m_SwapChain.GetViewportWidth(), m_SwapChain.GetViewportHeight(), 1 };
        const DirectX::XMUINT3 threadPerGroupCount{ 8, 8, 1 };
        commandList.Dispatch(dimensions, threadPerGroupCount);
    }

    void RtShadowDenoisingPass::OnResize(uint32_t width, uint32_t height)
    {
        m_TemporalAccumulationBuffers.InitializeAll([&](uint32_t i, std::unique_ptr<benzin::Texture>& temporalAccumulationBuffer)
        {
            benzin::MakeUniquePtr(temporalAccumulationBuffer, m_Device, benzin::TextureCreation
            {
                .DebugName = std::format("RtShadowDenoisingPass_TemporalAccumulationBuffer{}", i),
                .Type = benzin::TextureType::Texture2D,
                .Format = benzin::GraphicsFormat::R32Float,
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowUnorderedAccess,
                .IsNeedShaderResourceView = true,
                .IsNeedUnorderedAccessView = true,
            });
        });
    }

    // DeferredLightingPass

    DeferredLightingPass::DeferredLightingPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
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

    void DeferredLightingPass::OnUpdate(const benzin::Scene& scene)
    {
        m_PassConstantBuffer->UpdateConstants(joint::DeferredLightingPassConstants
        {
            .SunColor = g_DeferredLightingParams.SunColor,
            .SunIntensity = g_DeferredLightingParams.SunIntensity,
            .SunDirection = g_DeferredLightingParams.SunDirection,
            .ActivePointLightCount = scene.GetStats().PointLightCount,
        });
    }

    void DeferredLightingPass::OnRender(const benzin::Scene& scene, const GeometryPass::GBuffer& gbuffer, benzin::Texture& shadowVisiblityBuffer) const
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_OutputTexture, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ shadowVisiblityBuffer, benzin::ResourceState::PixelShaderResource });
        BenzinExecuteOnScopeExit([&]
        {
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_OutputTexture, benzin::ResourceState::Common });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ shadowVisiblityBuffer, benzin::ResourceState::Common });
        });

        commandList.SetRenderTargets({ m_OutputTexture->GetRenderTargetView() });
        commandList.ClearRenderTarget(m_OutputTexture->GetRenderTargetView());

        commandList.SetPipelineState(*m_Pso);

        commandList.SetRootResource(joint::DeferredLightingPassRc_PassConstantBuffer, m_PassConstantBuffer->GetActiveCbv());
        commandList.SetRootResource(joint::DeferredLightingPassRc_AlbedoAndRoughnessTexture, gbuffer.AlbedoAndRoughness->GetShaderResourceView());
        commandList.SetRootResource(joint::DeferredLightingPassRc_EmissiveAndMetallicTexture, gbuffer.EmissiveAndMetallic->GetShaderResourceView());
        commandList.SetRootResource(joint::DeferredLightingPassRc_WorldNormalTexture, gbuffer.WorldNormal->GetShaderResourceView());
        commandList.SetRootResource(joint::DeferredLightingPassRc_VelocityBuffer, gbuffer.VelocityBuffer->GetShaderResourceView());
        commandList.SetRootResource(joint::DeferredLightingPassRc_DepthStencilTexture, gbuffer.DepthStencil->GetShaderResourceView());
        commandList.SetRootResource(joint::DeferredLightingPassRc_PointLightBuffer, scene.GetPointLightBufferSRV());
        commandList.SetRootResource(joint::DeferredLightingPassRc_ShadowVisibilityBuffer, shadowVisiblityBuffer.GetShaderResourceView());

        commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
        commandList.DrawVertexed(3);
    }

    void DeferredLightingPass::OnResize(uint32_t width, uint32_t height)
    {
        benzin::MakeUniquePtr(m_OutputTexture, m_Device, benzin::TextureCreation
        {
            .DebugName = "DeferredLightingPass_OutputTexture",
            .Type = benzin::TextureType::Texture2D,
            .Format = benzin::CommandLineArgs::GetBackBufferFormat(),
            .Width = width,
            .Height = height,
            .MipCount = 1,
            .Flags = benzin::TextureFlag::AllowRenderTarget,
            .ClearValue = benzin::g_DefaultClearColor,
            .IsNeedRenderTargetView = true,
        });
    }

    // EnvironmentPass

    EnvironmentPass::EnvironmentPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
    {
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

        {
            // Load environment texture

            benzin::TextureImage equirectangularTextureImage;
            BenzinAssert(benzin::LoadTextureImageFromHdrFile("scythian_tombs_2_4k.hdr", equirectangularTextureImage));

            const auto equirectangularTexture = std::make_unique<benzin::Texture>(m_Device, benzin::TextureCreation
            {
                .DebugName = equirectangularTextureImage.DebugName,
                .Type = benzin::TextureType::Texture2D,
                .Format = equirectangularTextureImage.Format,
                .Width = equirectangularTextureImage.Width,
                .Height = equirectangularTextureImage.Height,
                .MipCount = 1,
                .IsNeedShaderResourceView = true,
            });

            {
                auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
                BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

                auto& commandList = copyCommandQueue.GetCommandList(equirectangularTexture->GetSizeInBytes());
                commandList.UpdateTextureTopMip(*equirectangularTexture, std::as_bytes(std::span{ equirectangularTextureImage.ImageData }));
            }

            // Convert equirectangular to cube map texture

            const auto equirectangularToCubePipelineState = std::make_unique<benzin::PipelineState>(m_Device, benzin::ComputePipelineStateCreation
            {
                .DebugName = "EquirectangularToCube",
                .ComputeShader{ "equirectangular_to_cube.hlsl", "CS_Main" },
            });

            const uint32_t cubeMapSize = 1024;
            benzin::MakeUniquePtr(m_CubeTexture, m_Device, benzin::TextureCreation
            {
                .DebugName = equirectangularTextureImage.DebugName,
                .Type = benzin::TextureType::Texture2D,
                .IsCubeMap = true,
                .Format = benzin::GraphicsFormat::Rgba32Float,
                .Width = cubeMapSize,
                .Height = cubeMapSize,
                .ArraySize = 6,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowUnorderedAccess,
                .IsNeedShaderResourceView = true,
                .IsNeedUnorderedAccessView = true,
            });

            {
                auto& computeCommandQueue = m_Device.GetComputeCommandQueue();
                BenzinFlushCommandQueueOnScopeExit(computeCommandQueue);

                auto& commandList = computeCommandQueue.GetCommandList();

                commandList.SetPipelineState(*equirectangularToCubePipelineState);

                commandList.SetRootResource(joint::EquirectangularToCubePassRc_EquirectangularTexture, equirectangularTexture->GetShaderResourceView());
                commandList.SetRootResource(joint::EquirectangularToCubePassRc_OutCubeTexture, m_CubeTexture->GetUnorderedAccessView());

                commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_CubeTexture, benzin::ResourceState::UnorderedAccess });

                commandList.Dispatch({ cubeMapSize, cubeMapSize, m_CubeTexture->GetArraySize() }, { 8, 8, 1 });

                commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_CubeTexture, benzin::ResourceState::Common });
            }
        }
    }

    void EnvironmentPass::OnRender(const benzin::Scene& scene, benzin::Texture& deferredLightingOutputTexture, benzin::Texture& gbufferDepthStecil) const
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ deferredLightingOutputTexture, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ gbufferDepthStecil, benzin::ResourceState::DepthRead });
        BenzinExecuteOnScopeExit([&]
        {
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ deferredLightingOutputTexture, benzin::ResourceState::Common });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ gbufferDepthStecil, benzin::ResourceState::Common });
        });

        commandList.SetRenderTargets({ deferredLightingOutputTexture.GetRenderTargetView() }, &gbufferDepthStecil.GetDepthStencilView());

        commandList.SetPipelineState(*m_Pso);

        commandList.SetRootResource(joint::EnvironmentPassRc_CubeMapTexture, m_CubeTexture->GetShaderResourceView());

        commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
        commandList.DrawVertexed(3);
    }

    // FullScreenDebugPass

    FullScreenDebugPass::FullScreenDebugPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
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

    void FullScreenDebugPass::OnUpdate()
    {
        m_PassConstantBuffer->UpdateConstants(joint::FullScreenDebugConstants
        {
            .OutputType = magic_enum::enum_integer(g_FullScreenDebugParams.OutputType),
            .CurrentShadowVisibilityBufferSlot = g_RtShadowParams.CurrentTextureSlot,
            .PreviousShadowVisibilityBufferSlot = g_RtShadowParams.PreviousTextureSlot,
        });
    }

    void FullScreenDebugPass::OnRender(benzin::Texture& finalOutput, const GeometryPass::GBuffer& gbuffer, benzin::Texture& shadowVisiblityBuffer, benzin::Texture& temporalAccumulationBuffer) const
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutput, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ shadowVisiblityBuffer, benzin::ResourceState::PixelShaderResource });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ temporalAccumulationBuffer, benzin::ResourceState::PixelShaderResource });
        BenzinExecuteOnScopeExit([&]
        {
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutput, benzin::ResourceState::Common });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ shadowVisiblityBuffer, benzin::ResourceState::Common });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ temporalAccumulationBuffer, benzin::ResourceState::Common });
        });

        commandList.SetRenderTargets({ finalOutput.GetRenderTargetView() });
        commandList.ClearRenderTarget(finalOutput.GetRenderTargetView());

        commandList.SetPipelineState(*m_Pso);

        commandList.SetRootResource(joint::FullScreenDebugRc_PassConstantBuffer, m_PassConstantBuffer->GetActiveCbv());
        commandList.SetRootResource(joint::FullScreenDebugRc_AlbedoAndRoughnessTexture, gbuffer.AlbedoAndRoughness->GetShaderResourceView());
        commandList.SetRootResource(joint::FullScreenDebugRc_EmissiveAndMetallicTexture, gbuffer.EmissiveAndMetallic->GetShaderResourceView());
        commandList.SetRootResource(joint::FullScreenDebugRc_WorldNormalTexture, gbuffer.WorldNormal->GetShaderResourceView());
        commandList.SetRootResource(joint::FullScreenDebugRc_VelocityBuffer, gbuffer.VelocityBuffer->GetShaderResourceView());
        commandList.SetRootResource(joint::FullScreenDebugRc_DepthBuffer, gbuffer.DepthStencil->GetShaderResourceView());
        commandList.SetRootResource(joint::FullScreenDebugRc_ShadowVisiblityBuffer, shadowVisiblityBuffer.GetShaderResourceView());
        commandList.SetRootResource(joint::FullScreenDebugRc_TemporalAccumulationBuffer, temporalAccumulationBuffer.GetShaderResourceView());

        commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
        commandList.DrawVertexed(3);
    }

    // SceneLayer

    SceneLayer::SceneLayer(const benzin::GraphicsRefs& graphicsRefs)
        : m_Window{ graphicsRefs.WindowRef }
        , m_Device{ graphicsRefs.DeviceRef }
        , m_SwapChain{ graphicsRefs.SwapChainRef }
        , m_GeometryPass{ m_Device, m_SwapChain }
        , m_RtShadowPass{ m_Device, m_SwapChain }
        , m_RtShadowDenoisingPass{ m_Device, m_SwapChain }
        , m_DeferredLightingPass{ m_Device, m_SwapChain }
        , m_EnvironmentPass{ m_Device, m_SwapChain }
        , m_FullScreenDebugPass{ m_Device, m_SwapChain }
    {
        benzin::MakeUniquePtr(m_GpuTimer, m_Device, benzin::GpuTimerCreation
        {
            .CommandList = m_Device.GetGraphicsCommandQueue().GetCommandList(),
            .TimestampFrequency = m_Device.GetGraphicsCommandQueue().GetTimestampFrequency(),
            .TimerCount = (uint32_t)magic_enum::enum_count<GpuTiming>(),
        });

        benzin::MakeUniquePtr(m_FrameConstantBuffer, m_Device, "FrameConstantBuffer");

        {
            auto& perspectiveProjection = m_Scene.GetPerspectiveProjection();
            perspectiveProjection.SetLens(DirectX::XMConvertToRadians(60.0f), m_SwapChain.GetAspectRatio(), 0.1f, 1000.0f);

            auto& camera = m_Scene.GetCamera();
            camera.SetPosition({ -3.0f, 2.0f, -0.25f });
            camera.SetFrontDirection({ 1.0f, 0.0f, 0.0f });
        }

        CreateEntities();

        {
            BenzinLogTimeOnScopeExit("Upload scene data to GPU");
            m_Scene.UploadMeshCollections();
        }

        {
            BenzinLogTimeOnScopeExit("Build scene RT BottomLevel ASs");
            m_Scene.BuildBottomLevelAccelerationStructures();
        }
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
            .InvRenderResolution{ 1.0f / m_SwapChain.GetViewportWidth(), 1.0f / m_SwapChain.GetViewportHeight(), },
            .CpuFrameIndex = (uint32_t)m_Device.GetCpuFrameIndex(),
            .DeltaTime = benzin::ToFloatMs(dt),
            .MaxTemporalAccumulationCount = g_RtShadowParams.MaxTemporalAccumulationCount,
        });

        m_FlyCameraController.OnUpdate(dt);
        m_Scene.OnUpdate(dt);

        {
            // Note: Before updating TopLevelAS the TransformComponents must be updated

            BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_BuildTopLevelAs]);
            BenzinGrabGpuTimeOnScopeExit(*m_GpuTimer, magic_enum::enum_integer(GpuTiming::_BuildTopLevelAs));

            m_Scene.BuildTopLevelAccelerationStructure();
        }

        m_GeometryPass.OnUpdate();
        m_RtShadowPass.OnUpdate(dt, elapsedTime);
        m_RtShadowDenoisingPass.OnUpdate();
        m_DeferredLightingPass.OnUpdate(m_Scene);
        m_FullScreenDebugPass.OnUpdate();
    }

    void SceneLayer::OnRender()
    {
        BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_SceneLayerOnRender]);

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        BenzinExecuteOnScopeExit([this] { m_GpuTimer->ResolveTimestamps(m_Device.GetCpuFrameIndex()); });
        BenzinGrabGpuTimeOnScopeExit(*m_GpuTimer, magic_enum::enum_integer(GpuTiming::_Total));

        commandList.SetRootResource(joint::GlobalRc_FrameConstantBuffer, m_FrameConstantBuffer->GetActiveCbv());
        commandList.SetRootResource(joint::GlobalRc_CameraConstantBuffer, m_Scene.GetCameraConstantBufferActiveCbv());

        auto& gbuffer = m_GeometryPass.GetGBuffer();
        auto& shadowVisibilityBuffer = m_RtShadowPass.GetVisibilityBuffer();
        auto& finalOutput = m_DeferredLightingPass.GetOutputTexture();
        auto& temporalAccumulationBuffer = *m_RtShadowDenoisingPass.GetTemporalAccumulationBuffers().GetCurrent();

        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_GeometryPass]);
            BenzinGrabGpuTimeOnScopeExit(*m_GpuTimer, magic_enum::enum_integer(GpuTiming::_GeometryPass));

            m_GeometryPass.OnRender(m_Scene);
        }

        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_RtShadowPass]);
            BenzinGrabGpuTimeOnScopeExit(*m_GpuTimer, magic_enum::enum_integer(GpuTiming::_RtShadowPass));

            m_RtShadowPass.OnRender(m_Scene, gbuffer);
        }

        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_RtShadowDenoisingPass]);
            BenzinGrabGpuTimeOnScopeExit(*m_GpuTimer, magic_enum::enum_integer(GpuTiming::_RtShadowDenoisingPass));

            m_RtShadowDenoisingPass.OnRender(shadowVisibilityBuffer, gbuffer);
        }

        if (g_FullScreenDebugParams.OutputType != joint::DebugOutputType_None)
        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_FullScreenDebugPass]);
            BenzinGrabGpuTimeOnScopeExit(*m_GpuTimer, magic_enum::enum_integer(GpuTiming::_FullScreenDebugPass));

            m_FullScreenDebugPass.OnRender(finalOutput, gbuffer, shadowVisibilityBuffer, temporalAccumulationBuffer);
        }
        else
        {
            {
                BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_DeferredLightingPass]);
                BenzinGrabGpuTimeOnScopeExit(*m_GpuTimer, magic_enum::enum_integer(GpuTiming::_DeferredLightingPass));

                m_DeferredLightingPass.OnRender(m_Scene, gbuffer, shadowVisibilityBuffer);
            }

            {
                BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_EnvironmentPass]);
                BenzinGrabGpuTimeOnScopeExit(*m_GpuTimer, magic_enum::enum_integer(GpuTiming::_EnvironmentPass));

                m_EnvironmentPass.OnRender(m_Scene, finalOutput, *gbuffer.DepthStencil);
            }
        }

        {
            BenzinGrabTimeOnScopeExit(g_CpuTimings[CpuTiming::_BackBufferCopy]);
            BenzinGrabGpuTimeOnScopeExit(*m_GpuTimer, magic_enum::enum_integer(GpuTiming::_BackBufferCopy));

            auto& currentBackBuffer = m_SwapChain.GetCurrentBackBuffer();

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::CopyDestination });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutput, benzin::ResourceState::CopySource });
            BenzinExecuteOnScopeExit([&]
            {
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::Common });
                commandList.SetResourceBarrier(benzin::TransitionBarrier{ finalOutput, benzin::ResourceState::Common });
            });

            commandList.CopyResource(currentBackBuffer, finalOutput);
        }
    }

    void SceneLayer::OnResize(uint32_t width, uint32_t height)
    {
        m_GeometryPass.OnResize(width, height);
        m_RtShadowPass.OnResize(width, height);
        m_RtShadowDenoisingPass.OnResize(width, height);
        m_DeferredLightingPass.OnResize(width, height);
    }

    void SceneLayer::OnImGuiRender()
    {
        m_FlyCameraController.OnImGuiRender();

        ImGui::Begin("Misc");
        {
            const ImVec4 titleColor{ 0.72f, 39.0f, 0.0f, 1.0f };

            {
                ImGui::TextColored(titleColor, "RtShadowParams");

                ImGui::SliderInt("RtShadows RaysPerPixel", (int*)&g_RtShadowParams.RaysPerPixel, 0, 100);
                ImGui::DragInt("MaxTemporalAccumulationCount", (int*)&g_RtShadowParams.MaxTemporalAccumulationCount, 1.0f, 1, 64);

                ImGui::Separator();
                ImGui::NewLine();
            }

            {
                ImGui::TextColored(titleColor, "DeferredLightingParams");

                ImGui::DragFloat("SunIntensity", &g_DeferredLightingParams.SunIntensity, 0.1f, 0.0f, 100.0f);
                ImGui::ColorEdit3("SunColor", reinterpret_cast<float*>(&g_DeferredLightingParams.SunColor));

                if (ImGui::DragFloat3("SunDirection", reinterpret_cast<float*>(&g_DeferredLightingParams.SunDirection), 0.01f, -1.0f, 1.0f))
                {
                    DirectX::XMStoreFloat3(&g_DeferredLightingParams.SunDirection, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&g_DeferredLightingParams.SunDirection)));
                }

                ImGui::Separator();
                ImGui::NewLine();
            }

            {
                ImGui::TextColored(titleColor, "FullScreenDebugParams");

                ImGui::Text("DebugOutputType");
                if (ImGui::BeginListBox("##emtpy", ImVec2{ -FLT_MIN, 200.0f })) // #TODO: Calculate item height
                {
                    for (const auto i : std::views::iota(0u, joint::DebugOutputType_Count))
                    {
                        const bool isSelected = g_FullScreenDebugParams.OutputType == i;

                        const auto name = magic_enum::enum_name((joint::DebugOutputType)i).substr("DebugOutputType_"sv.size());
                        if (ImGui::Selectable(name.data(), isSelected))
                        {
                            g_FullScreenDebugParams.OutputType = (joint::DebugOutputType)i;
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
                    timing = m_GpuTimer->GetElapsedTime((uint32_t)i);
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

    void SceneLayer::CreateEntities()
    {
        auto& entityRegistry = m_Scene.GetEntityRegistry();

        benzin::MeshCollectionResource sponzaMeshCollection;
#if !SANDBOX_IS_PIX_WORKAROUND_ENABLED
        const auto sponzaFuture = std::async(std::launch::async, [&]
        {
            const std::string_view fileName = "Sponza/glTF/Sponza.gltf";

            BenzinLogTimeOnScopeExit("Loading MeshCollection from {}", fileName);
            BenzinAssert(LoadMeshCollectionFromGltfFile(fileName, sponzaMeshCollection));
        });
#else
        {
            const std::string_view fileName = "Sponza/glTF/Sponza.gltf";

            BenzinLogTimeOnScopeExit("Loading MeshCollection from {}", fileName);
            BenzinAssert(LoadMeshCollectionFromGltfFile(fileName, sponzaMeshCollection));
        }
#endif

        benzin::MeshCollectionResource boomBooxMeshCollection;
        {
            const std::string_view fileName = "BoomBox/glTF/BoomBox.gltf";

            BenzinLogTimeOnScopeExit("Loading MeshCollection from {}", fileName);
            BenzinAssert(LoadMeshCollectionFromGltfFile(fileName, boomBooxMeshCollection));
        }

        benzin::MeshCollectionResource damagedHelmetMeshCollection;
        {
            const std::string_view fileName = "DamagedHelmet/glTF/DamagedHelmet.gltf";

            BenzinLogTimeOnScopeExit("Loading MeshCollection from {}", fileName);
            BenzinAssert(LoadMeshCollectionFromGltfFile(fileName, damagedHelmetMeshCollection));
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
