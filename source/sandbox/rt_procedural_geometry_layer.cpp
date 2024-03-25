#include "bootstrap.hpp"
#include "rt_procedural_geometry_layer.hpp"

#include <benzin/core/command_line_args.hpp>
#include <benzin/core/memory_writer.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/rt_acceleration_structures.hpp>
#include <benzin/graphics/shaders.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/system/window.hpp>

#include <shaders/joint/structured_buffer_types.hpp>

namespace sandbox
{

    enum class RayType : uint8_t
    {
        Radiance,
        Shadow,
    };

    enum class AABBAnalyticGeometryType : uint8_t
    {
        AABB,
        Spheres,
    };

    enum class AABBVolumetricGeometryType : uint8_t
    {
        Metaballs,
    };

    enum class AABBSignedDistanceGeometryType : uint8_t
    {
        MiniSpheres,
        IntersectedRoundCube,
        SquareTorus,
        TwistedTorus,
        Cog,
        Cylinder,
        FractalPyramid,
    };

    enum class AABBGeometryType : uint32_t
    {
        Analytic,
        Volumetric,
        SignedDistance,
    };

    constexpr auto g_AABBGeometryCounts = magic_enum::containers::to_array<AABBGeometryType>(
    {
        magic_enum::enum_count<AABBAnalyticGeometryType>(),
        magic_enum::enum_count<AABBVolumetricGeometryType>(),
        magic_enum::enum_count<AABBSignedDistanceGeometryType>(),
    });

    constexpr auto g_TotalAABBGeometryCount = std::ranges::fold_left(g_AABBGeometryCounts, 0, std::plus{});

    using GeometryType = RtProceduralGeometryLayer::GeometryType;

    constexpr auto g_GeometryCounts = magic_enum::containers::to_array<GeometryType>(
    {
        1ull,
        magic_enum::enum_count<AABBGeometryType>(),
    });

    constexpr auto g_TotalGeometryCount = std::ranges::fold_left(g_GeometryCounts, 0, std::plus{});

    constexpr auto g_RayGenShaderName = L"RayGen";

    constexpr auto g_ClosestHitShaderNames = magic_enum::containers::to_array<GeometryType>(
    {
        L"Triangle_RadianceRay_ClosestHit",
        L"AABB_RadianceRay_ClosestHit",
    });

    constexpr auto g_MissShaderNames = magic_enum::containers::to_array<GeometryType>(
    {
        L"RadianceRay_Miss",
        L"ShadowRay_Miss",
    });

    constexpr auto g_AABBIntersectionShaderNames = magic_enum::containers::to_array<AABBGeometryType>(
    {
        L"AnalyticAABB_Intersection",
        L"VolumetricAABB_Intersection",
        L"SignedDistanceAABB_Intersection",
    });

    constexpr auto g_TriangleHitGroupNames = magic_enum::containers::to_array<RayType>(
    {
        L"Triangle_RadianceRay_HitGroup",
        L"Triangle_ShadowRay_HitGroup",
    });

    constexpr auto g_AABBHitGroupNames = magic_enum::containers::to_array<AABBGeometryType>(
    {
        magic_enum::containers::to_array<RayType>(
        {
            L"AnalyticAABB_RadianceRay_HitGroup",
            L"AnalyticAABB_ShadowRay_HitGroup",
        }),
        magic_enum::containers::to_array<RayType>(
        {
            L"VolumetricAABB_RadianceRay_HitGroup",
            L"VolumetricAABB_ShadowRay_HitGroup",
        }),
        magic_enum::containers::to_array<RayType>(
        {
            L"SignedDistanceAABB_RadianceRay_HitGroup",
            L"SignedDistanceAABB_ShadowRay_HitGroup",
        }),
    });

    constexpr auto g_TotalHitGroupCount = g_TotalGeometryCount * magic_enum::enum_count<RayType>();

    struct RadianceRayPayload
    {
        DirectX::XMFLOAT4 Color;
        uint32_t RecursionDepth = 0;
    };

    struct ShadowRayPayload
    {
        bool IsHitted = false;
    };

    struct ProceduralPrimitiveAttributes
    {
        DirectX::XMFLOAT3 Normal;
    };

    struct SceneConstants
    {
        DirectX::XMMATRIX InverseViewProjectionMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMVECTOR CameraPosition = DirectX::XMVectorZero();
        DirectX::XMVECTOR LightPosition = DirectX::XMVectorZero();
        DirectX::XMVECTOR LightAmbientColor = DirectX::XMVectorZero();
        DirectX::XMVECTOR LightDiffuseColor = DirectX::XMVectorZero();
        float Reflectance = 0.0f;
        float ElapsedTime = 0.0f;
    };

    struct MeshMaterial
    {
        DirectX::XMFLOAT4 Albedo{ 0.0f, 0.0f, 0.0f, 0.0f };
        float ReflectanceCoefficient = 0.0f;
        float DiffuseCoefficient = 0.0f;
        float SpecularCoefficient = 0.0f;
        float SpecularPower = 0.0f;
        float StepScale = 0.0f;
    };

    struct ProceduralGeometryConstants
    {
        DirectX::XMMATRIX LocalSpaceToBottomLevelASMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX BottomLevelASToLocalSpaceMatrix = DirectX::XMMatrixIdentity();
        uint32_t MaterialIndex = 0;
        AABBGeometryType AABBGeometryType = AABBGeometryType::Analytic;

        DirectX::XMFLOAT2 __UnusedPadding;
    };

    enum class GPUTimerIndex
    {
        _DispatchRays,
        _CopyRaytracingOutput,
    };

    //

    RtProceduralGeometryLayer::RtProceduralGeometryLayer(const benzin::GraphicsRefs& graphicsRefs)
        : m_Window{ graphicsRefs.WindowRef }
        , m_Device{ graphicsRefs.DeviceRef }
        , m_SwapChain{ graphicsRefs.SwapChainRef }
    {
        m_GPUTimer = std::make_unique<benzin::GpuTimer>(m_Device, benzin::GpuTimerCreation
        {
            .CommandList = m_Device.GetGraphicsCommandQueue().GetCommandList(),
            .TimestampFrequency = m_Device.GetGraphicsCommandQueue().GetTimestampFrequency(),
            .TimerCount = (uint32_t)magic_enum::enum_count<GPUTimerIndex>(),
        });

        CreateGeometry();

        CreatePipelineStateObject();
        CreateAccelerationStructures();
        CreateShaderTables();
        CreateRaytracingResources();

        m_PerspectiveProjection.SetLens(DirectX::XMConvertToRadians(60.0f), m_SwapChain.GetAspectRatio(), 0.1f, 1000.0f);
        m_Camera.SetPosition(DirectX::XMVECTOR{ -4.0f, 3.0f, 2.0f });
        m_FlyCameraController.SetCameraPitchYaw(-18.0f, 22.0f);
    }

    RtProceduralGeometryLayer::~RtProceduralGeometryLayer() = default;

    void RtProceduralGeometryLayer::OnEvent(benzin::Event& event)
    {
        m_FlyCameraController.OnEvent(event);
    }

    void RtProceduralGeometryLayer::OnUpdate()
    {
        const auto dt = s_FrameTimer.GetDeltaTime();

        m_FlyCameraController.OnUpdate(dt);

        {
            const SceneConstants constants
            {
                .InverseViewProjectionMatrix = m_Camera.GetInverseViewProjectionMatrix(),
                .CameraPosition = m_Camera.GetPosition(),
                .LightPosition = DirectX::XMVECTOR{ 5.0f, 7.0f, 9.0f, 0.0f },
                .LightAmbientColor = DirectX::XMVECTOR{ 0.25f, 0.25f, 0.25f, 1.0f },
                .LightDiffuseColor = DirectX::XMVECTOR{ 0.6f, 0.6f, 0.6f, 1.0f },
                .Reflectance = 0.0f,
            };

            const benzin::MemoryWriter writer{ m_SceneConstantBuffer->GetMappedData(), m_SceneConstantBuffer->GetSizeInBytes() };
            writer.Write(constants);
        }
    }

    void RtProceduralGeometryLayer::OnRender()
    {
        using magic_enum::enum_integer;

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        auto* d3d12Device = m_Device.GetD3D12Device();
        auto* d3d12CommandList = commandList.GetD3D12GraphicsCommandList();

        {
            enum RootConstant : uint32_t
            {
                RootConstant_TopLevelASIndex,
                RootConstant_SceneConstantBufferIndex,
                RootConstant_TriangleVertexBufferIndex,
                RootConstant_TriangleIndexBufferIndex,
                RootConstant_MeshMaterialBufferIndex,
                RootConstant_ProceduralGeometryBufferIndex,
                RootConstant_RaytracingOutputTextureIndex,
            };

            commandList.SetRootResource(RootConstant_TopLevelASIndex, m_TopLevelAS->GetBuffer().GetRtAsSrv());
            commandList.SetRootResource(RootConstant_SceneConstantBufferIndex, m_SceneConstantBuffer->GetCbv());
            commandList.SetRootResource(RootConstant_TriangleVertexBufferIndex, m_GridVertexBuffer->GetStructuredSrv());
            commandList.SetRootResource(RootConstant_TriangleIndexBufferIndex, m_GridIndexBuffer->GetFormatSrv(benzin::GraphicsFormat::R32Uint));
            commandList.SetRootResource(RootConstant_MeshMaterialBufferIndex, m_MeshMaterialBuffer->GetStructuredSrv());
            commandList.SetRootResource(RootConstant_ProceduralGeometryBufferIndex, m_ProceduralGeometryBuffer->GetStructuredSrv());
            commandList.SetRootResource(RootConstant_RaytracingOutputTextureIndex, m_RaytracingOutput->GetUav());

            d3d12CommandList->SetPipelineState1(m_D3D12RaytracingStateObject.Get());
        }

        {
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
                .Width = m_RaytracingOutput->GetWidth(),
                .Height = m_RaytracingOutput->GetHeight(),
                .Depth = 1,
            };

            {
                BenzinGrabGpuTimeOnScopeExit(*m_GPUTimer, enum_integer(GPUTimerIndex::_DispatchRays));
                d3d12CommandList->DispatchRays(&d3d12DispatchRayDesc);
            }
        }

        {
            auto& currentBackBuffer = m_SwapChain.GetCurrentBackBuffer();

            commandList.SetResourceBarriers(
            {
                benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::CopyDestination },
                benzin::TransitionBarrier{ *m_RaytracingOutput, benzin::ResourceState::CopySource },
            });

            {
                BenzinGrabGpuTimeOnScopeExit(*m_GPUTimer, enum_integer(GPUTimerIndex::_CopyRaytracingOutput));
                commandList.CopyResource(currentBackBuffer, *m_RaytracingOutput);
            }

            commandList.SetResourceBarriers(
            {
                benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::Common },
                benzin::TransitionBarrier{ *m_RaytracingOutput, benzin::ResourceState::UnorderedAccess },
            });
        }

        m_GPUTimer->ResolveTimestamps(m_Device.GetCpuFrameIndex());
    }

    void RtProceduralGeometryLayer::OnImGuiRender()
    {
        m_FlyCameraController.OnImGuiRender();

        ImGui::Begin("Stats");
        {
            static magic_enum::containers::array<GPUTimerIndex, float> times;

            for (const auto& [index, time] : times | std::views::enumerate)
            {
                time = m_GPUTimer->GetElapsedTime((uint32_t)index).count() / 1000.0f;
            }

            for (const auto& [index, time] : times | std::views::enumerate)
            {
                const auto indexName = magic_enum::enum_name(magic_enum::enum_value<GPUTimerIndex>(index)).substr(1);
                ImGui::Text(std::format("GPU {} Time: {:.4f} ms", indexName, time).c_str());
            }
        }
        ImGui::End();
    }

    void RtProceduralGeometryLayer::CreateGeometry()
    {
        m_MeshMaterialBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "MeshMaterialBuffer",
            .ElementSize = sizeof(MeshMaterial),
            .ElementCount = 1 + g_TotalAABBGeometryCount,
            .Flags = benzin::BufferFlag::UploadBuffer,
        });

        // Triangular mesh
        {
            const benzin::MeshData gridMesh = benzin::GetDefaultGridMesh();

            m_GridVertexBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
            {
                .DebugName = "GridVertexBuffer",
                .ElementSize = sizeof(joint::MeshVertex),
                .ElementCount = (uint32_t)gridMesh.Vertices.size(),
            });

            m_GridIndexBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
            {
                .DebugName = "GridIndexBuffer",
                .ElementSize = sizeof(uint32_t),
                .ElementCount = (uint32_t)gridMesh.Indices.size(),
            });

            {
                auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
                BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

                auto& copyCommandList = copyCommandQueue.GetCommandList(m_GridVertexBuffer->GetSizeInBytes() + m_GridIndexBuffer->GetSizeInBytes());

                copyCommandList.UpdateBuffer(*m_GridVertexBuffer, std::span<const joint::MeshVertex>{ gridMesh.Vertices });
                copyCommandList.UpdateBuffer(*m_GridIndexBuffer, std::span<const uint32_t>{ gridMesh.Indices });
            }

            const MeshMaterial gridMaterial
            {
                .Albedo = DirectX::XMFLOAT4{ 0.3f, 0.6f, 0.3f, 1.0f },
                .ReflectanceCoefficient = 0.25f,
                .DiffuseCoefficient = 1.0f,
                .SpecularCoefficient = 0.4f,
                .SpecularPower = 50.0f,
                .StepScale = 1.0f,
            };

            const benzin::MemoryWriter writer{ m_MeshMaterialBuffer->GetMappedData(), m_MeshMaterialBuffer->GetSizeInBytes() };
            writer.Write(gridMaterial, 0);
        }

        // AABB meshes
        {
            static const auto GetD3D12RaytracingAABB = [&](uint32_t aabbType, uint32_t aabbIndex)
            {
                static const float size = 2.0f; // Unit size for aabb (-1 -> 1)
                static const float stride = 2.0f;

                const float x = (float)aabbType * (stride + size);
                const float z = (float)aabbIndex * (stride + size);

                return D3D12_RAYTRACING_AABB
                {
                    .MinX = x,
                    .MinY = 0.0f,
                    .MinZ = z,
                    .MaxX = x + size,
                    .MaxY = size,
                    .MaxZ = z + size,
                };
            };

            std::vector<D3D12_RAYTRACING_AABB> d3d12RaytracingAABBs;
            d3d12RaytracingAABBs.reserve(g_TotalAABBGeometryCount);

            for (auto aabbType : magic_enum::enum_values<AABBGeometryType>())
            {
                for (uint32_t i = 0; i < g_AABBGeometryCounts[aabbType]; ++i)
                {
                    d3d12RaytracingAABBs.push_back(GetD3D12RaytracingAABB(magic_enum::enum_integer(aabbType), i));
                }
            }

            m_AABBBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
            {
                .ElementSize = sizeof(D3D12_RAYTRACING_AABB),
                .ElementCount = g_TotalAABBGeometryCount,
                .Flags = benzin::BufferFlag::UploadBuffer,
                .InitialData = std::as_bytes(std::span{ d3d12RaytracingAABBs }),
            });

            m_ProceduralGeometryBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
            {
                .ElementSize = sizeof(ProceduralGeometryConstants),
                .ElementCount = g_TotalAABBGeometryCount,
                .Flags = benzin::BufferFlag::UploadBuffer,
            });

            const DirectX::XMFLOAT4 chromiumReflectance{ 0.549f, 0.556f, 0.554f, 1.0f };

            const benzin::MemoryWriter meshMaterialWriter{ m_MeshMaterialBuffer->GetMappedData(), m_MeshMaterialBuffer->GetSizeInBytes() };
            for (uint32_t i = 0; i < g_TotalAABBGeometryCount; ++i)
            {
                const float albedoChannelValue = (float)i / 9.0f;

                const MeshMaterial material
                {
                    .Albedo = chromiumReflectance,
                    .ReflectanceCoefficient = 0.2f,
                    .DiffuseCoefficient = 0.9f,
                    .SpecularCoefficient = 0.2f,
                    .SpecularPower = 50.0f,
                    .StepScale = 1.0f,
                };

                meshMaterialWriter.Write(material, 1 + i);
            }

            const benzin::MemoryWriter proceduralGeometryWriter{ m_ProceduralGeometryBuffer->GetMappedData(), m_ProceduralGeometryBuffer->GetSizeInBytes() };
            for (uint32_t i = 0; i < g_TotalAABBGeometryCount; ++i)
            {
                using namespace DirectX;

                const auto aabbMin = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&d3d12RaytracingAABBs[i].MinX));
                const auto aabbMax = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&d3d12RaytracingAABBs[i].MaxX));

                const XMVECTOR translation = 0.5f * (XMVectorAdd(aabbMin, aabbMax));
                const XMMATRIX translationMatrix = XMMatrixTranslationFromVector(translation);

                const ProceduralGeometryConstants constants
                {
                    .LocalSpaceToBottomLevelASMatrix = translationMatrix,
                    .BottomLevelASToLocalSpaceMatrix = XMMatrixInverse(nullptr, translationMatrix),
                    .MaterialIndex = 1 + i,
                    // .AABBGeometryType = AABBGeometryType::Analytic,
                };

                proceduralGeometryWriter.Write(constants, i);
            }
        }
    }

    void RtProceduralGeometryLayer::CreatePipelineStateObject()
    {
        static const auto CreateD3D12TriangleHitGroupDesc = [](RayType ray)
        {
            return D3D12_HIT_GROUP_DESC
            {
                .HitGroupExport = g_TriangleHitGroupNames[ray],
                .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES,
                .AnyHitShaderImport = nullptr,
                .ClosestHitShaderImport = ray == RayType::Radiance ? g_ClosestHitShaderNames[GeometryType::Triangle] : nullptr,
                .IntersectionShaderImport = nullptr,
            };
        };

        static const auto CreateD3D12AABBHitGroupDesc = [](AABBGeometryType aabbGeometry, RayType ray)
        {
            return D3D12_HIT_GROUP_DESC
            {
                .HitGroupExport = g_AABBHitGroupNames[aabbGeometry][ray],
                .Type = D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE,
                .AnyHitShaderImport = nullptr,
                .ClosestHitShaderImport = ray == RayType::Radiance ? g_ClosestHitShaderNames[GeometryType::AABB] : nullptr,
                .IntersectionShaderImport = g_AABBIntersectionShaderNames[aabbGeometry],
            };
        };

        // 1. D3D12_GLOBAL_ROOT_SIGNATURE
        const D3D12_GLOBAL_ROOT_SIGNATURE d3d12GlobalRootSignature
        {
            .pGlobalRootSignature = m_Device.GetD3D12BindlessRootSignature(),
        };

        // 2. D3D12_DXIL_LIBRARY_DESC
        const std::span<const std::byte> libraryBinary = benzin::GetShaderBinary(benzin::ShaderType::Library, { "raytracing_procedural_geometry.hlsl" });

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
        std::array<D3D12_HIT_GROUP_DESC, g_TotalHitGroupCount> d3d12HitGroupDescs;
        size_t d3d12HitGroupDescOffset = 0;

        for (auto ray : magic_enum::enum_values<RayType>())
        {
            d3d12HitGroupDescs[d3d12HitGroupDescOffset++] = CreateD3D12TriangleHitGroupDesc(ray);
        }

        for (auto aabbGeometry : magic_enum::enum_values<AABBGeometryType>())
        {
            for (auto ray : magic_enum::enum_values<RayType>())
            {
                d3d12HitGroupDescs[d3d12HitGroupDescOffset++] = CreateD3D12AABBHitGroupDesc(aabbGeometry, ray);
            }
        }

        // 4. D3D12_RAYTRACING_SHADER_CONFIG
        const D3D12_RAYTRACING_SHADER_CONFIG d3d12RaytracingShaderConfig
        {
            .MaxPayloadSizeInBytes = (uint32_t)std::max(sizeof(RadianceRayPayload), sizeof(ShadowRayPayload)),
            .MaxAttributeSizeInBytes = sizeof(ProceduralPrimitiveAttributes),
        };

        // 5. D3D12_RAYTRACING_PIPELINE_CONFIG
        const D3D12_RAYTRACING_PIPELINE_CONFIG d3d12RaytracingPipelineConfig
        {
            .MaxTraceRecursionDepth = 1,
        };

        // Create ID3D12StateObject
        std::vector<D3D12_STATE_SUBOBJECT> d3d12StateSubObjects
        {
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &d3d12GlobalRootSignature },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &d3d12DXILLibraryDesc },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &d3d12RaytracingShaderConfig },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &d3d12RaytracingPipelineConfig },
        };

        d3d12StateSubObjects.append_range(d3d12HitGroupDescs | std::views::transform([](const D3D12_HIT_GROUP_DESC& d3d12HitGroupDesc)
        {
            return D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &d3d12HitGroupDesc };
        }));

        const D3D12_STATE_OBJECT_DESC d3d12StateObjectDesc
        {
            .Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
            .NumSubobjects = (uint32_t)d3d12StateSubObjects.size(),
            .pSubobjects = d3d12StateSubObjects.data(),
        };

        BenzinAssert(m_Device.GetD3D12Device()->CreateStateObject(&d3d12StateObjectDesc, IID_PPV_ARGS(&m_D3D12RaytracingStateObject)));
    }

    void RtProceduralGeometryLayer::CreateAccelerationStructures()
    {
        auto& graphicsCommandQueue = m_Device.GetGraphicsCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(graphicsCommandQueue);

        auto& commandList = graphicsCommandQueue.GetCommandList();

        // BottomLevel Triangles
        {
            const benzin::RtGeometryVariant gridGeometry = benzin::RtTriangledGeometry
            {
                .VertexBuffer = *m_GridVertexBuffer,
                .IndexBuffer = *m_GridIndexBuffer,
            };

            m_BottomLevelASs[GeometryType::Triangle] = std::make_unique<benzin::BottomLevelAccelerationStructure>(m_Device, benzin::BottomLevelAccelerationStructureCreation
            {
                .DebugName = "Grid",
                .Geometries = std::span{ &gridGeometry, 1 },
            });
        }
        
        // BottomLevel Procedural
        {
            std::vector<benzin::RtGeometryVariant> proceduralGeometries;
            proceduralGeometries.reserve(g_TotalAABBGeometryCount);

            for (size_t i = 0; i < g_TotalAABBGeometryCount; ++i)
            {
                proceduralGeometries.push_back(benzin::RtProceduralGeometry
                {
                    .AABBBuffer = *m_AABBBuffer,
                    .AABBIndex = (uint32_t)i,
                });
            }

            m_BottomLevelASs[GeometryType::AABB] = std::make_unique<benzin::BottomLevelAccelerationStructure>(m_Device, benzin::BottomLevelAccelerationStructureCreation
            {
                .DebugName = "Procedural",
                .Geometries = proceduralGeometries,
            });
        }

        // TopLevel
        {
            const auto instanceCollection = std::to_array<benzin::TopLevelInstance>(
            {
                {
                    .BottomLevelAccelerationStructure = *m_BottomLevelASs[GeometryType::Triangle],
                    .HitGroupIndex = 0,
                    .Transform = DirectX::XMMatrixTranspose(DirectX::XMMatrixScaling(1000.0f, 1.0f, 1000.0f) * DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f)),
                },
                {
                    .BottomLevelAccelerationStructure = *m_BottomLevelASs[GeometryType::AABB],
                    .HitGroupIndex = 2,
                },
            });

            m_TopLevelAS = std::make_unique<benzin::TopLevelAccelerationStructure>(m_Device, benzin::TopLevelAccelerationStructureCreation
            {
                .Instances = instanceCollection,
            });
        }

        // Building
        {
            for (const auto& blas : m_BottomLevelASs)
            {
                commandList.SetResourceBarrier({ benzin::TransitionBarrier{ blas->GetScratchResource(), benzin::ResourceState::UnorderedAccess } });
                commandList.BuildRayTracingAccelerationStructure(*blas);
            }

            commandList.SetResourceBarriers(
                m_BottomLevelASs |
                std::views::transform([](const auto& bottomLevelAS) { return benzin::UnorderedAccessBarrier{ bottomLevelAS->GetBuffer() }; }) |
                std::ranges::to<std::vector<benzin::ResourceBarrierVariant>>()
            );

            commandList.SetResourceBarrier({ benzin::TransitionBarrier{ m_TopLevelAS->GetScratchResource(), benzin::ResourceState::UnorderedAccess } });
            commandList.BuildRayTracingAccelerationStructure(*m_TopLevelAS);
        }
    }

    void RtProceduralGeometryLayer::CreateShaderTables()
    {
    #if 1
        using ShaderID = std::span<const std::byte>;

        BenzinAssert(m_D3D12RaytracingStateObject.Get());

        ComPtr<ID3D12StateObjectProperties> d3d12StateObjectProperties;
        BenzinAssert(m_D3D12RaytracingStateObject.As(&d3d12StateObjectProperties));

        const auto GetShaderID = [&d3d12StateObjectProperties](const wchar_t* shaderName)
        {
            return ShaderID{ (const std::byte*)d3d12StateObjectProperties->GetShaderIdentifier(shaderName), benzin::config::g_ShaderIdentifierSizeInBytes };
        };

        const auto GetHitGroupIDs = [&]
        {
            const size_t triangleHitGroupCount = 1 * magic_enum::enum_count<RayType>();
            const size_t aabbHitGroupCount = std::ranges::fold_left(g_AABBGeometryCounts, 0, std::plus{}) * magic_enum::enum_count<RayType>();
            const size_t totalHitGroupCount = triangleHitGroupCount + aabbHitGroupCount;

            std::array<ShaderID, totalHitGroupCount> hitGroupIDs;
            size_t hitGroupIDOffset = 0;

            for (const auto& triangleHitGroupName : g_TriangleHitGroupNames)
            {
                hitGroupIDs[hitGroupIDOffset++] = GetShaderID(triangleHitGroupName);
            }

            for (const auto& [i, aabbGeometryHitGroupNames] : g_AABBHitGroupNames | std::views::enumerate)
            {
                for (size_t instanceIndex = 0; instanceIndex < g_AABBGeometryCounts[(AABBGeometryType)i]; ++instanceIndex)
                {
                    for (const auto aabbHitGroupName : aabbGeometryHitGroupNames)
                    {
                        hitGroupIDs[hitGroupIDOffset++] = GetShaderID(aabbHitGroupName);
                    }
                }
            }

            return hitGroupIDs;
        };

        const auto rayGenShaderID = GetShaderID(g_RayGenShaderName);
        const auto missShaderIDs = benzin::TransformArray(g_MissShaderNames.a, GetShaderID);
        const auto hitGroupIDs = GetHitGroupIDs();

        const size_t uploadBufferSize = (1 + missShaderIDs.size() + hitGroupIDs.size()) * benzin::config::g_ShaderIdentifierSizeInBytes;

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& commandList = copyCommandQueue.GetCommandList((uint32_t)uploadBufferSize);

        const auto CreateShaderTable = [&](std::string_view debugName, std::span<const ShaderID> shaderIDs)
        {
            auto shaderTable = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
            {
                .DebugName = std::format("{}ShaderTable", debugName),
                .ElementSize = benzin::config::g_ShaderIdentifierSizeInBytes,
                .ElementCount = (uint32_t)shaderIDs.size(),
            });

            for (const auto& [i, shaderID] : shaderIDs | std::views::enumerate)
            {
                commandList.UpdateBuffer(*shaderTable, shaderID, i * benzin::config::g_ShaderIdentifierSizeInBytes);
            }

            return std::move(shaderTable);
        };

        m_RayGenShaderTable = CreateShaderTable("RayGen", std::span{ &rayGenShaderID, 1 });
        m_MissShaderTable = CreateShaderTable("Miss", missShaderIDs);
        m_HitGroupShaderTable = CreateShaderTable("HitGroup", hitGroupIDs);
    #else

        

    #endif
    }

    void RtProceduralGeometryLayer::CreateRaytracingResources()
    {
        m_SceneConstantBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "SceneConstantBuffer",
            .ElementSize = sizeof(SceneConstants),
            .ElementCount = 1,
            .Flags = benzin::BufferFlag::ConstantBuffer,
        });

        m_RaytracingOutput = std::make_unique<benzin::Texture>(m_Device, benzin::TextureCreation
        {
            .DebugName = "RayTracingOutput",
            .Format = benzin::CommandLineArgs::GetBackBufferFormat(),
            .Width = m_Window.GetWidth(),
            .Height = m_Window.GetHeight(),
            .MipCount = 1,
            .Flags = benzin::TextureFlag::AllowUnorderedAccess,
        });
    }

} // namespace sandbox
