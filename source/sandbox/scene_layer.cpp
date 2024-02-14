#include "bootstrap.hpp"
#include "scene_layer.hpp"

#include <benzin/core/imgui_helper.hpp>
#include <benzin/core/math.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/mapped_data.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/rt_acceleration_structures.hpp>
#include <benzin/graphics/shaders.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/texture_loader.hpp>
#include <benzin/system/key_event.hpp>
#include <benzin/utility/random.hpp>

#include <shaders/joint/constant_buffer_types.hpp>
#include <shaders/joint/root_constants.hpp>
#include <shaders/joint/structured_buffer_types.hpp>

namespace sandbox
{

    namespace
    {

        enum CPUTiming : uint32_t
        {
            CPUTiming_BuildTopLevelAS,
            CPUTiming_GeometryPass,
            CPUTiming_RTShadowPass,
            CPUTiming_DeferredLightingPass,
            CPUTiming_EnvironmentPass,
            CPUTiming_BackBufferCopy,
            CPUTiming_SceneLayerOnUpdate,
            CPUTiming_SceneLayerOnRender,
        };

        enum GPUTiming : uint32_t
        {
            GPUTiming_GeometryPass,
            GPUTiming_RTShadowPass,
            GPUTiming_DeferredLightingPass,
            GPUTiming_EnvironmentPass,
            GPUTiming_BackBufferCopy,
            GPUTiming_Total,
        };

        void ImGuiDisplayTexture(const benzin::Texture& texture)
        {
            BenzinAssert(texture.HasShaderResourceView());

            const float widgetWidth = ImGui::GetContentRegionAvail().x;

            const uint64_t gpuHandle = texture.GetShaderResourceView().GetGPUHandle();
            const float textureAspectRatio = (float)texture.GetWidth() / (float)texture.GetHeight();

            const ImVec2 textureSize{ widgetWidth, widgetWidth / textureAspectRatio };

            const ImVec2 uv0{ 0.0f, 0.0f };
            const ImVec2 uv1{ 1.0f, 1.0f };
            const ImVec4 tintColor{ 1.0f, 1.0f, 1.0f, 1.0f };
            const ImVec4 borderColor{ 1.0f, 1.0f, 1.0f, 0.3f };

            ImGui::Image((ImTextureID)gpuHandle, textureSize, uv0, uv1, tintColor, borderColor);
        };

        benzin::MeshCollectionResource CreatePointLightMeshCollection(uint32_t spheresPerRowCount, uint32_t spheresPerColumnCount)
        {
            const uint32_t totalCount = spheresPerRowCount * spheresPerColumnCount;

            benzin::MeshCollectionResource resultMeshCollection;
            resultMeshCollection.DebugName = "PointLights";
            resultMeshCollection.Meshes.push_back(benzin::GetDefaultGeoSphereMesh());
            resultMeshCollection.Materials.reserve(totalCount);
            resultMeshCollection.MeshInstances.reserve(totalCount);

            for (const auto _ : std::views::iota(0u, spheresPerRowCount * spheresPerColumnCount))
            {
                const DirectX::XMFLOAT3 emissiveFactor
                {
                    benzin::Random::Get<float>(0.0f, 1.0f),
                    benzin::Random::Get<float>(0.0f, 1.0f),
                    benzin::Random::Get<float>(0.0f, 1.0f)
                };

                resultMeshCollection.Materials.push_back(benzin::Material
                {
                    .AlbedoFactor{ 0.0f, 0.0f, 0.0f, 0.0f },
                    .EmissiveFactor = emissiveFactor,
                });

                resultMeshCollection.MeshInstances.push_back(benzin::MeshInstance
                {
                    .MeshIndex = 0, // Only one mesh in MeshCollectionResource
                    .MaterialIndex = (uint32_t)resultMeshCollection.Materials.size() - 1,
                });
            }

            resultMeshCollection.Materials.push_back(benzin::Material
            {
                .AlbedoFactor{ 0.0f, 0.0f, 0.0f, 0.0f },
                .EmissiveFactor{ 1.0f, 1.0f, 1.0f },
            });

            resultMeshCollection.MeshInstances.push_back(benzin::MeshInstance
            {
                .MeshIndex = 0, // Only one mesh in MeshCollectionResource
                .MaterialIndex = (uint32_t)resultMeshCollection.Materials.size() - 1,
            });

            return resultMeshCollection;
        }

        bool IsMeshCulled(const benzin::Camera& camera, const benzin::MeshCollection& meshCollection, uint32_t meshInstanceIndex, const DirectX::XMMATRIX& worldMatrix)
        {
            const auto& meshInstance = meshCollection.MeshInstances[meshInstanceIndex];
            const auto& mesh = meshCollection.Meshes[meshInstance.MeshIndex];

            if (!mesh.BoundingBox)
            {
                return false;
            }

            const auto localToViewSpaceTransformMatrix = meshCollection.GetMeshParentTransform(meshInstance.MeshParentTransformIndex) * worldMatrix * camera.GetViewMatrix();
            const auto viewSpaceMeshBoundingBox = benzin::TransformBoundingBox(*mesh.BoundingBox, localToViewSpaceTransformMatrix);

            return camera.GetProjection().GetBoundingFrustum().Contains(viewSpaceMeshBoundingBox) == DirectX::DISJOINT;
        }

        std::array<std::chrono::microseconds, magic_enum::enum_count<CPUTiming>()> g_CPUTimings;

        constexpr auto g_GBuffer_Color0Format = benzin::GraphicsFormat::RGBA8Unorm;
        constexpr auto g_GBuffer_Color1Format = benzin::GraphicsFormat::RGBA16Float;
        constexpr auto g_GBuffer_Color2Format = benzin::GraphicsFormat::RGBA8Unorm;
        constexpr auto g_GBuffer_Color3Format = benzin::GraphicsFormat::RGBA8Unorm;
        constexpr auto g_GBuffer_DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint;

        constexpr std::wstring_view g_RTShadowPass_RayGenShaderName = L"RayGen";
        constexpr std::wstring_view g_RTShadowPass_MissShaderName = L"Miss";
        constexpr std::wstring_view g_RTShadowPass_HitGroupName = L"HitGroup";

    } // anonymous namespace

    // GeometryPass

    GeometryPass::GeometryPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
    {
        m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, benzin::GraphicsPipelineStateCreation
        {
            .DebugName = "GeometryPass",
            .VertexShader{ "geometry_pass.hlsl", "VS_Main" },
            .PixelShader{ "geometry_pass.hlsl", "PS_Main" },
            .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
            .RasterizerState
            {
                .TriangleOrder = benzin::TriangleOrder::CounterClockwise,
            },
            .RenderTargetFormats
            {
                g_GBuffer_Color0Format,
                g_GBuffer_Color1Format,
                g_GBuffer_Color2Format,
                g_GBuffer_Color3Format,
            },
            .DepthStencilFormat = g_GBuffer_DepthStencilFormat,
        });

        OnResize((uint32_t)m_SwapChain.GetViewport().Width, (uint32_t)m_SwapChain.GetViewport().Height);
    }

    void GeometryPass::OnRender(const benzin::Scene& scene)
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.Albedo, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.WorldNormal, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.Emissive, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.RoughnessMetalness, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.DepthStencil, benzin::ResourceState::DepthWrite });

        commandList.SetRenderTargets(
            {
                m_GBuffer.Albedo->GetRenderTargetView(),
                m_GBuffer.WorldNormal->GetRenderTargetView(),
                m_GBuffer.Emissive->GetRenderTargetView(),
                m_GBuffer.RoughnessMetalness->GetRenderTargetView(),
            },
            &m_GBuffer.DepthStencil->GetDepthStencilView()
        );

        commandList.ClearRenderTarget(m_GBuffer.Albedo->GetRenderTargetView());
        commandList.ClearRenderTarget(m_GBuffer.WorldNormal->GetRenderTargetView());
        commandList.ClearRenderTarget(m_GBuffer.Emissive->GetRenderTargetView());
        commandList.ClearRenderTarget(m_GBuffer.RoughnessMetalness->GetRenderTargetView());
        commandList.ClearDepthStencil(m_GBuffer.DepthStencil->GetDepthStencilView());

        commandList.SetPipelineState(*m_PipelineState);

        const auto view = scene.GetEntityRegistry().view<benzin::TransformComponent, benzin::MeshInstanceComponent>();
        for (const auto entityHandle : view)
        {
            const auto& tc = view.get<benzin::TransformComponent>(entityHandle);
            const auto& mic = view.get<benzin::MeshInstanceComponent>(entityHandle);

            const auto& meshCollection = scene.GetMeshCollection(mic.MeshUnionIndex);
            const auto& meshCollectionGPUStorage = scene.GetMeshCollectionGPUStorage(mic.MeshUnionIndex);

            commandList.SetRootResource(joint::GeometryPassRC_MeshVertexBuffer, meshCollectionGPUStorage.VertexBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRC_MeshIndexBuffer, meshCollectionGPUStorage.IndexBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRC_MeshInfoBuffer, meshCollectionGPUStorage.MeshInfoBuffer->GetShaderResourceView());
            commandList.SetRootConstant(joint::GeometryPassRC_MeshParentTransformBuffer, meshCollectionGPUStorage.MeshParentTransformBuffer ? meshCollectionGPUStorage.MeshParentTransformBuffer->GetShaderResourceView().GetHeapIndex() : benzin::g_InvalidIndex<uint32_t>);
            commandList.SetRootResource(joint::GeometryPassRC_MeshInstanceBuffer, meshCollectionGPUStorage.MeshInstanceBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRC_MaterialBuffer, meshCollectionGPUStorage.MaterialBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRC_MeshTransformConstantBuffer, tc.Buffer->GetConstantBufferView(m_Device.GetActiveFrameIndex()));

            const auto meshInstanceRange = mic.MeshInstanceRange.value_or(meshCollection.GetFullMeshInstanceRange());
            for (const auto i : benzin::IndexRangeToView(meshInstanceRange))
            {
                if (IsMeshCulled(scene.GetCamera(), meshCollection, i, tc.GetWorldMatrix()))
                {
                    continue;
                }

                commandList.SetRootConstant(joint::GeometryPassRC_MeshInstanceIndex, i);

                const auto& meshInstance = meshCollection.MeshInstances[i];
                const auto& mesh = meshCollection.Meshes[meshInstance.MeshIndex];

                commandList.SetPrimitiveTopology(mesh.PrimitiveTopology);
                commandList.DrawVertexed((uint32_t)mesh.Indices.size());
            }
        }

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.Albedo, benzin::ResourceState::Present });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.WorldNormal, benzin::ResourceState::Present });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.Emissive, benzin::ResourceState::Present });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.RoughnessMetalness, benzin::ResourceState::Present });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.DepthStencil, benzin::ResourceState::Present });
    }

    void GeometryPass::OnResize(uint32_t width, uint32_t height)
    {
        static const auto CreateGBufferRenderTarget = [](
            benzin::Device& device,
            benzin::GraphicsFormat format,
            uint32_t width,
            uint32_t height,
            std::string_view debugName
        )
        {
            return std::make_unique<benzin::Texture>(device, benzin::TextureCreation
            {
                .DebugName = debugName,
                .Type = benzin::TextureType::Texture2D,
                .Format = format,
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowRenderTarget,
                .ClearValue = benzin::g_DefaultClearColor,
                .IsNeedShaderResourceView = true,
                .IsNeedRenderTargetView = true,
            });
        };

        m_GBuffer.Albedo = CreateGBufferRenderTarget(m_Device, g_GBuffer_Color0Format, width, height, "GBuffer_Albedo");
        m_GBuffer.WorldNormal = CreateGBufferRenderTarget(m_Device, g_GBuffer_Color1Format, width, height, "GBuffer_WorldNormal");
        m_GBuffer.Emissive = CreateGBufferRenderTarget(m_Device, g_GBuffer_Color2Format, width, height, "GBuffer_Emissive");
        m_GBuffer.RoughnessMetalness = CreateGBufferRenderTarget(m_Device, g_GBuffer_Color3Format, width, height, "GBuffer_RoughnessMetalness");

        m_GBuffer.DepthStencil = std::make_unique<benzin::Texture>(m_Device, benzin::TextureCreation
        {
            .DebugName = "GBuffer_DepthStencil",
            .Type = benzin::TextureType::Texture2D,
            .Format = g_GBuffer_DepthStencilFormat,
            .Width = width,
            .Height = height,
            .MipCount = 1,
            .Flags = benzin::TextureFlag::AllowDepthStencil,
            .ClearValue = benzin::g_DefaultClearDepthStencil,
            .IsNeedDepthStencilView = true,
        });
        m_GBuffer.DepthStencil->PushShaderResourceView({ .Format{ benzin::GraphicsFormat::D24Unorm_X8Typeless } });
    }

    // RTShadowPass

    RTShadowPass::RTShadowPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
    {
        CreatePipelineStateObject();
        CreateShaderTable();

        m_PassConstantBuffer = benzin::CreateFrameDependentConstantBuffer<joint::RTShadowPassConstants>(m_Device, "RTShadowPassConstantBuffer");

        m_OutputTexture = std::make_unique<benzin::Texture>(m_Device, benzin::TextureCreation
        {
            .DebugName = "RTShadowPass_Output",
            .Type = benzin::TextureType::Texture2D,
            .Format = benzin::GraphicsFormat::RGBA8Unorm,
            .Width = (uint32_t)swapChain.GetViewport().Width,
            .Height = (uint32_t)swapChain.GetViewport().Height,
            .MipCount = 1,
            .Flags = benzin::TextureFlag::AllowUnorderedAccess,
            .IsNeedShaderResourceView = true, // For debugging
            .IsNeedUnorderedAccessView = true,
        });
    }

    void RTShadowPass::OnUpdate(std::chrono::microseconds dt, std::chrono::milliseconds elapsedTime)
    {
        const joint::RTShadowPassConstants constants
        {
            .DeltaTime = benzin::ToFloatMS(dt),
            .ElapsedTime = (float)elapsedTime.count(),
            .RandomFloat01 = benzin::Random::Get(0.0f, 1.0f),
            .RaysPerPixel = m_RaysPerPixel,
        };

        benzin::MappedData passConstantBuffer{ *m_PassConstantBuffer };
        passConstantBuffer.Write(constants, m_Device.GetActiveFrameIndex());
    }

    void RTShadowPass::OnRender(const benzin::Scene& scene, const GeometryPass::GBuffer& gbuffer)
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();
        auto* d3d12CommandList = commandList.GetD3D12GraphicsCommandList();

        d3d12CommandList->SetPipelineState1(m_D3D12RaytracingStateObject.Get());

        const auto& activeTopLevelAS = scene.GetActiveTopLevelAS();

        commandList.SetRootResource(joint::RTShadowPassRC_PassConstantBuffer, m_PassConstantBuffer->GetConstantBufferView(m_Device.GetActiveFrameIndex()));
        commandList.SetRootResource(joint::RTShadowPassRC_GBufferWorldNormalTexture, gbuffer.WorldNormal->GetShaderResourceView());
        commandList.SetRootResource(joint::RTShadowPassRC_GBufferDepthTexture, gbuffer.DepthStencil->GetShaderResourceView());
        commandList.SetRootResource(joint::RTShadowPassRC_PointLightBuffer, scene.GetPointLightBufferSRV());
        commandList.SetRootResource(joint::RTShadowPassRC_OutputTexture, m_OutputTexture->GetUnorderedAccessView());

        d3d12CommandList->SetComputeRootShaderResourceView(1, activeTopLevelAS.GetBuffer().GetGPUVirtualAddress());

        const D3D12_DISPATCH_RAYS_DESC d3d12DispatchRayDesc
        {
            .RayGenerationShaderRecord
            {
                .StartAddress = m_RayGenShaderTable->GetGPUVirtualAddress(),
                .SizeInBytes = m_RayGenShaderTable->GetNotAlignedSizeInBytes(),
            },
            .MissShaderTable
            {
                .StartAddress = m_MissShaderTable->GetGPUVirtualAddress(),
                .SizeInBytes = m_MissShaderTable->GetNotAlignedSizeInBytes(),
                .StrideInBytes = m_MissShaderTable->GetElementSize(),
            },
            .HitGroupTable
            {
                .StartAddress = m_HitGroupShaderTable->GetGPUVirtualAddress(),
                .SizeInBytes = m_HitGroupShaderTable->GetNotAlignedSizeInBytes(),
                .StrideInBytes = m_HitGroupShaderTable->GetElementSize(),
            },
            .CallableShaderTable
            {
                .StartAddress = 0,
                .SizeInBytes = 0,
                .StrideInBytes = 0,
            },
            .Width = m_OutputTexture->GetWidth(),
            .Height = m_OutputTexture->GetHeight(),
            .Depth = 1,
        };

        d3d12CommandList->DispatchRays(&d3d12DispatchRayDesc);
    }

    void RTShadowPass::CreatePipelineStateObject()
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
            .HitGroupExport = g_RTShadowPass_HitGroupName.data(),
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

    void RTShadowPass::CreateShaderTable()
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

        m_RayGenShaderTable = CreateShaderTable(g_RTShadowPass_RayGenShaderName);
        m_MissShaderTable = CreateShaderTable(g_RTShadowPass_MissShaderName);
        m_HitGroupShaderTable = CreateShaderTable(g_RTShadowPass_HitGroupName);
    }

    // DeferredLightingPass

    DeferredLightingPass::DeferredLightingPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
    {
        m_PassConstantBuffer = benzin::CreateFrameDependentConstantBuffer<joint::DeferredLightingPassConstants>(m_Device, "DeferredLightingPassConstantBuffer");

        m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, benzin::GraphicsPipelineStateCreation
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
            .RenderTargetFormats{ benzin::GraphicsFormat::RGBA8Unorm },
        });

        OnResize((uint32_t)m_SwapChain.GetViewport().Width, (uint32_t)m_SwapChain.GetViewport().Height);
    }

    void DeferredLightingPass::OnUpdate(const benzin::Scene& scene)
    {
        const joint::DeferredLightingPassConstants constants
        {
            .SunColor = m_SunColor,
            .SunIntensity = m_SunIntensity,
            .SunDirection = m_SunDirection,
            .ActivePointLightCount = scene.GetStats().PointLightCount,
            .OutputType = magic_enum::enum_integer(m_OutputType),
        };

        benzin::MappedData passConstantBuffer{ *m_PassConstantBuffer };
        passConstantBuffer.Write(constants, m_Device.GetActiveFrameIndex());
    }

    void DeferredLightingPass::OnRender(const benzin::Scene& scene, const GeometryPass::GBuffer& gbuffer, const benzin::Texture& shadowTexture)
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_OutputTexture, benzin::ResourceState::RenderTarget });

        commandList.SetRenderTargets({ m_OutputTexture->GetRenderTargetView() });
        commandList.ClearRenderTarget(m_OutputTexture->GetRenderTargetView());

        {
            commandList.SetPipelineState(*m_PipelineState);

            commandList.SetRootResource(joint::DeferredLightingPassRC_PassConstantBuffer, m_PassConstantBuffer->GetConstantBufferView(m_Device.GetActiveFrameIndex()));
            commandList.SetRootResource(joint::DeferredLightingPassRC_AlbedoTexture, gbuffer.Albedo->GetShaderResourceView());
            commandList.SetRootResource(joint::DeferredLightingPassRC_WorldNormalTexture, gbuffer.WorldNormal->GetShaderResourceView());
            commandList.SetRootResource(joint::DeferredLightingPassRC_EmissiveTexture, gbuffer.Emissive->GetShaderResourceView());
            commandList.SetRootResource(joint::DeferredLightingPassRC_RoughnessMetalnessTexture, gbuffer.RoughnessMetalness->GetShaderResourceView());
            commandList.SetRootResource(joint::DeferredLightingPassRC_DepthStencilTexture, gbuffer.DepthStencil->GetShaderResourceView());
            commandList.SetRootResource(joint::DeferredLightingPassRC_ShadowTexture, shadowTexture.GetShaderResourceView());
            commandList.SetRootResource(joint::DeferredLightingPassRC_PointLightBuffer, scene.GetPointLightBufferSRV());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_OutputTexture, benzin::ResourceState::Present });
    }

    void DeferredLightingPass::OnImGuiRender()
    {
        ImGui::Begin("DeferredLightingPass");
        {
            ImGui::ColorEdit3("SunColor", reinterpret_cast<float*>(&m_SunColor));
            ImGui::DragFloat("SunIntensity", &m_SunIntensity, 0.1f, 0.0f, 100.0f);

            if (ImGui::DragFloat3("SunDirection", reinterpret_cast<float*>(&m_SunDirection), 0.01f, -1.0f, 1.0f))
            {
                DirectX::XMStoreFloat3(&m_SunDirection, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_SunDirection)));
            }

            ImGui::Separator();

            static constexpr auto names = magic_enum::enum_names<joint::DeferredLightingOutputType>();

            if (ImGui::BeginListBox("##listbox", ImVec2{ -FLT_MIN, 0.0f }))
            {
                for (const auto i : std::views::iota(0u, joint::DeferredLightingOutputType_Count))
                {
                    const bool isSelected = m_OutputType == i;

                    if (ImGui::Selectable(names[i].data(), isSelected))
                    {
                        m_OutputType = (joint::DeferredLightingOutputType)i;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndListBox();
            }
        }
        ImGui::End();
    }

    void DeferredLightingPass::OnResize(uint32_t width, uint32_t height)
    {
        m_OutputTexture = std::make_unique<benzin::Texture>(m_Device, benzin::TextureCreation
        {
            .DebugName = "DeferredLightingPass_OutputTexture",
            .Type = benzin::TextureType::Texture2D,
            .Format = benzin::GraphicsSettingsInstance::Get().BackBufferFormat,
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
        m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, benzin::GraphicsPipelineStateCreation
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
            .RenderTargetFormats{ benzin::GraphicsFormat::RGBA8Unorm },
            .DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint,
        });

        {
            m_CubeTexture = std::unique_ptr<benzin::Texture>{ m_Device.GetTextureLoader().LoadCubeMapFromHDRFile("scythian_tombs_2_4k.hdr", 1024) };
            m_CubeTexture->PushShaderResourceView();
        }
    }

    void EnvironmentPass::OnRender(const benzin::Scene& scene, benzin::Texture& deferredLightingOutputTexture, benzin::Texture& gbufferDepthStecil)
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ deferredLightingOutputTexture, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ gbufferDepthStecil, benzin::ResourceState::DepthWrite });

        commandList.SetRenderTargets({ deferredLightingOutputTexture.GetRenderTargetView() }, &gbufferDepthStecil.GetDepthStencilView());

        {
            commandList.SetPipelineState(*m_PipelineState);

            commandList.SetRootResource(joint::EnvironmentPassRC_CubeMapTexture, m_CubeTexture->GetShaderResourceView());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ deferredLightingOutputTexture, benzin::ResourceState::Present });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ gbufferDepthStecil, benzin::ResourceState::Present });
    }

    // SceneLayer

    SceneLayer::SceneLayer(const benzin::GraphicsRefs& graphicsRefs)
        : m_Window{ graphicsRefs.WindowRef }
        , m_Device{ graphicsRefs.DeviceRef }
        , m_SwapChain{ graphicsRefs.SwapChainRef }
        , m_GeometryPass{ m_Device, m_SwapChain }
        , m_RTShadowPass{ m_Device, m_SwapChain }
        , m_DeferredLightingPass{ m_Device, m_SwapChain }
        , m_EnvironmentPass{ m_Device, m_SwapChain }
    {
        m_GPUTimer = std::make_unique<benzin::GPUTimer>(m_Device, benzin::GPUTimerCreation
        {
            .CommandList = m_Device.GetGraphicsCommandQueue().GetCommandList(),
            .TimestampFrequency = m_Device.GetGraphicsCommandQueue().GetTimestampFrequency(),
            .TimerCount = (uint32_t)magic_enum::enum_count<GPUTiming>(),
        });

        auto& perspectiveProjection = m_Scene.GetPerspectiveProjection();
        perspectiveProjection.SetLens(DirectX::XMConvertToRadians(60.0f), m_SwapChain.GetAspectRatio(), 0.1f, 1000.0f);

        auto& camera = m_Scene.GetCamera();
        camera.SetPosition({ -3.0f, 2.0f, -0.25f });
        camera.SetFrontDirection({ 1.0f, 0.0f, 0.0f });

        CreateEntities();

        {
            BenzinLogTimeOnScopeExit("Upload scene data to GPU");
            m_Scene.UploadMeshCollections();
        }

        {
            BenzinLogTimeOnScopeExit("Build scene RT BottomLevel ASs");
            m_Scene.BuildBottomLevelAccelerationStructures();
        }

        {
            BenzinLogTimeOnScopeExit("Build scene RT TopLevel AS");
            m_Scene.BuildTopLevelAccelerationStructure();
        }
    }

    SceneLayer::~SceneLayer() = default;

    void SceneLayer::OnEndFrame()
    {
        m_GPUTimer->ResolveTimestamps();
    }

    void SceneLayer::OnEvent(benzin::Event& event)
    {
        m_FlyCameraController.OnEvent(event);

        benzin::EventDispatcher dispatcher{ event };
        {
            dispatcher.Dispatch<benzin::WindowResizedEvent>([&](auto& event)
            {
                m_GeometryPass.OnResize(event.GetWidth(), event.GetHeight());
                m_DeferredLightingPass.OnResize(event.GetWidth(), event.GetHeight());

                return false;
            });

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
        BenzinGrabTimeOnScopeExit(g_CPUTimings[CPUTiming_SceneLayerOnUpdate]);

        {
            BenzinGrabTimeOnScopeExit(g_CPUTimings[CPUTiming_BuildTopLevelAS]);
            m_Scene.BuildTopLevelAccelerationStructure();
        }

        const auto dt = s_FrameTimer.GetDeltaTime();
        const auto elapsedTime = s_FrameTimer.GetElapsedTime();

        m_FlyCameraController.OnUpdate(dt);
        m_Scene.OnUpdate(dt);

        m_RTShadowPass.OnUpdate(dt, elapsedTime);
        m_DeferredLightingPass.OnUpdate(m_Scene);
    }

    void SceneLayer::OnRender()
    {
        BenzinGrabTimeOnScopeExit(g_CPUTimings[CPUTiming_SceneLayerOnRender]);

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        BenzinGrabGPUTimeOnScopeExit(*m_GPUTimer, GPUTiming_Total);

        commandList.SetRootResource(joint::GlobalRC_CameraConstantBuffer, m_Scene.GetCameraConstantBufferCBV());

        {
            BenzinGrabTimeOnScopeExit(g_CPUTimings[CPUTiming_GeometryPass]);
            BenzinGrabGPUTimeOnScopeExit(*m_GPUTimer, GPUTiming_GeometryPass);
            m_GeometryPass.OnRender(m_Scene);
        }

        {
            BenzinGrabTimeOnScopeExit(g_CPUTimings[CPUTiming_RTShadowPass]);
            BenzinGrabGPUTimeOnScopeExit(*m_GPUTimer, GPUTiming_RTShadowPass);
            m_RTShadowPass.OnRender(m_Scene, m_GeometryPass.GetGBuffer());
        }

        {
            BenzinGrabTimeOnScopeExit(g_CPUTimings[CPUTiming_DeferredLightingPass]);
            BenzinGrabGPUTimeOnScopeExit(*m_GPUTimer, GPUTiming_DeferredLightingPass);
            m_DeferredLightingPass.OnRender(m_Scene, m_GeometryPass.GetGBuffer(), m_RTShadowPass.GetOutputTexture());
        }

        if (m_DeferredLightingPass.GetOutputType() == joint::DeferredLightingOutputType_Final)
        {
            BenzinGrabTimeOnScopeExit(g_CPUTimings[CPUTiming_EnvironmentPass]);
            BenzinGrabGPUTimeOnScopeExit(*m_GPUTimer, GPUTiming_EnvironmentPass);
            m_EnvironmentPass.OnRender(m_Scene, m_DeferredLightingPass.GetOutputTexture(), *m_GeometryPass.GetGBuffer().DepthStencil);
        }

        {
            BenzinGrabTimeOnScopeExit(g_CPUTimings[CPUTiming_BackBufferCopy]);
            BenzinGrabGPUTimeOnScopeExit(*m_GPUTimer, GPUTiming_BackBufferCopy);

            auto& currentBackBuffer = m_SwapChain.GetCurrentBackBuffer();
            auto& deferredLightingOutputTexture = m_DeferredLightingPass.GetOutputTexture();

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::CopyDestination });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ deferredLightingOutputTexture, benzin::ResourceState::CopySource });

            commandList.CopyResource(currentBackBuffer, m_DeferredLightingPass.GetOutputTexture());

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::Present });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ deferredLightingOutputTexture, benzin::ResourceState::Present });
        }
    }

    void SceneLayer::OnImGuiRender()
    {
        m_FlyCameraController.OnImGuiRender();
        m_DeferredLightingPass.OnImGuiRender();

        ImGui::Begin("Misc");
        {
            ImGuiDisplayTexture(*m_GeometryPass.GetGBuffer().Albedo);
            ImGuiDisplayTexture(*m_GeometryPass.GetGBuffer().WorldNormal);
            ImGuiDisplayTexture(m_RTShadowPass.GetOutputTexture());

            ImGui::Separator();

            static int raysPerPixel = 1; 
            if (ImGui::SliderInt("RTShadows RaysPerPixel", &raysPerPixel, 0, 100))
            {
                m_RTShadowPass.SetRaysPerPixel(raysPerPixel);
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
            benzin::ImGui_Text("VertexCount: {:L}", sceneStats.VertexCount);
            benzin::ImGui_Text("TriangleCount: {:L}", sceneStats.TriangleCount);
            benzin::ImGui_Text("PointLightCount: {:L}", sceneStats.PointLightCount);
        }
        ImGui::End();

        ImGui::Begin("CPU Timings");
        {
            static auto cpuTimings = g_CPUTimings;

            if (s_IsUpdateStatsIntervalPassed)
            {
                cpuTimings = g_CPUTimings;
            }

            for (const auto [i, timing] : cpuTimings | std::views::enumerate)
            {
                benzin::ImGui_Text("{}: {:.4f} ms", magic_enum::enum_name((CPUTiming)i).substr("CPUTiming"sv.size() + 1), benzin::ToFloatMS(timing));
            }
        }
        ImGui::End();

        ImGui::Begin("GPU Timings");
        {
            static constexpr size_t gpuTimingCount = magic_enum::enum_count<GPUTiming>();
            static constexpr size_t enumNameOffset = "GPUTiming"sv.size() + 1;

            static std::array<std::chrono::microseconds, gpuTimingCount> gpuTimings;

            if (s_IsUpdateStatsIntervalPassed)
            {
                for (const auto& [i, timing] : gpuTimings | std::views::enumerate)
                {
                    timing = m_GPUTimer->GetElapsedTime((uint32_t)i);
                }
            }

            for (const auto [i, timing] : gpuTimings | std::views::enumerate)
            {
                benzin::ImGui_Text("{}: {:.4f} ms", magic_enum::enum_name((GPUTiming)i).substr("GPUTiming"sv.size() + 1), benzin::ToFloatMS(timing));
            }
        }
        ImGui::End();
    }

    void SceneLayer::CreateEntities()
    {
        auto& entityRegistry = m_Scene.GetEntityRegistry();

        benzin::MeshCollectionResource sponzaMeshCollection;
        const auto sponzaFuture = std::async(std::launch::async, [&]
        {
            const std::string_view fileName = "Sponza/glTF/Sponza.gltf";

            BenzinLogTimeOnScopeExit(std::format("Loading MeshCollection from {}", fileName));
            BenzinAssert(LoadMeshCollectionFromGLTFFile(fileName, sponzaMeshCollection));
        });

        benzin::MeshCollectionResource boomBooxMeshCollection;
        {
            const std::string_view fileName = "BoomBox/glTF/BoomBox.gltf";

            BenzinLogTimeOnScopeExit(std::format("Loading MeshCollection from {}", fileName));
            BenzinAssert(LoadMeshCollectionFromGLTFFile(fileName, boomBooxMeshCollection));
        }

        benzin::MeshCollectionResource damagedHelmetMeshCollection;
        {
            const std::string_view fileName = "DamagedHelmet/glTF/DamagedHelmet.gltf";

            BenzinLogTimeOnScopeExit(std::format("Loading MeshCollection from {}", fileName));
            BenzinAssert(LoadMeshCollectionFromGLTFFile(fileName, damagedHelmetMeshCollection));
        }

        const size_t spheresPerRowCount = 20;
        const size_t spheresPerColumnCount = 6;
        benzin::MeshCollectionResource pointLightMeshCollection = CreatePointLightMeshCollection(spheresPerRowCount, spheresPerColumnCount);

        sponzaFuture.wait();

        const uint32_t sponzaMeshUnionIndex = m_Scene.PushMeshCollection(std::move(sponzaMeshCollection));
        const uint32_t boomBooxMeshUnionIndex = m_Scene.PushMeshCollection(std::move(boomBooxMeshCollection));
        const uint32_t damagedHelmetMeshUnionIndex = m_Scene.PushMeshCollection(std::move(damagedHelmetMeshCollection));
        const uint32_t pointLightsMeshUnionIndex = m_Scene.PushMeshCollection(std::move(pointLightMeshCollection));

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sponzaMeshUnionIndex;

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.Rotation = { 0.0f, DirectX::XM_PI, 0.0f };
            tc.Translation = { 5.0f, 0.0f, 0.0f };
        }

        {
            m_BoomBoxEntity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(m_BoomBoxEntity);
            mic.MeshUnionIndex = boomBooxMeshUnionIndex;

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(m_BoomBoxEntity);
            tc.Rotation = { 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f };
            tc.Scale = { 30.0f, 30.0f, 30.0f };
            tc.Translation = { 0.0f, 0.6f, 0.0f };

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(m_BoomBoxEntity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle, std::chrono::microseconds dt)
            {
                if (!m_IsAnimationEnabled)
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);
                tc.Rotation.x += 0.0001f * benzin::ToFloatMS(dt);
                tc.Rotation.z += 0.0002f * benzin::ToFloatMS(dt);
            };
        }

        {
            m_DamagedHelmetEntity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(m_DamagedHelmetEntity);
            mic.MeshUnionIndex = damagedHelmetMeshUnionIndex;

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(m_DamagedHelmetEntity);
            tc.Rotation = { 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f };
            tc.Scale = { 0.4f, 0.4f, 0.4f };
            tc.Translation = { 1.0f, 0.5f, -0.5f };

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(m_DamagedHelmetEntity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle, std::chrono::microseconds dt)
            {
                if (!m_IsAnimationEnabled)
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);
                tc.Rotation.x += 0.0001f * benzin::ToFloatMS(dt);
                tc.Rotation.y -= 0.00015f * benzin::ToFloatMS(dt);
            };
        }

#if 0
        {
            for (const auto i : std::views::iota(0u, spheresPerRowCount))
            {
                for (const auto j : std::views::iota(0u, spheresPerColumnCount))
                {
                    const uint32_t currentPointLightIndex = i * spheresPerColumnCount + j;

                    const auto entity = entityRegistry.create();

                    auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
                    mic.MeshUnionIndex = pointLightsMeshUnionIndex;
                    mic.MeshInstanceRange = benzin::IndexRangeU32{ currentPointLightIndex, 1 };

                    auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
                    tc.Scale = { 0.02f, 0.02f, 0.02f };
                    tc.Translation = DirectX::XMFLOAT3{ ((float)spheresPerRowCount / -2.0f + (float)i) * 0.5f, 0.2f, ((float)spheresPerColumnCount / -2.0f + (float)j) * 0.5f };

                    // auto& plc = entityRegistry.emplace<benzin::PointLightComponent>(entity);
                    // plc.Color = m_Scene.GetMeshCollection(pointLightsMeshUnionIndex).Materials[currentPointLightIndex].EmissiveFactor;
                    // plc.Intensity = 3.0f;
                    // plc.Range = 4.0f;
                }
            }
        }
#endif

        {
            const uint32_t pointLightIndex = spheresPerRowCount * spheresPerColumnCount;

            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = pointLightsMeshUnionIndex;
            mic.MeshInstanceRange = benzin::IndexRangeU32{ pointLightIndex, 1 };

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.Scale = { 0.2f, 0.2f, 0.2f };
            tc.Translation = DirectX::XMFLOAT3{ 0.5f, 1.5f, -0.25f };

            auto& plc = entityRegistry.emplace<benzin::PointLightComponent>(entity);
            plc.Color = m_Scene.GetMeshCollection(pointLightsMeshUnionIndex).Materials[pointLightIndex].EmissiveFactor;
            plc.Intensity = 10.0f;
            plc.Range = 30.0f;
            plc.GeometryRadius = 0.2f;

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(entity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle, std::chrono::microseconds dt)
            {
                static constexpr float travelRadius = 1.0f;
                static constexpr float travelSpeed = 0.0004f;

                static std::chrono::microseconds elapsedTime;

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                static const float startX = tc.Translation.x;
                static const float startZ = tc.Translation.z;

                if (m_IsAnimationEnabled)
                {
                    elapsedTime += dt;

                    tc.Translation.x = startX + travelRadius * std::cos(travelSpeed * benzin::ToFloatMS(elapsedTime));
                    tc.Translation.z = startZ + travelRadius * std::sin(travelSpeed * benzin::ToFloatMS(elapsedTime));
                }
            };
        }
    }

} // namespace sandbox
