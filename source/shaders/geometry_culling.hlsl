#include "joint/geometry_resources.hpp"
#include "joint/mesh_types.hpp"
#include "unified_root_parameters.hlsli"

#define LATE_CULLING_ENABLED defined(LATE_CULLING)

BenzinDeclareRenderPassConsts(joint::GeometryCullConsts, g_CullConsts);

bool IsFrustumCulled(joint::MeshDraw draw, float3 viewCenter, float worldRadius)
{
    bool isVisible = true;

    [unroll]
    for (uint i = 0; i < 6; ++i)
    {
        const float4 viewPlane = GetCameraConsts().m_ViewFrustumPlanes[i];
        const float distanceToPlane = dot(viewPlane.xyz, viewCenter.xyz) + viewPlane.w;

        isVisible &= distanceToPlane < worldRadius;
    }

    return !isVisible;
}

bool ProjectSphere(float3 viewCenter, float radius, out float4 uvAabb)
{
    // Ref: https://jcgt.org/published/0002/02/05/
    // Ref: https://zeux.io/2023/01/12/approximate-projected-bounds/

    if (viewCenter.z < radius + g_CullConsts.m_NearZ)
        return false;

    const float3 cr = viewCenter * radius;
    const float czr2 = viewCenter.z * viewCenter.z - radius * radius;

    const float vx = sqrt(viewCenter.x * viewCenter.x + czr2);
    const float minx = (vx * viewCenter.x - cr.z) / (vx * viewCenter.z + cr.x);
    const float maxx = (vx * viewCenter.x + cr.z) / (vx * viewCenter.z - cr.x);

    const float vy = sqrt(viewCenter.y * viewCenter.y + czr2);
    const float miny = (vy * viewCenter.y - cr.z) / (vy * viewCenter.z + cr.y);
    const float maxy = (vy * viewCenter.y + cr.z) / (vy * viewCenter.z - cr.y);

    uvAabb = float4(minx * g_CullConsts.m_P00, miny * g_CullConsts.m_P11, maxx * g_CullConsts.m_P00, maxy * g_CullConsts.m_P11);
    uvAabb = uvAabb.xwzy * float4(0.5f, -0.5f, 0.5f, -0.5f) + 0.5f;

    return true;
}

[numthreads(64, 1, 1)]
void CsMain(uint dtid : SV_DispatchThreadID)
{
    if (dtid >= g_CullConsts.m_MeshDrawCount)
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

    float4 viewCenter = mul(float4(mesh.m_Center, 1.0), draw.m_LocalToWorld);
    viewCenter = mul(viewCenter, GetCameraConsts().m_WorldToView);

    const float worldRadius = mesh.m_Radius * draw.m_LocalToWorldScale;

    if (g_CullConsts.m_IsFrustumCullingEnabled && isVisible)
    {
        isVisible = isVisible && !IsFrustumCulled(draw, viewCenter.xyz, worldRadius);
    }

#if LATE_CULLING_ENABLED
    if (g_CullConsts.m_IsOcclusionCullingEnabled && isVisible)
    {
        float4 uvAabb;
        if (ProjectSphere(viewCenter.xyz, worldRadius, uvAabb))
        {
            const float width = (uvAabb.z - uvAabb.x) * g_FrameConsts.m_RenderResolution.x;
            const float height = (uvAabb.w - uvAabb.y) * g_FrameConsts.m_RenderResolution.y;
            const float mip = ceil(log2(max(width, height)));

            Texture2D<float> hzb = BenzinGetRootResource(joint::GeometryCullingRootParam::Hzb);

            const float hzbDepth =  hzb.SampleLevel(g_MinLinearClampSampler, (uvAabb.xy + uvAabb.zw) * 0.5, mip);
            const float sphereDepth = g_CullConsts.m_NearZ / (viewCenter.z - worldRadius);

            isVisible = isVisible && sphereDepth > hzbDepth;
        }
    }

    const bool isDrawNeeded = isVisible && !visibilityBuffer[dtid];
#else
    const bool isDrawNeeded = isVisible;
#endif

    if (isDrawNeeded)
    {
        RWBuffer<uint> cmdCounter = BenzinGetRootResource(joint::GeometryCullingRootParam::MeshCmdCounter);

        uint cmdIndex;
        InterlockedAdd(cmdCounter[0], 1, cmdIndex);

        const joint::MeshLod lod = mesh.m_Lods[0];

        if (g_CullConsts.m_IsMeshPipelineEnabled)
        {
            joint::MeshDispatchCmd cmd = (joint::MeshDispatchCmd)0;
            cmd.m_DrawIndex = dtid;
            cmd.m_MeshletOffset = lod.m_MeshletOffset;
            cmd.m_MeshletCount = lod.m_MeshletCount;
            cmd.m_ThreadGroupCountX = lod.m_MeshletCount; // (lod.m_MeshletCount + asGroupSize - 1) / asGroupSize;
            cmd.m_ThreadGroupCountY = 1;
            cmd.m_ThreadGroupCountZ = 1;

            RWStructuredBuffer<joint::MeshDispatchCmd> dispatchCmds = BenzinGetRootResource(joint::GeometryCullingRootParam::MeshDispatchCmds);
            dispatchCmds[cmdIndex] = cmd;
        }
        else
        {
            joint::MeshDrawCmd cmd = (joint::MeshDrawCmd)0;
            cmd.m_DrawIndex = dtid;
            cmd.m_IndexCountPerInstance = lod.m_IndexCount;
            cmd.m_InstanceCount = 1;
            cmd.m_StartIndexLocation = lod.m_IndexOffset;
            cmd.m_BaseVertexLocation = mesh.m_VertexOffset;
            cmd.m_StartInstanceLocation = 0;

            RWStructuredBuffer<joint::MeshDrawCmd> drawCmds = BenzinGetRootResource(joint::GeometryCullingRootParam::MeshDrawCmds);
            drawCmds[cmdIndex] = cmd;
        }
    }

#if LATE_CULLING_ENABLED
    visibilityBuffer[dtid] = isVisible;
#endif
}
