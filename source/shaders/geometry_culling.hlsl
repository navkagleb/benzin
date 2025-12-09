#include "joint/geometry_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "joint/mesh_types.hpp"

#define LATE_CULLING_ENABLED defined(LATE_CULLING)

BenzinDeclareRootResource(StructuredBuffer<joint::MeshDraw>, g_MeshDraws, joint::GeometryCullingResources::MeshDraws);
BenzinDeclareRootResource(StructuredBuffer<joint::Mesh>, g_Meshes, joint::GeometryCullingResources::Meshes);
#if LATE_CULLING_ENABLED
BenzinDeclareRootResource(RWBuffer<uint>, g_VisibilityBuffer, joint::GeometryCullingResources::VisibilityBuffer);
#else
BenzinDeclareRootResource(Buffer<uint>, g_VisibilityBuffer, joint::GeometryCullingResources::VisibilityBuffer);
#endif
BenzinDeclareRootResource(RWBuffer<uint>, g_CmdCounter, joint::GeometryCullingResources::MeshCmdCounter);
BenzinDeclareRootResource(RWStructuredBuffer<joint::MeshDrawCmd>, g_DrawCmds, joint::GeometryCullingResources::MeshDrawCmds);
BenzinDeclareRootResource(RWStructuredBuffer<joint::MeshDispatchCmd>, g_DispatchCmds, joint::GeometryCullingResources::MeshDispatchCmds);

bool IsFrustumCulled(joint::MeshDraw draw, float3 center, float radius)
{
    bool isVisible = true;

    float4 viewCenter = mul(float4(center, 1.0), draw.m_LocalToWorld);
    viewCenter = mul(viewCenter, GetCameraConsts().m_WorldToView);

    const float worldRadius = radius * draw.m_LocalToWorldScale;

    [unroll]
    for (uint i = 0; i < 6; ++i)
    {
        const float4 viewPlane = GetCameraConsts().m_ViewFrustumPlanes[i];
        const float distanceToPlane = dot(viewPlane.xyz, viewCenter.xyz) + viewPlane.w;

        isVisible &= distanceToPlane < worldRadius;
    }

    return !isVisible;
}

[numthreads(64, 1, 1)]
void CsMain(uint dtid : SV_DispatchThreadID)
{
    if (dtid >= BenzinGetRootConstant(joint::GeometryCullingResources::MeshDrawCount))
        return;

    const joint::MeshDraw draw = g_MeshDraws[dtid];
    const joint::Mesh mesh = g_Meshes[draw.m_MeshIndex];

#if LATE_CULLING_ENABLED
    bool isVisible = true;
#else
    bool isVisible = g_VisibilityBuffer[dtid];
#endif

    if (g_FrameConsts.m_IsFrustumCullingEnabled && isVisible)
    {
        isVisible &= !IsFrustumCulled(draw, mesh.m_Center, mesh.m_Radius);
    }

#if LATE_CULLING_ENABLED
    const bool isDrawNeeded = isVisible && !g_VisibilityBuffer[dtid];
#else
    const bool isDrawNeeded = isVisible;
#endif

    if (isDrawNeeded)
    {
        const joint::MeshLod lod = mesh.m_Lods[0];

        joint::MeshDrawCmd cmd = (joint::MeshDrawCmd)0;
        cmd.m_DrawIndex = dtid;
        cmd.m_IndexCountPerInstance = lod.m_IndexCount;
        cmd.m_InstanceCount = 1;
        cmd.m_StartIndexLocation = lod.m_IndexOffset;
        cmd.m_BaseVertexLocation = mesh.m_VertexOffset;
        cmd.m_StartInstanceLocation = 0;

        joint::MeshDispatchCmd dispatchCmd = (joint::MeshDispatchCmd)0;
        dispatchCmd.m_DrawIndex = dtid;
        dispatchCmd.m_MeshletOffset = lod.m_MeshletOffset;
        dispatchCmd.m_MeshletCount = lod.m_MeshletCount;
        dispatchCmd.m_ThreadGroupCountX = lod.m_MeshletCount; // (lod.m_MeshletCount + asGroupSize - 1) / asGroupSize;
        dispatchCmd.m_ThreadGroupCountY = 1;
        dispatchCmd.m_ThreadGroupCountZ = 1;

        uint cmdIndex;
        InterlockedAdd(g_CmdCounter[0], 1, cmdIndex);

        g_DrawCmds[cmdIndex] = cmd;
        g_DispatchCmds[cmdIndex] = dispatchCmd;
    }

#if LATE_CULLING_ENABLED
    g_VisibilityBuffer[dtid] = isVisible;
#endif
}
