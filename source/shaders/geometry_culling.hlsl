#include "joint/geometry_resources.hpp"
#include "joint/mesh_types.hpp"
#include "unified_root_parameters.hlsli"

#define LATE_CULLING_ENABLED defined(LATE_CULLING)

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
    const uint drawCount = BenzinGetRootConstant(joint::GeometryCullingRootParam::MeshDrawCount);
    if (dtid >= drawCount)
        return;

    StructuredBuffer<joint::MeshDraw> draws = BenzinGetRootResource(joint::GeometryCullingRootParam::MeshDraws);
    StructuredBuffer<joint::Mesh> meshes = BenzinGetRootResource(joint::GeometryCullingRootParam::Meshes);

    const joint::MeshDraw draw = draws[dtid];
    const joint::Mesh mesh = meshes[draw.m_MeshIndex];

#if LATE_CULLING_ENABLED
    RWBuffer<uint> visibilityBuffer = BenzinGetRootResource(joint::GeometryCullingRootParam::VisibilityBuffer);
    bool isVisible = true;
#else
    Buffer<uint> visibilityBuffer = BenzinGetRootResource(joint::GeometryCullingRootParam::VisibilityBuffer);
    bool isVisible = visibilityBuffer[dtid];
#endif

    if (g_FrameConsts.m_IsFrustumCullingEnabled && isVisible)
    {
        isVisible &= !IsFrustumCulled(draw, mesh.m_Center, mesh.m_Radius);
    }

#if LATE_CULLING_ENABLED
    const bool isDrawNeeded = isVisible && !visibilityBuffer[dtid];
#else
    const bool isDrawNeeded = isVisible;
#endif

    RWBuffer<uint> cmdCounter = BenzinGetRootResource(joint::GeometryCullingRootParam::MeshCmdCounter);
    RWStructuredBuffer<joint::MeshDrawCmd> drawCmds = BenzinGetRootResource(joint::GeometryCullingRootParam::MeshDrawCmds);
    RWStructuredBuffer<joint::MeshDispatchCmd> dispatchCmds = BenzinGetRootResource(joint::GeometryCullingRootParam::MeshDispatchCmds);

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
        InterlockedAdd(cmdCounter[0], 1, cmdIndex);

        drawCmds[cmdIndex] = cmd;
        dispatchCmds[cmdIndex] = dispatchCmd;
    }

#if LATE_CULLING_ENABLED
    visibilityBuffer[dtid] = isVisible;
#endif
}
