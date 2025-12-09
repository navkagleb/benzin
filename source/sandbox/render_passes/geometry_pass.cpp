#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/geometry_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/pso.hpp>
#include <benzin/graphics/query_heap.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/geometry_resources.hpp>
#include <shaders/joint/mesh_types.hpp>

BenzinAllowDereferenceOperatorForEnum(joint::GeometryResources);
BenzinAllowDereferenceOperatorForEnum(joint::GeometryCullingResources);

namespace sandbox
{

    // GeometryPass

    GeometryPass::GeometryPass()
    {
        const auto createComputePso = [](PsoId id)
        {
            ms_PsoManager->Create(id, [id](benzin::ComputePsoProxy& proxy)
            {
                proxy.m_Cs.m_FileName = "geometry_culling.hlsl";

                if (id == PsoId::GeometryPass_LateComputeCulling)
                {
                    proxy.m_Cs.m_Defines.push_back("LATE_CULLING");
                }
            });
        };

        createComputePso(PsoId::GeometryPass_EarlyComputeCulling);
        createComputePso(PsoId::GeometryPass_LateComputeCulling);

        CreateGeometryPso(PsoId::GeometryPass_Vertex, false);
        CreateGeometryPso(PsoId::GeometryPass_Mesh, true);

        {
            std::array<D3D12_INDIRECT_ARGUMENT_DESC, 2> d3d12ArgumentDescs = {};

            d3d12ArgumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            d3d12ArgumentDescs[0].Constant.RootParameterIndex = *benzin::UnifiedRootParameter::Root32Consts;
            d3d12ArgumentDescs[0].Constant.DestOffsetIn32BitValues = *joint::GeometryResources::MeshDrawIndex;
            d3d12ArgumentDescs[0].Constant.Num32BitValuesToSet = 1;

            d3d12ArgumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

            D3D12_COMMAND_SIGNATURE_DESC d3d12CmdSignatureDesc = {};
            d3d12CmdSignatureDesc.ByteStride = sizeof(joint::MeshDrawCmd);
            d3d12CmdSignatureDesc.NumArgumentDescs = (uint32_t)d3d12ArgumentDescs.size();
            d3d12CmdSignatureDesc.pArgumentDescs = d3d12ArgumentDescs.data();
            d3d12CmdSignatureDesc.NodeMask = 0;

            BenzinD3D12Call(ms_Device->GetD3D12Device()->CreateCommandSignature(
                &d3d12CmdSignatureDesc,
                ms_Device->GetUnifiedRootSignature().GetD3D12RootSignature(),
                IID_PPV_ARGS(&m_D3D12DrawCmdSignature)));
        }

        {
            std::array<D3D12_INDIRECT_ARGUMENT_DESC, 4> d3d12ArgumentDescs = {};

            d3d12ArgumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            d3d12ArgumentDescs[0].Constant.RootParameterIndex = *benzin::UnifiedRootParameter::Root32Consts;
            d3d12ArgumentDescs[0].Constant.DestOffsetIn32BitValues = *joint::GeometryResources::MeshDrawIndex;
            d3d12ArgumentDescs[0].Constant.Num32BitValuesToSet = 1;

            d3d12ArgumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            d3d12ArgumentDescs[1].Constant.RootParameterIndex = *benzin::UnifiedRootParameter::Root32Consts;
            d3d12ArgumentDescs[1].Constant.DestOffsetIn32BitValues = *joint::GeometryResources::MeshletOffset;
            d3d12ArgumentDescs[1].Constant.Num32BitValuesToSet = 1;

            d3d12ArgumentDescs[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            d3d12ArgumentDescs[2].Constant.RootParameterIndex = *benzin::UnifiedRootParameter::Root32Consts;
            d3d12ArgumentDescs[2].Constant.DestOffsetIn32BitValues = *joint::GeometryResources::MeshletCount;
            d3d12ArgumentDescs[2].Constant.Num32BitValuesToSet = 1;

            d3d12ArgumentDescs[3].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;

            D3D12_COMMAND_SIGNATURE_DESC d3d12CmdSignatureDesc = {};
            d3d12CmdSignatureDesc.ByteStride = sizeof(joint::MeshDrawCmd);
            d3d12CmdSignatureDesc.NumArgumentDescs = (uint32_t)d3d12ArgumentDescs.size();
            d3d12CmdSignatureDesc.pArgumentDescs = d3d12ArgumentDescs.data();
            d3d12CmdSignatureDesc.NodeMask = 0;

            BenzinD3D12Call(ms_Device->GetD3D12Device()->CreateCommandSignature(
                &d3d12CmdSignatureDesc,
                ms_Device->GetUnifiedRootSignature().GetD3D12RootSignature(),
                IID_PPV_ARGS(&m_D3D12MeshDispatchCmdSignature)));
        }
    }

    GeometryPass::~GeometryPass()
    {
        ms_Device->DeferredRelease(m_D3D12DrawCmdSignature);
        m_D3D12DrawCmdSignature = nullptr;

        ms_Device->DeferredRelease(m_D3D12MeshDispatchCmdSignature);
        m_D3D12MeshDispatchCmdSignature = nullptr;

        ms_PsoManager->Destroy(PsoId::GeometryPass_EarlyComputeCulling);
        ms_PsoManager->Destroy(PsoId::GeometryPass_LateComputeCulling);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Vertex);
        ms_PsoManager->Destroy(PsoId::GeometryPass_Mesh);

        ms_Resources->Destroy(TextureId::AlbedoAndRoughness);
        ms_Resources->Destroy(TextureId::EmissiveAndMetallic);
        ms_Resources->Destroy(TextureId::WorldNormal);
        ms_Resources->Destroy(TextureId::Mv);
        ms_Resources->Destroy(TextureId::ViewDepth);
        ms_Resources->Destroy(TextureId::DepthStencil);
    }

    void GeometryPass::OnZeroFrameInit()
    {
        const uint32_t drawCount = (uint32_t)ms_Scene->m_JointMeshDraws.size();
        BenzinAssert(drawCount != 0);

        benzin::GpuHeapLinearAllocator& allocator = ms_Device->GetPersistentDefaultAllocator();

        m_VisibilityBuffer = allocator.AllocateBuffer([drawCount](benzin::BufferCreation& creation)
        {
            creation.m_DebugName = "GeometryPass::VisibilityBuffer";
            creation.m_Type = benzin::BufferType::Format;
            creation.m_DxgiFormat = DXGI_FORMAT_R8_UINT;
            creation.m_ElementSizeInBytes = sizeof(uint8_t);
            creation.m_ElementCount = drawCount;
            creation.m_IsUnorderedAccessAllowed = true;
        });

        m_CmdCountBuffer = allocator.AllocateBuffer([](benzin::BufferCreation& creation)
        {
            creation.m_DebugName = "GeometryPass::CmdCountBuffer";
            creation.m_Type = benzin::BufferType::Format;
            creation.m_DxgiFormat = DXGI_FORMAT_R32_UINT;
            creation.m_ElementSizeInBytes = sizeof(uint32_t);
            creation.m_ElementCount = 1;
            creation.m_IsUnorderedAccessAllowed = true;
        });

        m_DrawCmdBuffer = allocator.AllocateBuffer([drawCount](benzin::BufferCreation& creation)
        {
            creation.m_DebugName = "GeometryPass::DrawCmdBuffer";
            creation.m_Type = benzin::BufferType::Structured;
            creation.m_ElementSizeInBytes = sizeof(joint::MeshDrawCmd);
            creation.m_ElementCount = drawCount;
            creation.m_IsUnorderedAccessAllowed = true;
        });

        m_DispatchCmdBuffer = allocator.AllocateBuffer([drawCount](benzin::BufferCreation& creation)
        {
            creation.m_DebugName = "GeometryPass::DispatchCmdBuffer";
            creation.m_Type = benzin::BufferType::Structured;
            creation.m_ElementSizeInBytes = sizeof(joint::MeshDispatchCmd);
            creation.m_ElementCount = drawCount;
            creation.m_IsUnorderedAccessAllowed = true;
        });

        benzin::QueryHeapCreation queryHeapCreation;
        queryHeapCreation.m_DebugName = "GeometryPass::StatsQueryHeap";
        queryHeapCreation.m_D3D12Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS1;
        queryHeapCreation.m_Count = 1;
        MakeUniquePtr(m_StatsQueryHeap, *ms_Device, queryHeapCreation);

        m_StatsBuffer = ms_Device->GetPersistentReadbackAllocator().AllocateStructuredBuffer(
            "GeometryPass::StatsBuffer",
            BENZIN_READBACK_LATENCY,
            sizeof(m_D3D12PipelineStats));

        benzin::ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        cmdList.AddTransition(*m_VisibilityBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList.FlushBarriers();
        cmdList.ClearUnorderedAccess(*m_VisibilityBuffer, m_VisibilityBuffer->GetUav());
    }

    void GeometryPass::OnRenderViewportResize()
    {
        ms_Resources->Create(TextureId::AlbedoAndRoughness, GBufferSettings::ms_Color0DxgiFormat, benzin::TextureAccessFlag::AllowRenderTarget);
        ms_Resources->Create(TextureId::EmissiveAndMetallic, GBufferSettings::ms_Color1DxgiFormat, benzin::TextureAccessFlag::AllowRenderTarget);
        ms_Resources->Create(TextureId::WorldNormal, GBufferSettings::ms_Color2DxgiFormat, benzin::TextureAccessFlag::AllowRenderTarget);
        ms_Resources->Create(TextureId::Mv, GBufferSettings::ms_Color3DxgiFormat, benzin::TextureAccessFlag::AllowRenderTarget);
        ms_Resources->Create(TextureId::ViewDepth, GBufferSettings::ms_Color4DxgiFormat, benzin::TextureAccessFlag::AllowRenderTarget);
        ms_Resources->Create(TextureId::DepthStencil, GBufferSettings::ms_DepthStencilDxgiFormat, benzin::TextureAccessFlag::AllowDepthStencil);
    }

    void GeometryPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("Geometry");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        ID3D12GraphicsCommandList* d3d12CmdList = cmdList.GetD3D12GraphicsCommandList();

        d3d12CmdList->BeginQuery(m_StatsQueryHeap->GetD3D12QueryHeap(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS1, 0);

        RunCullingPass("EarlyCulling", false);
        RunDrawPass("EarlyDraw", false);
        RunCullingPass("LateCulling", true);
        RunDrawPass("LateDraw", true);

        d3d12CmdList->EndQuery(m_StatsQueryHeap->GetD3D12QueryHeap(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS1, 0);

        cmdList.AddTransition(*m_StatsBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
        cmdList.FlushBarriers();

        d3d12CmdList->ResolveQueryData(
            m_StatsQueryHeap->GetD3D12QueryHeap(),
            D3D12_QUERY_TYPE_PIPELINE_STATISTICS1,
            0,
            1,
            m_StatsBuffer->GetD3D12Resource(),
            ms_Device->GetReadbackWriteIndex() * sizeof(m_D3D12PipelineStats));

        cmdList.AddTransition(*m_StatsBuffer, D3D12_RESOURCE_STATE_COMMON);
        cmdList.FlushBarriers();

        m_StatsBuffer->MapReadbackData<D3D12_QUERY_DATA_PIPELINE_STATISTICS1>(
            ms_Device->GetReadbackReadIndex(),
            1,
            [this](std::span<const D3D12_QUERY_DATA_PIPELINE_STATISTICS1> stats)
            {
                m_D3D12PipelineStats = stats.front();
            });

        ms_Settings->GetSection<GBufferStats>().m_D3D12PipelineStats = m_D3D12PipelineStats;
    }

    void GeometryPass::CreateGeometryPso(PsoId id, bool isMeshPipeline)
    {
        const auto configure = [](auto& proxy)
        {
            proxy.m_Ps.m_FileName = "geometry_pass.hlsl";
            proxy.m_DepthStencilDxgiFormat = GBufferSettings::ms_DepthStencilDxgiFormat;
            proxy.m_RenderTargetDxgiFormats.reserve(5);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color0DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color1DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color2DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color3DxgiFormat);
            proxy.m_RenderTargetDxgiFormats.push_back(GBufferSettings::ms_Color4DxgiFormat);

            proxy.m_DepthState.m_IsEnabled = true;
            proxy.m_DepthState.m_IsWriteEnabled = true;
            proxy.m_DepthState.m_D3D12ComparisonFunction = D3D12_COMPARISON_FUNC_GREATER;
        };

        if (isMeshPipeline)
        {
            ms_PsoManager->Create(id, [&configure](benzin::MeshPsoProxy& proxy)
            {
                // proxy.m_As.m_FileName = "geometry_pass.hlsl";
                // proxy.m_As.m_Defines.push_back("MESH_PIPELINE");

                proxy.m_Ms.m_FileName = "geometry_pass.hlsl";
                proxy.m_Ms.m_Defines.push_back("MESH_PIPELINE");

                configure(proxy);
            });
        }
        else
        {
            ms_PsoManager->Create(id, [&configure](benzin::VertexPsoProxy& proxy)
            {
                proxy.m_InputLayout.emplace_back("sem_Position", DXGI_FORMAT_R32G32B32_FLOAT);
                proxy.m_InputLayout.emplace_back("sem_Normal", DXGI_FORMAT_R32G32B32_FLOAT);
                proxy.m_InputLayout.emplace_back("sem_Uv", DXGI_FORMAT_R32G32_FLOAT);

                BenzinAssert(benzin::GetDxgiFormatSizeInBytes(proxy.m_InputLayout[0].m_DxgiFormat) == sizeof(joint::MeshVertex::m_Position));
                BenzinAssert(benzin::GetDxgiFormatSizeInBytes(proxy.m_InputLayout[1].m_DxgiFormat) == sizeof(joint::MeshVertex::m_Normal));
                BenzinAssert(benzin::GetDxgiFormatSizeInBytes(proxy.m_InputLayout[2].m_DxgiFormat) == sizeof(joint::MeshVertex::m_Uv));

                proxy.m_Vs.m_FileName = "geometry_pass.hlsl";

                configure(proxy);
            });
        }
    }

    void GeometryPass::RunCullingPass(const char* gpuName, bool isLate) const
    {
        using Resources = joint::GeometryCullingResources;

        BenzinProfile();
        BenzinGpuProfile(gpuName);

        benzin::ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const uint32_t drawCount = (uint32_t)ms_Scene->m_JointMeshDraws.size();

        cmdList.SetComputeRootConstant(*Resources::MeshDrawCount, drawCount);
        cmdList.SetComputeRootSrv(*Resources::MeshDraws, *ms_Scene->m_MeshDrawBuffer);
        cmdList.SetComputeRootSrv(*Resources::Meshes, *ms_Scene->m_MeshBuffer);
        cmdList.SetComputeRootUav(*Resources::MeshCmdCounter, *m_CmdCountBuffer);
        cmdList.SetComputeRootUav(*Resources::MeshDrawCmds, *m_DrawCmdBuffer);
        cmdList.SetComputeRootUav(*Resources::MeshDispatchCmds, *m_DispatchCmdBuffer);

        if (isLate)
        {
            cmdList.SetComputeRootUav(*Resources::VisibilityBuffer, *m_VisibilityBuffer);
        }
        else
        {
            cmdList.SetComputeRootSrv(*Resources::VisibilityBuffer, *m_VisibilityBuffer);
        }

        cmdList.FlushBarriers();

        cmdList.ClearUnorderedAccess(*m_CmdCountBuffer, m_CmdCountBuffer->GetUav());

        cmdList.SetComputePso(ms_PsoManager->GetCompute(isLate ? PsoId::GeometryPass_LateComputeCulling : PsoId::GeometryPass_EarlyComputeCulling));
        cmdList.Dispatch({ drawCount, 1, 1 }, { 64, 1, 1 });

        cmdList.AddUnorderedAccess(*m_CmdCountBuffer);
        cmdList.AddUnorderedAccess(*m_DrawCmdBuffer);
        cmdList.AddUnorderedAccess(*m_DispatchCmdBuffer);
    }

    void GeometryPass::RunDrawPass(const char* gpuName, bool isLate) const
    {
        BenzinProfile();
        BenzinGpuProfile(gpuName);

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const bool isMeshPipeline = ms_Settings->GetSection<GBufferSettings>().m_IsMeshPipelineEnabled;

        if (isMeshPipeline)
        {
            cmdList.SetMeshPso(ms_PsoManager->GetMesh(PsoId::GeometryPass_Mesh));

            cmdList.SetGraphicsRootSrv(*joint::GeometryResources::Vertices, *ms_Scene->m_VertexBuffer), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            cmdList.SetGraphicsRootSrv(*joint::GeometryResources::Meshlets, *ms_Scene->m_MeshletBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.SetGraphicsRootSrv(*joint::GeometryResources::MeshletCullVolumes, *ms_Scene->m_MeshletCullVolumeBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.SetGraphicsRootSrv(*joint::GeometryResources::MeshletVertexIndices, *ms_Scene->m_MeshletVertexIndexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.SetGraphicsRootSrv(*joint::GeometryResources::MeshletIndices, *ms_Scene->m_MeshletIndexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
        else
        {
            cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::GeometryPass_Vertex));

            cmdList.SetVertexBuffer(*ms_Scene->m_VertexBuffer);
            cmdList.SetIndexBuffer(*ms_Scene->m_IndexBuffer);

            cmdList.GetD3D12GraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }

        cmdList.SetGraphicsRootSrv(*joint::GeometryResources::MeshDraws, *ms_Scene->m_MeshDrawBuffer);
        cmdList.SetGraphicsRootSrv(*joint::GeometryResources::Materials, *ms_Scene->m_MaterialBuffer);

        const auto& cmdBuffer = isMeshPipeline ? m_DispatchCmdBuffer : m_DrawCmdBuffer;

        cmdList.AddTransition(*m_CmdCountBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        cmdList.AddTransition(*cmdBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

        const GBuffer gbuffer{ *ms_Resources };

        cmdList.AddRenderTarget(gbuffer.m_AlbedoAndRoughness);
        cmdList.AddRenderTarget(gbuffer.m_EmissiveAndMetallic);
        cmdList.AddRenderTarget(gbuffer.m_WorldNormal);
        cmdList.AddRenderTarget(gbuffer.m_Mv);
        cmdList.AddRenderTarget(gbuffer.m_ViewDepth);
        cmdList.AddDepthStencil(gbuffer.m_DepthStencil);
        cmdList.SetRenderTargets();
        cmdList.FlushBarriers();

        if (!isLate)
        {
            cmdList.ClearRenderTarget(gbuffer.m_AlbedoAndRoughness);
            cmdList.ClearRenderTarget(gbuffer.m_EmissiveAndMetallic);
            cmdList.ClearRenderTarget(gbuffer.m_WorldNormal);
            cmdList.ClearRenderTarget(gbuffer.m_Mv);
            cmdList.ClearRenderTarget(gbuffer.m_ViewDepth);
            cmdList.ClearDepthStencil(gbuffer.m_DepthStencil);
        }

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);

        cmdList.GetD3D12GraphicsCommandList()->ExecuteIndirect(
            isMeshPipeline ? m_D3D12MeshDispatchCmdSignature : m_D3D12DrawCmdSignature,
            (uint32_t)cmdBuffer->GetElementCount(),
            cmdBuffer->GetD3D12Resource(),
            0,
            m_CmdCountBuffer->GetD3D12Resource(),
            0);
    }

    // GBuffer

    GBuffer::GBuffer(const benzin::RenderResources& resources)
        : m_AlbedoAndRoughness{ resources.Get(TextureId::AlbedoAndRoughness) }
        , m_EmissiveAndMetallic{ resources.Get(TextureId::EmissiveAndMetallic) }
        , m_WorldNormal{ resources.Get(TextureId::WorldNormal) }
        , m_Mv{ resources.Get(TextureId::Mv) }
        , m_ViewDepth{ resources.Get(TextureId::ViewDepth) }
        , m_DepthStencil{ resources.Get(TextureId::DepthStencil) }
    {}

}
