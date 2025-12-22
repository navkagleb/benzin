#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/geometry_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/profiler.hpp>
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

BenzinAllowDereferenceOperatorForEnum(joint::GeometryCullingRootParam);
BenzinAllowDereferenceOperatorForEnum(joint::GeometryRootParam);
BenzinAllowDereferenceOperatorForEnum(joint::GeometryHzbRootParam);

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

                if (id == PsoId::Geometry_LateComputeCulling)
                {
                    proxy.m_Cs.m_Defines.push_back("LATE_CULLING");
                }
            });
        };

        createComputePso(PsoId::Geometry_EarlyComputeCulling);
        createComputePso(PsoId::Geometry_LateComputeCulling);

        CreateGeometryPso(PsoId::Geometry_Vertex, false);
        CreateGeometryPso(PsoId::Geometry_Mesh, true);

        ms_PsoManager->Create(PsoId::Geometry_HzbGeneration, [](benzin::ComputePsoProxy& proxy)
        {
            proxy.m_Cs.m_FileName = "geometry_hzb.hlsl";
        });

        {
            std::array<D3D12_INDIRECT_ARGUMENT_DESC, 2> d3d12ArgumentDescs = {};

            d3d12ArgumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            d3d12ArgumentDescs[0].Constant.RootParameterIndex = *benzin::UnifiedRootParameter::Root32Consts;
            d3d12ArgumentDescs[0].Constant.DestOffsetIn32BitValues = *joint::GeometryRootParam::MeshDrawIndex;
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
            d3d12ArgumentDescs[0].Constant.DestOffsetIn32BitValues = *joint::GeometryRootParam::MeshDrawIndex;
            d3d12ArgumentDescs[0].Constant.Num32BitValuesToSet = 1;

            d3d12ArgumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            d3d12ArgumentDescs[1].Constant.RootParameterIndex = *benzin::UnifiedRootParameter::Root32Consts;
            d3d12ArgumentDescs[1].Constant.DestOffsetIn32BitValues = *joint::GeometryRootParam::MeshletOffset;
            d3d12ArgumentDescs[1].Constant.Num32BitValuesToSet = 1;

            d3d12ArgumentDescs[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            d3d12ArgumentDescs[2].Constant.RootParameterIndex = *benzin::UnifiedRootParameter::Root32Consts;
            d3d12ArgumentDescs[2].Constant.DestOffsetIn32BitValues = *joint::GeometryRootParam::MeshletCount;
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

        ms_PsoManager->Destroy(PsoId::Geometry_EarlyComputeCulling);
        ms_PsoManager->Destroy(PsoId::Geometry_LateComputeCulling);
        ms_PsoManager->Destroy(PsoId::Geometry_Vertex);
        ms_PsoManager->Destroy(PsoId::Geometry_Mesh);
        ms_PsoManager->Destroy(PsoId::Geometry_HzbGeneration);

        ms_Resources->Destroy(TextureId::AlbedoAndRoughness);
        ms_Resources->Destroy(TextureId::EmissiveAndMetallic);
        ms_Resources->Destroy(TextureId::WorldNormal);
        ms_Resources->Destroy(TextureId::Mv);
        ms_Resources->Destroy(TextureId::ViewDepth);
        ms_Resources->Destroy(TextureId::Depth);
        ms_Resources->Destroy(TextureId::Hzb);
    }

    void GeometryPass::OnZeroFrameInit()
    {
        const uint32_t drawCount = ms_Scene->m_TotalMeshDrawCount;
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

        for (uint32_t frameIndex = 0; frameIndex < BENZIN_FRAME_COUNT; ++frameIndex)
        {
            m_CmdCountBuffers[frameIndex] = allocator.AllocateBuffer([frameIndex](benzin::BufferCreation& creation)
            {
                creation.m_DebugName = std::format("GeometryPass::CmdCountBuffer_{}", frameIndex);
                creation.m_Type = benzin::BufferType::Format;
                creation.m_DxgiFormat = DXGI_FORMAT_R32_UINT;
                creation.m_ElementSizeInBytes = sizeof(uint32_t);
                creation.m_ElementCount = 1;
                creation.m_IsUnorderedAccessAllowed = true;
            });

            m_DrawCmdBuffers[frameIndex] = allocator.AllocateBuffer([drawCount, frameIndex](benzin::BufferCreation& creation)
            {
                creation.m_DebugName = std::format("GeometryPass::DrawCmdBuffer_{}", frameIndex);
                creation.m_Type = benzin::BufferType::Structured;
                creation.m_ElementSizeInBytes = sizeof(joint::MeshDrawCmd);
                creation.m_ElementCount = drawCount;
                creation.m_IsUnorderedAccessAllowed = true;
            });

            m_DispatchCmdBuffers[frameIndex] = allocator.AllocateBuffer([drawCount, frameIndex](benzin::BufferCreation& creation)
            {
                creation.m_DebugName = std::format("GeometryPass::DispatchCmdBuffer_{}", frameIndex);
                creation.m_Type = benzin::BufferType::Structured;
                creation.m_ElementSizeInBytes = sizeof(joint::MeshDispatchCmd);
                creation.m_ElementCount = drawCount;
                creation.m_IsUnorderedAccessAllowed = true;
            });
        }

        benzin::QueryHeapCreation queryHeapCreation;
        queryHeapCreation.m_DebugName = "GeometryPass::StatsQueryHeap";
        queryHeapCreation.m_D3D12Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS1;
        queryHeapCreation.m_Count = 1;
        MakeUniquePtr(m_StatsQueryHeap, *ms_Device, queryHeapCreation);

        m_StatsBuffer = ms_Device->GetPersistentReadbackAllocator().AllocateStructuredBuffer(
            "GeometryPass::StatsBuffer",
            BENZIN_READBACK_LATENCY,
            sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS1));

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
        ms_Resources->Create(TextureId::Depth, GBufferSettings::ms_DepthStencilDxgiFormat, benzin::TextureAccessFlag::AllowDepthStencil);

        BenzinAssert(GBufferSettings::ms_DepthStencilDxgiFormat == DXGI_FORMAT_D32_FLOAT);
        ms_Resources->Create(
            TextureId::Hzb,
            DXGI_FORMAT_R32_FLOAT,
            ms_RenderViewportWidth,
            ms_RenderViewportHeight,
            benzin::CalcTextureMipCount(ms_RenderViewportWidth, ms_RenderViewportHeight),
            benzin::TextureAccessFlag::AllowUnorderedAccess);
    }

    void GeometryPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("GeometryPass");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        ID3D12GraphicsCommandList* d3d12CmdList = cmdList.GetD3D12GraphicsCommandList();

        d3d12CmdList->BeginQuery(m_StatsQueryHeap->GetD3D12QueryHeap(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS1, 0);

        RunCullingPass(false);
        RunDrawPass(false);
        RunHzbGeneration();
        RunCullingPass(true);
        RunDrawPass(true);

        d3d12CmdList->EndQuery(m_StatsQueryHeap->GetD3D12QueryHeap(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS1, 0);

        cmdList.AddTransition(*m_StatsBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
        cmdList.FlushBarriers();

        d3d12CmdList->ResolveQueryData(
            m_StatsQueryHeap->GetD3D12QueryHeap(),
            D3D12_QUERY_TYPE_PIPELINE_STATISTICS1,
            0,
            1,
            m_StatsBuffer->GetD3D12Resource(),
            ms_Device->GetReadbackWriteIndex() * sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS1));

        cmdList.AddTransition(*m_StatsBuffer, D3D12_RESOURCE_STATE_COMMON);
        cmdList.FlushBarriers();

        m_StatsBuffer->MapReadbackData<D3D12_QUERY_DATA_PIPELINE_STATISTICS1>(
            ms_Device->GetReadbackReadIndex(),
            1,
            [this](std::span<const D3D12_QUERY_DATA_PIPELINE_STATISTICS1> stats)
            {
                ms_Settings->GetSection<GBufferStats>().m_D3D12PipelineStats = stats.front();
            });
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

    void GeometryPass::RunCullingPass(bool isLate) const
    {
        using RootParam = joint::GeometryCullingRootParam;

        BenzinScopeProfile(isLate ? "GeometryPass::RunCulling_Late" : "GeometryPass::RunCulling_Early");
        BenzinGpuProfile(isLate ? "CullingLate" : "CullingEarly");

        benzin::ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const uint32_t drawCount = ms_Scene->m_TotalMeshDrawCount;
        const uint32_t activeIndex = ms_Device->GetActiveFrameIndex();

        cmdList.SetComputeRootConstant(*RootParam::MeshDrawCount, drawCount);
        cmdList.SetComputeRootSrv(*RootParam::MeshDraws, *ms_Scene->m_PerFrameResources[activeIndex].m_MeshDrawBuffer);
        cmdList.SetComputeRootSrv(*RootParam::Meshes, *ms_Scene->m_MeshBuffer);
        cmdList.SetComputeRootUav(*RootParam::MeshCmdCounter, *m_CmdCountBuffers[activeIndex]);
        cmdList.SetComputeRootUav(*RootParam::MeshDrawCmds, *m_DrawCmdBuffers[activeIndex]);
        cmdList.SetComputeRootUav(*RootParam::MeshDispatchCmds, *m_DispatchCmdBuffers[activeIndex]);

        if (isLate)
        {
            cmdList.SetComputeRootUav(*RootParam::VisibilityBuffer, *m_VisibilityBuffer);
        }
        else
        {
            cmdList.SetComputeRootSrv(*RootParam::VisibilityBuffer, *m_VisibilityBuffer);
        }

        cmdList.FlushBarriers();

        cmdList.ClearUnorderedAccess(*m_CmdCountBuffers[activeIndex], m_CmdCountBuffers[activeIndex]->GetUav());

        cmdList.SetComputePso(ms_PsoManager->GetCompute(isLate ? PsoId::Geometry_LateComputeCulling : PsoId::Geometry_EarlyComputeCulling));
        cmdList.Dispatch({ drawCount, 1, 1 }, { 64, 1, 1 });

        cmdList.AddUnorderedAccess(*m_CmdCountBuffers[activeIndex]);
        cmdList.AddUnorderedAccess(*m_DrawCmdBuffers[activeIndex]);
        cmdList.AddUnorderedAccess(*m_DispatchCmdBuffers[activeIndex]);
    }

    void GeometryPass::RunDrawPass(bool isLate) const
    {
        using RootParam = joint::GeometryRootParam;

        BenzinScopeProfile(isLate ? "GeometryPass::RunDrawPass_Late" : "GeometryPass::RunDrawPass_Early");
        BenzinGpuProfile(isLate ? "DrawLate" : "DrawEarly");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const bool isMeshPipeline = ms_Settings->GetSection<GBufferSettings>().m_IsMeshPipelineEnabled;

        if (isMeshPipeline)
        {
            cmdList.SetMeshPso(ms_PsoManager->GetMesh(PsoId::Geometry_Mesh));

            cmdList.SetGraphicsRootSrv(*RootParam::Vertices, *ms_Scene->m_VertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.SetGraphicsRootSrv(*RootParam::Meshlets, *ms_Scene->m_MeshletBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.SetGraphicsRootSrv(*RootParam::MeshletCullVolumes, *ms_Scene->m_MeshletCullVolumeBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.SetGraphicsRootSrv(*RootParam::MeshletVertexIndices, *ms_Scene->m_MeshletVertexIndexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cmdList.SetGraphicsRootSrv(*RootParam::MeshletIndices, *ms_Scene->m_MeshletIndexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
        else
        {
            cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::Geometry_Vertex));

            cmdList.SetVertexBuffer(*ms_Scene->m_VertexBuffer);
            cmdList.SetIndexBuffer(*ms_Scene->m_IndexBuffer);

            cmdList.GetD3D12GraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }

        cmdList.SetGraphicsRootSrv(*RootParam::MeshDraws, *ms_Scene->m_PerFrameResources[ms_Device->GetActiveFrameIndex()].m_MeshDrawBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmdList.SetGraphicsRootSrv(*RootParam::Materials, *ms_Scene->m_MaterialBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        const uint32_t activeIndex = ms_Device->GetActiveFrameIndex();
        const auto& cmdBuffer = isMeshPipeline ? m_DispatchCmdBuffers[activeIndex] : m_DrawCmdBuffers[activeIndex];

        cmdList.AddTransition(*m_CmdCountBuffers[activeIndex], D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        cmdList.AddTransition(*cmdBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

        const GBuffer gbuffer{ *ms_Resources };

        cmdList.AddRenderTarget(gbuffer.m_AlbedoAndRoughness, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddRenderTarget(gbuffer.m_EmissiveAndMetallic, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddRenderTarget(gbuffer.m_WorldNormal, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddRenderTarget(gbuffer.m_Mv, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddRenderTarget(gbuffer.m_ViewDepth, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList.AddDepthStencil(gbuffer.m_Depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        cmdList.SetRenderTargets();
        cmdList.FlushBarriers();

        if (!isLate)
        {
            cmdList.ClearRenderTarget(gbuffer.m_AlbedoAndRoughness);
            cmdList.ClearRenderTarget(gbuffer.m_EmissiveAndMetallic);
            cmdList.ClearRenderTarget(gbuffer.m_WorldNormal);
            cmdList.ClearRenderTarget(gbuffer.m_Mv);
            cmdList.ClearRenderTarget(gbuffer.m_ViewDepth);
            cmdList.ClearDepthStencil(gbuffer.m_Depth);
        }

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);

        cmdList.GetD3D12GraphicsCommandList()->ExecuteIndirect(
            isMeshPipeline ? m_D3D12MeshDispatchCmdSignature : m_D3D12DrawCmdSignature,
            (uint32_t)cmdBuffer->GetElementCount(),
            cmdBuffer->GetD3D12Resource(),
            0,
            m_CmdCountBuffers[activeIndex]->GetD3D12Resource(),
            0);
    }

    void GeometryPass::RunHzbGeneration() const
    {
        BenzinProfile();
        BenzinGpuProfile("HZB Generation");

        benzin::ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const benzin::Texture& depth = ms_Resources->Get(TextureId::Depth);
        const benzin::Texture& hzb = ms_Resources->Get(TextureId::Hzb);

        cmdList.CopyTextureRegion(hzb, hzb.CalcSubResourceIndex(0, 0), depth, depth.CalcSubResourceIndex(0, 0));

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::Geometry_HzbGeneration));

        for (uint32_t sourceMipIndex = 0; sourceMipIndex < hzb.GetMipCount() - 1; ++sourceMipIndex)
        {
            const uint32_t destMipIndex = sourceMipIndex + 1;
            const uint32_t destMipWidth = hzb.GetMipWidth(destMipIndex);
            const uint32_t destMipHeight = hzb.GetMipHeight(destMipIndex);

            joint::GeometryHzbConsts consts = {};
            consts.m_DestMipTexelSize = { 1.0f / destMipWidth, 1.0f / destMipHeight };
            consts.m_IsSourceWidthOdd = (hzb.GetMipWidth(sourceMipIndex) & 1) == 1;
            consts.m_IsSourceHeightOdd = (hzb.GetMipHeight(sourceMipIndex) & 1) == 1;

            cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConsts, ms_Device->GetConstBufferAllocator().Allocate(consts));

            using RootParam = joint::GeometryHzbRootParam;
            cmdList.SetComputeRootSrv(*RootParam::SourceMip, hzb, { .m_MipOffset = sourceMipIndex, .m_MipCount = 1 }, benzin::g_MaxEnum<D3D12_RESOURCE_STATES>);
            cmdList.SetComputeRootUav(*RootParam::DestMip, hzb, { .m_MipIndex = destMipIndex });
            cmdList.FlushBarriers();

            cmdList.Dispatch({ destMipWidth, destMipHeight, 1 }, { 8, 8, 1 });

            cmdList.AddUnorderedAccess(hzb);
        }
    }

    // GBuffer

    GBuffer::GBuffer(const benzin::RenderResources& resources)
        : m_AlbedoAndRoughness{ resources.Get(TextureId::AlbedoAndRoughness) }
        , m_EmissiveAndMetallic{ resources.Get(TextureId::EmissiveAndMetallic) }
        , m_WorldNormal{ resources.Get(TextureId::WorldNormal) }
        , m_Mv{ resources.Get(TextureId::Mv) }
        , m_ViewDepth{ resources.Get(TextureId::ViewDepth) }
        , m_Depth{ resources.Get(TextureId::Depth) }
    {}

}
