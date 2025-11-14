#include "joint/procedural_grass_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "color_convertions.hlsli"
#include "common.hlsli"
#include "gbuffer.hlsli"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(StructuredBuffer<joint::GrassPatch>, g_GrassPatches, joint::ProceduralGrassResources::GrassPatches);
BenzinDeclareRootResource(Texture2D<float>, g_PerlinNoise, joint::ProceduralGrassResources::PerlinNoise);

struct Rand01
{
    uint Seed;

    uint HashUint(uint x)
    {
        // Strong integer hash (PCG-style)

        x ^= x >> 17;
        x *= 0xed5ad4bb;
        x ^= x >> 11;
        x *= 0xac4c1b51;
        x ^= x >> 15;
        x *= 0x31848bab;
        x ^= x >> 14;

        return x;
    }

    float UintToFloat01(uint x)
    {
        return float(x & 0x00FFFFFFu) / float(0x01000000);
    }

    uint CombineSeed(uint a, uint b)
    {
        a ^= b + 0x9e3779b9 + (a << 6) + (a >> 2);
        return a;
    }

    void CombineSeed(uint seed)
    {
        Seed = CombineSeed(Seed, seed);
    }

    float Next(uint seed)
    {
        seed = CombineSeed(Seed, seed);
        return UintToFloat01(HashUint(seed));
    }
};

static Rand01 g_Rand;

static const float g_WindAnimationScale = 0.05;
static const float g_GrassLeaningFactor = 0.3;

static const uint g_VertexCountPerBladeEdge = (uint)joint::ProceduralGrassConsts::VertexCountPerBladeEdge;
static const uint g_VertexCountPerBlade = (uint)joint::ProceduralGrassConsts::VertexCountPerBlade;
static const uint g_TriangleCountPerBlade = (uint)joint::ProceduralGrassConsts::TriangleCountPerBlade;

static const uint g_MaxVertexCount = (uint)joint::ProceduralGrassConsts::MaxVertexCountPerThreadGroup;
static const uint g_MaxBladeCount = (uint)joint::ProceduralGrassConsts::MaxBladeCountPerPatch;
static const uint g_MaxTriangleCount = g_MaxBladeCount * g_TriangleCountPerBlade;

static const uint g_VertexPerThreadCount = 2;

static const uint g_AsGroupSize = (uint)joint::ProceduralGrassConsts::AsGroupSize;
static const uint g_MsGroupSize = g_MaxVertexCount / g_VertexPerThreadCount;

struct BladeArgs
{
    float2 Dir2D;
    float3 Offset;
    float3 RightDir;
};

struct BezierControlPoints
{
    float3 P0;
    float3 P1;
    float3 P2;
};

struct Vertex
{
    float4 ClipPos : SV_Position;
    float3 WorldPos : WorldPos;
    float ViewDepth : ViewDepth;
    float3 PrevViewPos : PrevViewPos;
    float BladeRootHeight : BladeRootHeight;
    float3 WorldNormal : WorldNormal;
    float PatchHeight : PatchHeight;
};

BladeArgs GenBladeArgs(float3 patchNormal)
{
    // Generate the blade direction in the patch and the blade offset from the patch center position

    const float dirAngle = 2.0 * g_Pi * g_Rand.Next(53);

    const float offsetAngle = 2.0 * g_Pi * g_Rand.Next(71);
    const float offsetRadius = g_PassConsts.SpacingInGrassPatch * sqrt(g_Rand.Next(48));

    const float3 tangent = normalize(cross(g_UpDir, patchNormal));
    const float3 bitangent = normalize(cross(patchNormal, tangent));

    BladeArgs info = (BladeArgs)0;
    info.Dir2D = float2(cos(dirAngle), sin(dirAngle));
    info.Offset = offsetRadius * (cos(offsetAngle) * tangent + sin(offsetAngle) * bitangent);
    info.RightDir = normalize(float3(info.Dir2D.y, 0.0, -info.Dir2D.x));

    return info;
}

float2 CalcWindOffset(float2 pos, float elapsedTimeInSec)
{
    // Ref: https://www.youtube.com/watch?v=wavnKZNSYqU&t=1063s&ab_channel=GameDevelopersConference

    float posOnSineWave = cos(g_PassConsts.WindDirection) * pos.x - sin(g_PassConsts.WindDirection) * pos.y;

    const float noise = g_PerlinNoise.SampleLevel(g_PointWrapSampler, 0.25 * pos, 0.0);
    const float t = elapsedTimeInSec * 2.0 + posOnSineWave + 4.0 * noise;

    const float windX = 2.0 * sin(0.5 * t);
    const float windY = 1.0 * sin(1.0 * t);

    return g_WindAnimationScale * float2(windX, windY);
}

void MakePersistentLength(float3 v0, inout float3 v1, inout float3 v2, float height)
{
    // Ref: http://steve.hollasch.net/cgindex/curves/cbezarclen.html

    float3 v01 = v1 - v0;
    float3 v12 = v2 - v1;
    float lv01 = length(v01);
    float lv12 = length(v12);

    float L1 = lv01 + lv12;
    float L0 = length(v2-v0);
    float L = (2.0 * L0 + L1) / 3.0;

    float ldiff = height / L;
    v01 = v01 * ldiff;
    v12 = v12 * ldiff;
    v1 = v0 + v01;
    v2 = v1 + v12;
}

BezierControlPoints CalcBladeBezierPoints(joint::GrassPatch patch, BladeArgs bladeArgs)
{
    BezierControlPoints points;
    points.P0 = patch.Pos + bladeArgs.Offset;

    const float bladeHeight = patch.Height + g_Rand.Next(123) * 0.01;
    points.P1 = points.P0 + float3(0.0, bladeHeight, 0.0);

    points.P2 = points.P1;
    points.P2.xz += bladeArgs.Dir2D * bladeHeight * g_GrassLeaningFactor;

    return points;
}

void ApplyBladeWindOffset(float elapsedTimeInSec, inout BezierControlPoints outBladePoints)
{
    outBladePoints.P2.xz += CalcWindOffset(outBladePoints.P0.xz, elapsedTimeInSec);
}

void ApplyBladeWidthOffset(BladeArgs bladeArgs, uint localVertexIndex, float bladeWidth, inout BezierControlPoints outBladePoints)
{
    const float bladeHeight = outBladePoints.P1.y;
    MakePersistentLength(outBladePoints.P0, outBladePoints.P1, outBladePoints.P2, bladeHeight);

    float3 sideOffset = (localVertexIndex & 1) ? 1.0 : -1.0;
    sideOffset *= bladeWidth * bladeArgs.RightDir;

    outBladePoints.P0 += sideOffset * 1.0;
    outBladePoints.P1 += sideOffset * 0.7;
    outBladePoints.P2 += sideOffset * 0.3;
}

float3 CalcQuadraticBezierPoint(float3 p0, float3 p1, float3 p2, float t)
{
    // B(t) = ((1 - t) ^ 2 * P0) + (2 * (1 - t) * t * P1) + (t ^ 2 * P2) =>
    // => a = (1 - t) * P0 + t * P1
    //    b = (1 - t) * P1 + t * P2
    // => (1 - t) * a + t * b

    const float3 a = lerp(p0, p1, t);
    const float3 b = lerp(p1, p2, t);
    return lerp(a, b, t);
}

float3 CalcQuadraticBezierDerivative(float3 p0, float3 p1, float3 p2, float t)
{
    return 2.0 * (1.0 - t) * (p1 - p0) + 2.0 * t * (p2 - p1);
}

struct Payload
{
    uint GrassPatchIndices[g_AsGroupSize];
};

groupshared Payload g_Payload;

[NumThreads(g_AsGroupSize, 1, 1)]
void AsMain(uint dtid : SV_DispatchThreadID)
{
    bool isVisible = dtid < g_PassConsts.GrassPatchCount;

    if (isVisible && g_PassConsts.IsFrustumCullingEnabled)
    {
        // TODO: Redo frustum culling
    }

    if (isVisible)
    {
        const uint index = WavePrefixCountBits(isVisible); // TODO: Need to understand the WavePrefixCountBits
        g_Payload.GrassPatchIndices[index] = dtid;
    }

    const uint visibleCount = WaveActiveCountBits(isVisible);
    DispatchMesh(visibleCount, 1, 1, g_Payload);
}

[NumThreads(g_MsGroupSize, 1, 1)]
[OutputTopology("triangle")]
void MsMain(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload Payload payload,
    out vertices Vertex outVertices[g_MaxVertexCount],
    out indices uint3 outTriangles[g_MaxTriangleCount]
)
{
    // Ref: https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/

    const uint patchIndex = payload.GrassPatchIndices[gid];

    if (patchIndex >= g_PassConsts.GrassPatchCount)
    {
        return;
    }

    const joint::GrassPatch patch = g_GrassPatches[patchIndex];

    const float distanceToCamera = length(patch.Pos - GetCameraConsts().m_WorldPosition);

    const float floatBladeCount = lerp(float(g_MaxBladeCount), 2.0, pow(saturate(distanceToCamera / (g_PassConsts.GrassEndDistance * 1.05)), 0.75)); // TODO: Some magic math
    const uint bladeCount = ceil(floatBladeCount);

    const uint vertexCount = bladeCount * g_VertexCountPerBlade;
    const uint triangleCount = bladeCount * g_TriangleCountPerBlade;

#if defined(CALC_STATS)
    if (gtid == 0)
    {
        InterlockedAddToStat(joint::ReadbackStat::ProceduralGrass_PatchCount, 1);
        InterlockedAddToStat(joint::ReadbackStat::ProceduralGrass_BladeCount, bladeCount);
        InterlockedAddToStat(joint::ReadbackStat::ProceduralGrass_VertexCount, vertexCount);
        InterlockedAddToStat(joint::ReadbackStat::ProceduralGrass_TriangleCount, triangleCount);
    }
#endif

    // NOTE: In Nvidia GPU you must provide exact quantity of vertex and primitives (g_MaxVertexCount and g_MaxTriangleCount won't work)
    SetMeshOutputCounts(vertexCount, triangleCount);

    g_Rand.CombineSeed((uint)(patch.Pos.x / g_PassConsts.SpacingInGrassPatch));
    g_Rand.CombineSeed((uint)(patch.Pos.y / g_PassConsts.SpacingInGrassPatch));

    for (uint i = 0; i < g_VertexPerThreadCount; ++i)
    {
        const uint vertexIndex = gtid + g_MsGroupSize * i;

        if (vertexIndex >= vertexCount)
        {
            break;
        }

        const uint bladeIndex = vertexIndex / g_VertexCountPerBlade;
        const uint localVertexIndex = vertexIndex % g_VertexCountPerBlade;

        g_Rand.CombineSeed(bladeIndex);

        const BladeArgs bladeArgs = GenBladeArgs(patch.Normal);

        BezierControlPoints bladePoints = CalcBladeBezierPoints(patch, bladeArgs);
        BezierControlPoints prevBladePoints = bladePoints;

        float bladeWidth = g_PassConsts.BladeWidth;
        bladeWidth *= g_MaxBladeCount / floatBladeCount;
        bladeWidth *= (bladeIndex == bladeCount - 1) ? frac(floatBladeCount) : 1.0;

        ApplyBladeWindOffset(g_FrameConsts.m_AnimationElapsedTimeInSec, bladePoints);
        ApplyBladeWindOffset(g_FrameConsts.m_PrevAnimationElapsedTimeInSec, prevBladePoints);

        ApplyBladeWidthOffset(bladeArgs, localVertexIndex, bladeWidth, bladePoints);
        ApplyBladeWidthOffset(bladeArgs, localVertexIndex, bladeWidth, prevBladePoints);

        Vertex vertex;
        vertex.BladeRootHeight = bladePoints.P0.y;
        vertex.PatchHeight = patch.Height;

        const float bladeT = (float)(localVertexIndex / 2) / (g_VertexCountPerBladeEdge - 1);

        vertex.WorldPos = CalcQuadraticBezierPoint(bladePoints.P0, bladePoints.P1, bladePoints.P2, bladeT);
        vertex.WorldNormal = cross(bladeArgs.RightDir, normalize(CalcQuadraticBezierDerivative(bladePoints.P0, bladePoints.P1, bladePoints.P2, bladeT)));
        
        const float4 viewPos = mul(float4(vertex.WorldPos, 1.0), GetCameraConsts().m_WorldToView);
        vertex.ClipPos = mul(viewPos, GetCameraConsts().m_ViewToClip);
        vertex.ViewDepth = viewPos.z;

        const float3 prevWorldPos = CalcQuadraticBezierPoint(prevBladePoints.P0, prevBladePoints.P1, prevBladePoints.P2, bladeT);
        vertex.PrevViewPos = mul(float4(prevWorldPos, 1.0), GetPrevCameraConsts().m_WorldToView).xyz;

        outVertices[vertexIndex] = vertex;
    }

    for (uint i = 0; i < g_VertexPerThreadCount; ++i)
    {
        const int triangleIndex = gtid + g_MsGroupSize * i;

        if (triangleIndex >= triangleCount)
        {
            break;
        }

        const int bladeIndex = triangleIndex / g_TriangleCountPerBlade;
        const int localTriangleIndex = triangleIndex % g_TriangleCountPerBlade;

        const int indexOffset = bladeIndex * g_VertexCountPerBlade + 2 * (localTriangleIndex / 2);

        const uint3 triangleIndices = localTriangleIndex & 1 ? uint3(0, 1, 2) : uint3(3, 2, 1);
        outTriangles[triangleIndex] = indexOffset + triangleIndices;
    }
}

PackedGBuffer PsMain(const Vertex input, bool isFrontFace : SV_IsFrontFace)
{
    const float2 perlinNoiseUv = input.WorldPos.xz * 0.2;
    const float perlinNoiseFactor = g_PerlinNoise.SampleLevel(g_PointWrapSampler, perlinNoiseUv, 0.0);

    GBuffer gbuffer;
    gbuffer.m_Roughness = lerp(0.3, 0.8, perlinNoiseFactor);
    gbuffer.m_Metallic = 0.0;
    gbuffer.m_ViewDepth = input.ViewDepth;

    const float selfshadowFactor = saturate(pow((input.WorldPos.y - input.BladeRootHeight) / input.PatchHeight, 1.5)) + 0.1;
    const float brightnessFactor = lerp(0.85, 1.5, perlinNoiseFactor);
    gbuffer.m_Albedo = g_PassConsts.BaseColor;
    gbuffer.m_Albedo *= selfshadowFactor;
    gbuffer.m_Albedo *= brightnessFactor;
    gbuffer.m_Albedo = SrgbToLinearAccurate(gbuffer.m_Albedo);

    gbuffer.m_WorldNormal = normalize(input.WorldNormal) * (isFrontFace ? 1.0 : -1.0);
    gbuffer.m_WorldNormal = normalize(lerp(g_UpDir, gbuffer.m_WorldNormal, 0.5)); // Interpolating the blade normal with the up vector gave the blades a softer look

    CalcGBufferMv(input.ClipPos.xy, input.ViewDepth, input.PrevViewPos, gbuffer);

    return PackGBuffer(gbuffer);
}
