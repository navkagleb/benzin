#include "bootstrap.hpp"
#include "rt_hello_triangle_layer.hpp"

#include <benzin/core/command_line_args.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/shaders.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>

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
            _BuildBottomLevelAS,
            _BuildTopLevelAS, 
            _DispatchRays,
        };

    } // anonymous namespace

    RTHelloTriangleLayer::RTHelloTriangleLayer(const benzin::GraphicsRefs& graphicsRefs)
        : m_Window{ graphicsRefs.WindowRef }
        , m_Device{ graphicsRefs.DeviceRef }
        , m_SwapChain{ graphicsRefs.SwapChainRef }
        , m_RayGenConstants
        {
            .Viewport{ -1.0f, -1.0f, 1.0f, 1.0f },
            .Stencil
            {
                -1.0f + 0.1f / m_SwapChain.GetAspectRatio(), -1.0f + 0.1f,
                1.0f - 0.1f / m_SwapChain.GetAspectRatio(), 1.0f - 0.1f,
            },
        }
    {
        m_GPUTimer = std::make_shared<benzin::GPUTimer>(m_Device, benzin::GPUTimerCreation
        {
            .CommandList = m_Device.GetGraphicsCommandQueue().GetCommandList(),
            .TimestampFrequency = m_Device.GetGraphicsCommandQueue().GetTimestampFrequency(),
            .TimerCount = (uint32_t)magic_enum::enum_count<GPUTimerIndex>(),
        });

        CreateEntities();

        CreateRaytracingStateObject();
        CreateAccelerationStructures();
        CreateShaderTables();
        CreateRaytracingResources();
    }

    void RTHelloTriangleLayer::OnRender()
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        auto* d3d12Device = m_Device.GetD3D12Device();
        auto* d3d12CommandList = commandList.GetD3D12GraphicsCommandList();

        {
            enum RootConstant : uint32_t
            {
                RootConstant_TopLevelASIndex,
                RootConstant_RayGenConstantBufferIndex,
                RootConstant_RaytracingOutputTextureIndex,
            };

            commandList.SetRootResource(RootConstant_TopLevelASIndex, m_TLAS->GetShaderResourceView());
            commandList.SetRootResource(RootConstant_RayGenConstantBufferIndex, m_RayGenConstantBuffer->GetConstantBufferView());
            commandList.SetRootResource(RootConstant_RaytracingOutputTextureIndex, m_RaytracingOutput->GetUnorderedAccessView());

            d3d12CommandList->SetPipelineState1(m_D3D12RaytracingStateObject.Get());
        }
        
        {
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
                    .StrideInBytes = m_MissShaderTable->GetNotAlignedSizeInBytes(),
                },
                .HitGroupTable
                {
                    .StartAddress = m_HitGroupShaderTable->GetGPUVirtualAddress(),
                    .SizeInBytes = m_HitGroupShaderTable->GetNotAlignedSizeInBytes(),
                    .StrideInBytes = m_HitGroupShaderTable->GetNotAlignedSizeInBytes(),
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
                BenzinGrabGPUTimeOnScopeExit(*m_GPUTimer, magic_enum::enum_integer(GPUTimerIndex::_DispatchRays));
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

            commandList.CopyResource(currentBackBuffer, *m_RaytracingOutput);

            commandList.SetResourceBarriers(
            {
                benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::Common },
                benzin::TransitionBarrier{ *m_RaytracingOutput, benzin::ResourceState::UnorderedAccess },
            });
        }

        m_GPUTimer->ResolveTimestamps();
    }

    void RTHelloTriangleLayer::OnImGuiRender()
    {
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

    void RTHelloTriangleLayer::CreateEntities()
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

        m_VertexBuffer = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation{ .ElementSize = vertexSize, .ElementCount = (uint32_t)m_Vertices.size() });
        m_IndexBuffer = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation{ .ElementSize = indexSize, .ElementCount = (uint32_t)m_Indices.size() });

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(m_VertexBuffer->GetSizeInBytes() + m_IndexBuffer->GetSizeInBytes());

        copyCommandList.UpdateBuffer(*m_VertexBuffer, std::span<const DirectX::XMFLOAT3>{ m_Vertices });
        copyCommandList.UpdateBuffer(*m_IndexBuffer, std::span<const uint32_t>{ m_Indices });
    }

    void RTHelloTriangleLayer::CreateRaytracingStateObject()
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

        // 1. D3D12_GLOBAL_ROOT_SIGNATURE
        const D3D12_GLOBAL_ROOT_SIGNATURE d3d12GlobalRootSignature
        {
            .pGlobalRootSignature = m_Device.GetD3D12BindlessRootSignature(),
        };

        // 2. D3D12_DXIL_LIBRARY_DESC
        const std::span<const std::byte> libraryBinary = benzin::GetShaderBinary(benzin::ShaderType::Library, { "raytracing_hello_triangle.hlsl" });

#if 0 // Can use default 'D3D12_DXIL_LIBRARY_DESC::NumExports'
        std::vector<D3D12_EXPORT_DESC> d3d12ExportDescs
        {
            D3D12_EXPORT_DESC{ .Name = g_RayGenShaderName.data(), .ExportToRename = nullptr, .Flags = D3D12_EXPORT_FLAG_NONE },
            D3D12_EXPORT_DESC{ .Name = g_ClosestHitShaderName.data(), .ExportToRename = nullptr, .Flags = D3D12_EXPORT_FLAG_NONE },
            D3D12_EXPORT_DESC{ .Name = g_MissShaderName.data(), .ExportToRename = nullptr, .Flags = D3D12_EXPORT_FLAG_NONE },
        };
#endif

        const D3D12_DXIL_LIBRARY_DESC d3d12DXILLibraryDesc
        {
            .DXILLibrary
            {
                .pShaderBytecode = libraryBinary.data(),
                .BytecodeLength = libraryBinary.size(),
            },
#if 0 // Can use default 'D3D12_DXIL_LIBRARY_DESC::NumExports'
            .NumExports = (uint32_t)d3d12ExportDescs.size(),
            .pExports = d3d12ExportDescs.data(),
#endif
        };

        // 3. D3D12_HIT_GROUP_DESC
        const D3D12_HIT_GROUP_DESC d3d12HitGroupDesc
        {
            .HitGroupExport = g_HitGroupShaderName.data(),
            .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES,
            .AnyHitShaderImport = nullptr,
            .ClosestHitShaderImport = g_ClosestHitShaderName.data(),
            .IntersectionShaderImport = nullptr,
        };

        // 4. D3D12_RAYTRACING_SHADER_CONFIG
        const D3D12_RAYTRACING_SHADER_CONFIG d3d12RaytracingShaderConfig
        {
            .MaxPayloadSizeInBytes = sizeof(DirectX::XMFLOAT4), // Color
            .MaxAttributeSizeInBytes = sizeof(DirectX::XMFLOAT2), // Barycentrics
        };

        // 5. D3D12_RAYTRACING_PIPELINE_CONFIG
        const D3D12_RAYTRACING_PIPELINE_CONFIG d3d12RaytracingPipelineConfig
        {
            .MaxTraceRecursionDepth = 1,
        };

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

    void RTHelloTriangleLayer::CreateAccelerationStructures()
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
                .IndexCount = (uint32_t)m_Indices.size(),
                .VertexCount = (uint32_t)m_Vertices.size(),
                .IndexBuffer = m_IndexBuffer->GetGPUVirtualAddress(),
                .VertexBuffer
                {
                    .StartAddress = m_VertexBuffer->GetGPUVirtualAddress(),
                    .StrideInBytes = sizeof(DirectX::XMFLOAT3),
                },
            },
        };

        // Create BottomLevel AS
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

        m_BLAS = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "BottomLevel AS",
            .ElementCount = (uint32_t)d3d12BottomLevelPrebuildInfo.ResultDataMaxSizeInBytes,
            .Flags = benzin::BufferFlag::AllowUnorderedAccess,
            .InitialState = benzin::ResourceState::RaytracingAccelerationStructure,
        });

        // Create TopLevel AS
        const D3D12_RAYTRACING_INSTANCE_DESC d3d12RaytracingInstanceDesc
        {
            .Transform
            {
                { 1.0f, 0.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f, 0.0f },
            },
            .InstanceID = 0, // 24 bit
            .InstanceMask = 1, // 8 bit
            .InstanceContributionToHitGroupIndex = 0, // 24 bit
            .Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE, // 8 bit
            .AccelerationStructure = m_BLAS->GetGPUVirtualAddress(),
        };

        const auto instanceBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "InstanceBuffer",
            .ElementSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
            .ElementCount = 1,
            .Flags = benzin::BufferFlag::UploadBuffer,
            .InitialData = std::as_bytes(std::span{ &d3d12RaytracingInstanceDesc, 1 }),
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

        m_TLAS = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "TopLevel AS",
            .ElementCount = (uint32_t)d3d12TopLevelPrebuildInfo.ResultDataMaxSizeInBytes,
            .Flags = benzin::BufferFlag::AllowUnorderedAccess,
            .InitialState = benzin::ResourceState::RaytracingAccelerationStructure,
            // .IsNeedStructuredBufferView = true, // This leads to 'DXGI_ERROR_DEVICE_HUNG' !!!
            .IsNeedRaytracingAccelerationStructureView = true,
        });

        // Create scratch resource for build BottomLevel and TopLevel ASs
        const auto scratchResource = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "ScratchResource",
            .ElementCount = (uint32_t)std::max(d3d12BottomLevelPrebuildInfo.ScratchDataSizeInBytes, d3d12TopLevelPrebuildInfo.ScratchDataSizeInBytes),
            .Flags = benzin::BufferFlag::AllowUnorderedAccess,
        });

        // Place 'graphicsCommandQueue' here to release 'sctrachResource' after 'graphicsCommandQueue' will be flushed
        auto& graphicsCommandQueue = m_Device.GetGraphicsCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(graphicsCommandQueue);

        auto& commandList = graphicsCommandQueue.GetCommandList();
        ID3D12GraphicsCommandList4* d3d12CommandList = commandList.GetD3D12GraphicsCommandList();

        commandList.SetResourceBarriers({ benzin::TransitionBarrier{ *scratchResource, benzin::ResourceState::UnorderedAccess } });

        // Build BottomLevel AS
        {
            BenzinGrabGPUTimeOnScopeExit(*m_GPUTimer, magic_enum::enum_integer(GPUTimerIndex::_BuildBottomLevelAS));

            const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC d3d12BLASDesc
            {
                .DestAccelerationStructureData = m_BLAS->GetGPUVirtualAddress(),
                .Inputs = d3d12BottomLevelInputs,
                .SourceAccelerationStructureData = 0,
                .ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress(),
            };
            d3d12CommandList->BuildRaytracingAccelerationStructure(&d3d12BLASDesc, 0, nullptr);

            commandList.SetResourceBarrier(benzin::UnorderedAccessBarrier{ *m_BLAS });
        }

        // Build TopLevel AS
        {
            BenzinGrabGPUTimeOnScopeExit(*m_GPUTimer, magic_enum::enum_integer(GPUTimerIndex::_BuildTopLevelAS));

            const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC d3d12TLASDesc
            {
                .DestAccelerationStructureData = m_TLAS->GetGPUVirtualAddress(),
                .Inputs = d3d12TopLevelInputs,
                .SourceAccelerationStructureData = 0,
                .ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress(),
            };

            d3d12CommandList->BuildRaytracingAccelerationStructure(&d3d12TLASDesc, 0, nullptr);
        }
    }

    void RTHelloTriangleLayer::CreateShaderTables()
    {
        BenzinAssert(m_D3D12RaytracingStateObject.Get());

        ComPtr<ID3D12StateObjectProperties> d3d12StateObjectProperties;
        BenzinAssert(m_D3D12RaytracingStateObject.As(&d3d12StateObjectProperties));

        const auto CreateShaderTable = [&](std::wstring_view shaderName)
        {
            const auto shaderIdentifier = std::span{ (const std::byte*)d3d12StateObjectProperties->GetShaderIdentifier(shaderName.data()), benzin::config::g_ShaderIdentifierSizeInBytes };

            return std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation
            {
                .DebugName = std::format("{}ShaderTable", benzin::ToNarrowString(shaderName)),
                .ElementCount = benzin::AlignAbove((uint32_t)shaderIdentifier.size(), benzin::config::g_RayTracingShaderRecordAlignment),
                .Flags = benzin::BufferFlag::UploadBuffer,
                .InitialData = shaderIdentifier,
            });
        };

        m_RayGenShaderTable = CreateShaderTable(g_RayGenShaderName);
        m_MissShaderTable = CreateShaderTable(g_MissShaderName);
        m_HitGroupShaderTable = CreateShaderTable(g_HitGroupShaderName);
    }

    void RTHelloTriangleLayer::CreateRaytracingResources()
    {
        m_RayGenConstantBuffer = std::make_shared<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "RayGenConstantBuffer",
            .ElementSize = sizeof(m_RayGenConstants),
            .ElementCount = 1,
            .Flags = benzin::BufferFlag::ConstantBuffer,
            .InitialData = std::as_bytes(std::span{ &m_RayGenConstants, 1 }),
            .IsNeedConstantBufferView = true,
        });

        m_RaytracingOutput = std::make_shared<benzin::Texture>(m_Device, benzin::TextureCreation
        {
            .DebugName = "RayTracingOutput",
            .Type = benzin::TextureType::Texture2D,
            .Format = benzin::CommandLineArgs::GetBackBufferFormat(),
            .Width = (uint32_t)m_SwapChain.GetViewport().Width,
            .Height = (uint32_t)m_SwapChain.GetViewport().Height,
            .MipCount = 1,
            .Flags = benzin::TextureFlag::AllowUnorderedAccess,
            .IsNeedUnorderedAccessView = true,
        });
    }

} // namespace sandbox
