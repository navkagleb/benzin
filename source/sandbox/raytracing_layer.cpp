#include "bootstrap.hpp"
#include "raytracing_layer.hpp"

#include <benzin/engine/model.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/shaders.hpp>

namespace sandbox
{

    namespace
    {

        const std::wstring_view g_RayGenShaderName = L"RayGen";
        const std::wstring_view g_ClosestHitShaderName = L"ClosestHit";
        const std::wstring_view g_MissShaderName = L"Miss";
        const std::wstring_view g_HitGroupShaderName = L"HitGroup";

        enum class LocalRootSignatureParams
        {
            ViewportConstant,
        };

        enum class GPUTimerIndex
        {
            DispatchRays,
        };

    } // anonymous namespace

    RaytracingLayer::RaytracingLayer(const benzin::GraphicsRefs& graphicsRefs)
        : m_Window{ graphicsRefs.WindowRef }
        , m_Device{ graphicsRefs.DeviceRef }
        , m_SwapChain{ graphicsRefs.SwapChainRef }
    {
        m_GPUTimer = std::make_shared<benzin::GPUTimer>(m_Device, m_Device.GetGraphicsCommandQueue(), magic_enum::enum_count<GPUTimerIndex>());

        CreateEntities();

        CreateRootSignatures();
        CreateRaytracingStateObject();
        CreateAccelerationStructures();
        CreateShaderTables();
        CreateRaytracingResources();
    }

    void RaytracingLayer::OnEndFrame()
    {
        m_GPUTimer->OnEndFrame(m_Device.GetGraphicsCommandQueue().GetCommandList());
    }

    void RaytracingLayer::OnRender()
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        auto* d3d12Device = m_Device.GetD3D12Device();
        auto* d3d12CommandList = commandList.GetD3D12GraphicsCommandList();

        {
            enum class RootConstant : uint32_t
            {
                TopLevelASIndex,
                RaytracingOutputTextureIndex,
            };

            commandList.SetRootConstant(RootConstant::TopLevelASIndex, m_TLAS->GetShaderResourceView().GetHeapIndex());
            commandList.SetRootConstant(RootConstant::RaytracingOutputTextureIndex, m_RaytracingOutput->GetUnorderedAccessView().GetHeapIndex());

            d3d12CommandList->SetPipelineState1(m_D3D12RaytracingStateObject.Get());
        }
        
        {
            const D3D12_DISPATCH_RAYS_DESC d3d12DispatchRayDesc
            {
                .RayGenerationShaderRecord
                {
                    .StartAddress = m_RayGenShaderTable->GetGPUVirtualAddress(),
                    .SizeInBytes = m_RayGenShaderTable->GetElementCount(),
                },
                .MissShaderTable
                {
                    .StartAddress = m_MissShaderTable->GetGPUVirtualAddress(),
                    .SizeInBytes = m_MissShaderTable->GetElementCount(),
                    .StrideInBytes = m_MissShaderTable->GetElementCount(),
                },
                .HitGroupTable
                {
                    .StartAddress = m_HitGroupShaderTable->GetGPUVirtualAddress(),
                    .SizeInBytes = m_HitGroupShaderTable->GetElementCount(),
                    .StrideInBytes = m_HitGroupShaderTable->GetElementCount(),
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
                BenzinGPUTimerSlotScopeMeasurement(*m_GPUTimer, commandList, GPUTimerIndex::DispatchRays);
                d3d12CommandList->DispatchRays(&d3d12DispatchRayDesc);
            }
        }

        {
            const auto CreateD3D12TransitionBarrier = [](benzin::Resource& resource, benzin::ResourceState stateAfter)
            {
                const auto stateBefore = resource.GetCurrentState();
                resource.SetCurrentState(stateAfter);

                return D3D12_RESOURCE_BARRIER
                {
                    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                    .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                    .Transition
                    {
                        .pResource = resource.GetD3D12Resource(),
                        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        .StateBefore = static_cast<D3D12_RESOURCE_STATES>(stateBefore),
                        .StateAfter = static_cast<D3D12_RESOURCE_STATES>(stateAfter),
                    },
                };
            };

            auto& backBuffer = m_SwapChain.GetCurrentBackBuffer();

            {
                const D3D12_RESOURCE_BARRIER d3d12ResourceBarriers[]
                {
                    CreateD3D12TransitionBarrier(*backBuffer, benzin::ResourceState::CopyDestination),
                    CreateD3D12TransitionBarrier(*m_RaytracingOutput, benzin::ResourceState::CopySource),
                };

                d3d12CommandList->ResourceBarrier(static_cast<uint32_t>(std::size(d3d12ResourceBarriers)), d3d12ResourceBarriers);
            }

            d3d12CommandList->CopyResource(backBuffer->GetD3D12Resource(), m_RaytracingOutput->GetD3D12Resource());

            {
                const D3D12_RESOURCE_BARRIER d3d12ResourceBarriers[]
                {
                    CreateD3D12TransitionBarrier(*backBuffer, benzin::ResourceState::Present),
                    CreateD3D12TransitionBarrier(*m_RaytracingOutput, benzin::ResourceState::UnorderedAccess),
                };

                d3d12CommandList->ResourceBarrier(static_cast<uint32_t>(std::size(d3d12ResourceBarriers)), d3d12ResourceBarriers);
            }
        }
    }

    void RaytracingLayer::OnImGuiRender()
    {
        ImGui::Begin("Stats");
        {
            static float dispatchRaysTime = 0.0f;

            if (s_FrameStats.IsReady())
            {
                dispatchRaysTime = m_GPUTimer->GetElapsedTime(GPUTimerIndex::DispatchRays).count();
            }

            ImGui::Text(std::format("DispatchRays Time: {:.4f} ms", dispatchRaysTime).c_str());
        }
        ImGui::End();
    }

    void RaytracingLayer::CreateEntities()
    {
        const uint32_t vertexSize = sizeof(DirectX::XMFLOAT3);
        const uint32_t indexSize = sizeof(uint32_t);

        const float depthValue = 1.0f;
        const float offset = 0.7f;
            
        m_Vertices =
        {
            DirectX::XMFLOAT3{ 0.0f, -offset, depthValue },
            DirectX::XMFLOAT3{ -offset, offset, depthValue },
            DirectX::XMFLOAT3{ offset, offset, depthValue }
        };

        m_Indices = { 0, 1, 2 };

        m_VertexBuffer = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation{ .ElementSize = vertexSize, .ElementCount = static_cast<uint32_t>(m_Vertices.size()) });
        m_IndexBuffer = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation{ .ElementSize = indexSize, .ElementCount = static_cast<uint32_t>(m_Indices.size()) });

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(m_VertexBuffer->GetSizeInBytes() + m_IndexBuffer->GetSizeInBytes());

        copyCommandList.UpdateBuffer(*m_VertexBuffer, std::span<const DirectX::XMFLOAT3>{ m_Vertices });
        copyCommandList.UpdateBuffer(*m_IndexBuffer, std::span<const uint32_t>{ m_Indices });
    }

    void RaytracingLayer::CreateRootSignatures()
    {
        const auto SerializeAndCreateD3D12RootSignature = [&](const D3D12_ROOT_SIGNATURE_DESC& d3d12RootSignatureDesc, ComPtr<ID3D12RootSignature>& d3d12RootSignature)
        {
            ComPtr<ID3DBlob> d3d12Blob;
            ComPtr<ID3DBlob> d3d12Error;

            const HRESULT hr = D3D12SerializeRootSignature(
                &d3d12RootSignatureDesc,
                D3D_ROOT_SIGNATURE_VERSION_1,
                &d3d12Blob,
                &d3d12Error
            );

            if (FAILED(hr))
            {
                BenzinError("Failed to serialize RootSignature. Error: {}", reinterpret_cast<const char*>(d3d12Error->GetBufferPointer()));
                return;
            }

            BenzinAssert(m_Device.GetD3D12Device()->CreateRootSignature(1, d3d12Blob->GetBufferPointer(), d3d12Blob->GetBufferSize(), IID_PPV_ARGS(&d3d12RootSignature)));
        };

        // m_D3D12LocalRootSignature
        {
            const D3D12_ROOT_PARAMETER d3d12RootParameter
            {
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
                .Constants
                {
                    .ShaderRegister = 1,
                    .RegisterSpace = 0,
                    .Num32BitValues = (sizeof(RayGenConstants) - 1) / sizeof(uint32_t) + 1,
                },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
            };

            const D3D12_ROOT_SIGNATURE_DESC d3d12RootSignatureDesc
            {
                .NumParameters = 1,
                .pParameters = &d3d12RootParameter,
                .NumStaticSamplers = 0,
                .pStaticSamplers = nullptr,
                .Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE,
            };

            SerializeAndCreateD3D12RootSignature(d3d12RootSignatureDesc, m_D3D12LocalRootSignature);
        }
    }

    void RaytracingLayer::CreateRaytracingStateObject()
    {
        // D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG
        // D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE
        // D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE
        // D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK
        // D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY
        // D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION
        // D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION
        // D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION
        // D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG
        // D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG
        // D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP
        // D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG1
        // D3D12_STATE_SUBOBJECT_TYPE_MAX_VALID

        auto* d3d12BindlessRootSignature = m_Device.GetD3D12BindlessRootSignature();

        std::vector<D3D12_STATE_SUBOBJECT> d3d12StateSubObjects;
        d3d12StateSubObjects.reserve(7); // HARDCODE!

        // 1. D3D12_GLOBAL_ROOT_SIGNATURE
        const D3D12_GLOBAL_ROOT_SIGNATURE d3d12GlobalRootSignature
        {
            .pGlobalRootSignature = m_Device.GetD3D12BindlessRootSignature(),
        };

        d3d12StateSubObjects.emplace_back(D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &d3d12GlobalRootSignature);

        // 2. D3D12_LOCAL_ROOT_SIGNATURE
        const D3D12_LOCAL_ROOT_SIGNATURE d3d12LocalRootSignature
        {
            .pLocalRootSignature = m_D3D12LocalRootSignature.Get(),
        };

        d3d12StateSubObjects.emplace_back(D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &d3d12LocalRootSignature);

        // 3. D3D12_DXIL_LIBRARY_DESC
        const std::span<const std::byte> libraryBinary = benzin::GetShaderBinary(benzin::ShaderType::Library, { "default_raytracing.hlsl" });

        std::vector<D3D12_EXPORT_DESC> d3d12ExportDescs
        {
            D3D12_EXPORT_DESC{ .Name = g_RayGenShaderName.data(), .ExportToRename = nullptr, .Flags = D3D12_EXPORT_FLAG_NONE },
            D3D12_EXPORT_DESC{ .Name = g_ClosestHitShaderName.data(), .ExportToRename = nullptr, .Flags = D3D12_EXPORT_FLAG_NONE },
            D3D12_EXPORT_DESC{ .Name = g_MissShaderName.data(), .ExportToRename = nullptr, .Flags = D3D12_EXPORT_FLAG_NONE },
        };

        const D3D12_DXIL_LIBRARY_DESC d3d12DXILLibraryDesc
        {
            .DXILLibrary
            {
                .pShaderBytecode = libraryBinary.data(),
                .BytecodeLength = libraryBinary.size(),
            },
            .NumExports = static_cast<uint32_t>(d3d12ExportDescs.size()),
            .pExports = d3d12ExportDescs.data(),
        };

        d3d12StateSubObjects.emplace_back(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &d3d12DXILLibraryDesc);

        // 4. D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION
        const wchar_t* rayGenExportName = g_RayGenShaderName.data();

        const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION d3d12SubobjectToExportsAssociation
        {
            .pSubobjectToAssociate = &d3d12StateSubObjects[1], // HARDCODE!
            .NumExports = 1,
            .pExports = &rayGenExportName,
        };

        d3d12StateSubObjects.emplace_back(D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &d3d12SubobjectToExportsAssociation);

        // 5. D3D12_HIT_GROUP_DESC
        const D3D12_HIT_GROUP_DESC d3d12HitGroupDesc
        {
            .HitGroupExport = g_HitGroupShaderName.data(),
            .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES,
            .AnyHitShaderImport = nullptr,
            .ClosestHitShaderImport = g_ClosestHitShaderName.data(),
            .IntersectionShaderImport = nullptr,
        };

        d3d12StateSubObjects.emplace_back(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &d3d12HitGroupDesc);

        // 6. D3D12_RAYTRACING_SHADER_CONFIG
        const D3D12_RAYTRACING_SHADER_CONFIG d3d12RaytracingShaderConfig
        {
            .MaxPayloadSizeInBytes = sizeof(DirectX::XMFLOAT4), // Color
            .MaxAttributeSizeInBytes = sizeof(DirectX::XMFLOAT2), // Barycentrics
        };

        d3d12StateSubObjects.emplace_back(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &d3d12RaytracingShaderConfig);

        // 7. D3D12_RAYTRACING_PIPELINE_CONFIG
        const D3D12_RAYTRACING_PIPELINE_CONFIG d3d12RaytracingPipelineConfig
        {
            .MaxTraceRecursionDepth = 1,
        };

        d3d12StateSubObjects.emplace_back(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &d3d12RaytracingPipelineConfig);

        const D3D12_STATE_OBJECT_DESC d3d12StateObjectDesc
        {
            .Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
            .NumSubobjects = static_cast<uint32_t>(d3d12StateSubObjects.size()),
            .pSubobjects = d3d12StateSubObjects.data(),
        };

        m_Device.GetD3D12Device()->CreateStateObject(&d3d12StateObjectDesc, IID_PPV_ARGS(&m_D3D12RaytracingStateObject));
    }

    void RaytracingLayer::CreateAccelerationStructures()
    {
        const D3D12_RAYTRACING_GEOMETRY_DESC d3d12RaytracingGeometryDesc
        {
            .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
            .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
            .Triangles
            {
                .Transform3x4 = 0,
                .IndexFormat = DXGI_FORMAT_R32_UINT,
                .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
                .IndexCount = static_cast<uint32_t>(m_Indices.size()),
                .VertexCount = static_cast<uint32_t>(m_Vertices.size()),
                .IndexBuffer = m_IndexBuffer->GetGPUVirtualAddress(),
                .VertexBuffer
                {
                    .StartAddress = m_VertexBuffer->GetGPUVirtualAddress(),
                    .StrideInBytes = sizeof(DirectX::XMFLOAT3),
                },
            }
        };

        // Update it later in function
        const auto instanceBuffer = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "InstanceBuffer",
            .ElementSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
            .ElementCount = 1,
            .Flags = benzin::BufferFlag::AllowUnorderedAccess,
            .IsNeedUnorderedAccessView = true,
        });

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS d3d12TopLevelInputs
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
            .NumDescs = 1,
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .InstanceDescs = instanceBuffer->GetGPUVirtualAddress(),
        };

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO d3d12TopLevelPrebuildInfo{};
        m_Device.GetD3D12Device()->GetRaytracingAccelerationStructurePrebuildInfo(&d3d12TopLevelInputs, &d3d12TopLevelPrebuildInfo);
        BenzinEnsure(d3d12TopLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS d3d12BottomLevelInputs
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
            .NumDescs = 1,
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .pGeometryDescs = &d3d12RaytracingGeometryDesc,
        };

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO d3d12BottomLevelPrebuildInfo{};
        m_Device.GetD3D12Device()->GetRaytracingAccelerationStructurePrebuildInfo(&d3d12BottomLevelInputs, &d3d12BottomLevelPrebuildInfo);
        BenzinEnsure(d3d12BottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);
    
        const auto scratchResource = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "ScratchResource",
            .ElementCount = static_cast<uint32_t>(std::max(d3d12TopLevelPrebuildInfo.ScratchDataSizeInBytes, d3d12BottomLevelPrebuildInfo.ScratchDataSizeInBytes)),
            .Flags = benzin::BufferFlag::AllowUnorderedAccess,
            .IsNeedUnorderedAccessView = true,
        });

        m_Device.GetGraphicsCommandQueue().GetCommandList().SetResourceBarrier(*scratchResource, benzin::ResourceState::UnorderedAccess);

        {
            m_TLAS = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation
            {
                .DebugName = "TopLevel AS",
                .ElementCount = static_cast<uint32_t>(d3d12TopLevelPrebuildInfo.ResultDataMaxSizeInBytes),
                .Flags = benzin::BufferFlag::AllowUnorderedAccess,
                .InitialState = benzin::ResourceState::RaytracingAccelerationStructure,
                .IsNeedShaderResourceView = true,
                .IsNeedUnorderedAccessView = true,
            });

            m_BLAS = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation
            {
                .DebugName = "BottomLevel AS",
                .ElementCount = static_cast<uint32_t>(d3d12BottomLevelPrebuildInfo.ResultDataMaxSizeInBytes),
                .Flags = benzin::BufferFlag::AllowUnorderedAccess,
                .InitialState = benzin::ResourceState::RaytracingAccelerationStructure,
                .IsNeedUnorderedAccessView = true,
            });
        }

        {
            const D3D12_RAYTRACING_INSTANCE_DESC d3d12RaytracingInstanceDesc
            {
                .Transform
                {
                    { 1.0f, 0.0f, 0.0f, 0.0f },
                    { 0.0f, 1.0f, 0.0f, 0.0f },
                    { 0.0f, 0.0f, 1.0f, 0.0f },
                },
                .InstanceMask = 1,
                .Flags = 0,
                .AccelerationStructure = m_BLAS->GetGPUVirtualAddress(),
            };

            auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
            BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

            auto& copyCommandList = copyCommandQueue.GetCommandList(sizeof(d3d12RaytracingInstanceDesc));

            copyCommandList.UpdateBuffer(*instanceBuffer, std::span{ &d3d12RaytracingInstanceDesc, 1 });
        }

        // Bottom Level Acceleration Structure desc
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC d3d12BLASDesc
        {
            .DestAccelerationStructureData = m_BLAS->GetGPUVirtualAddress(),
            .Inputs = d3d12BottomLevelInputs,
            .SourceAccelerationStructureData = 0,
            .ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress(),
        };

        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC d3d12TLASDesc
        {
            .DestAccelerationStructureData = m_TLAS->GetGPUVirtualAddress(),
            .Inputs = d3d12TopLevelInputs,
            .SourceAccelerationStructureData = 0,
            .ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress(),
        };

        const D3D12_RESOURCE_BARRIER d3d12ResourceBarrier
        {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .UAV
            {
                .pResource = m_BLAS->GetD3D12Resource(),
            },
        };

        ID3D12GraphicsCommandList4* d3d12CommandList = m_Device.GetGraphicsCommandQueue().GetCommandList().GetD3D12GraphicsCommandList();
        d3d12CommandList->BuildRaytracingAccelerationStructure(&d3d12BLASDesc, 0, nullptr);
        d3d12CommandList->ResourceBarrier(1, &d3d12ResourceBarrier);
        d3d12CommandList->BuildRaytracingAccelerationStructure(&d3d12TLASDesc, 0, nullptr);

        {
            m_Device.GetGraphicsCommandQueue().ExecuteCommandList();
            m_Device.GetGraphicsCommandQueue().Flush();
        }
    }

    void RaytracingLayer::CreateShaderTables()
    {
        ComPtr<ID3D12StateObjectProperties> d3d12StateObjectProperties;
        BenzinAssert(m_D3D12RaytracingStateObject.As(&d3d12StateObjectProperties));

        auto rayGenShaderIdentifier = reinterpret_cast<const std::byte*>(d3d12StateObjectProperties->GetShaderIdentifier(g_RayGenShaderName.data()));
        auto missShaderIdentifier = reinterpret_cast<const std::byte*>(d3d12StateObjectProperties->GetShaderIdentifier(g_MissShaderName.data()));
        auto hitGroupShaderIdentifier = reinterpret_cast<const std::byte*>(d3d12StateObjectProperties->GetShaderIdentifier(g_HitGroupShaderName.data()));

        const uint32_t shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

        // m_RayGenShaderTable
        {
            const uint32_t shaderRecordCount = 1;
            const uint32_t shaderRecordSize = shaderIdentifierSize + sizeof(m_RayGenConstants);

            const uint32_t alignedShaderRecordSize = benzin::AlignAbove(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

            m_RayGenShaderTable = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation
            {
                .DebugName = "RayGenShaderTable",
                .ElementCount = alignedShaderRecordSize,
                .Flags = benzin::BufferFlag::UploadBuffer,
            });

            benzin::MappedData rayGenShaderTable{ *m_RayGenShaderTable };
            rayGenShaderTable.Write(std::span{ rayGenShaderIdentifier, shaderIdentifierSize });
            rayGenShaderTable.WriteBytes(m_RayGenConstants, shaderIdentifierSize);
        }

        // m_MissShaderTable
        {
            const uint32_t shaderRecordCount = 1;
            const uint32_t shaderRecordSize = shaderIdentifierSize;

            const uint32_t alignedShaderRecordSize = benzin::AlignAbove(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

            m_MissShaderTable = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation
            {
                .DebugName = "MissShaderTable",
                .ElementCount = alignedShaderRecordSize,
                .Flags = benzin::BufferFlag::UploadBuffer,
                .InitialData = std::span{ missShaderIdentifier, shaderIdentifierSize },
            });
        }

        // m_HitGroupShaderTable
        {
            const uint32_t shaderRecordCount = 1;
            const uint32_t shaderRecordSize = shaderIdentifierSize;

            const uint32_t alignedShaderRecordSize = benzin::AlignAbove<uint32_t>(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

            m_HitGroupShaderTable = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation
            {
                .DebugName = "HitGroupShaderTable",
                .ElementCount = alignedShaderRecordSize,
                .Flags = benzin::BufferFlag::UploadBuffer,
                .InitialData = std::span{ hitGroupShaderIdentifier, shaderIdentifierSize },
            });
        }
    }

    void RaytracingLayer::CreateRaytracingResources()
    {
        m_RaytracingOutput = std::make_shared<benzin::Texture>(m_Device, benzin::TextureCreation
        {
            .DebugName = "RayTracingOutput",
            .Type = benzin::TextureType::Texture2D,
            .Format = benzin::config::g_BackBufferFormat,
            .Width = static_cast<uint32_t>(m_SwapChain.GetViewport().Width),
            .Height = static_cast<uint32_t>(m_SwapChain.GetViewport().Height),
            .MipCount = 1,
            .Flags = benzin::TextureFlag::AllowUnorderedAccess,
            .IsNeedUnorderedAccessView = true,
        });
    }

} // namespace sandbox
