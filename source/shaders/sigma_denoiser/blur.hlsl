#define SIGMA_USE_BORDER_2

#include "joint/sigma_denoiser_resources.hpp"

#define RenderPassConstantsType joint::SigmaConstants
#include "unified_root_parameters.hlsli"

#include "gbuffer.hlsli"
#include "sigma_denoiser/lds_preloader.hlsli"
#include "sigma_denoiser/sigma_common.hlsli"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_WorldNormalTex, joint::Rc_SigmaBlur::WorldNormalTex);
BenzinDeclareRootResource(Texture2D<float>, g_ViewDepthTex, joint::Rc_SigmaBlur::ViewDepthTex);
BenzinDeclareRootResource(Texture2D<float>, g_PenumbraTex, joint::Rc_SigmaBlur::PenumbraTex);
BenzinDeclareRootResource(Texture2D<float2>, g_SmoothTilesTex, joint::Rc_SigmaBlur::SmoothTilesTex);

#if defined(FIRST_BLUR_PASS)
    BenzinDeclareRootResource(Texture2D<float>, g_HistoryTex, joint::Rc_SigmaBlur::HistoryTex);
    BenzinDeclareRootResource(RWTexture2D<float>, g_OutHistoryTex, joint::Rc_SigmaBlur::OutHistoryTex);
#else
    BenzinDeclareRootResource(Texture2D<float>, g_ShadowTex, joint::Rc_SigmaBlur::ShadowTex);
#endif

BenzinDeclareRootResource(RWTexture2D<float>, g_OutPenumbraTex, joint::Rc_SigmaBlur::OutPenumbraTex);
BenzinDeclareRootResource(RWTexture2D<float>, g_OutShadowTex, joint::Rc_SigmaBlur::OutShadowTex);

static const uint g_GroupSizeX = 8; // == g_ThreadCount
static const uint g_GroupSizeY = 16;

static const uint g_BufferSizeX = g_GroupSizeX + SIGMA_BORDER * 2;
static const uint g_BufferSizeY = g_GroupSizeY + SIGMA_BORDER * 2;

struct LdsData
{
    float Penumbra;
    float ViewDepth;
    float Shadow;
};

groupshared LdsData g_Data[g_BufferSizeY][g_BufferSizeX];

void Preload(uint2 localPos, uint2 pixelPos)
{
    LdsData data;
    data.Penumbra = g_PenumbraTex[pixelPos];
    data.ViewDepth = g_ViewDepthTex[pixelPos];

#if defined(FIRST_BLUR_PASS)
    data.Shadow = sigma::IsLit(data.Penumbra);
#else
    data.Shadow = sigma::UnpackShadow(g_ShadowTex[pixelPos]);
#endif

    g_Data[localPos.y][localPos.x] = data;
}

struct CsInput
{
    uint2 ThreadPos : SV_GroupThreadID;
    uint2 PixelPos : SV_DispatchThreadID;
    uint FlatThreadIndex : SV_GroupIndex;
};

[numthreads(g_GroupSizeX, g_GroupSizeY, 1)]
void CsMain(CsInput input)
{
    const bool isSky = g_SmoothTilesTex[input.PixelPos >> 4].y;

    if (!isSky)
    {
        sigma::LdsPreloadCreation creation;
        creation.ThreadPos = input.ThreadPos;
        creation.PixelPos = input.PixelPos;
        creation.FlatThreadIndex = input.FlatThreadIndex;
        creation.GroupSize = uint2(g_GroupSizeX, g_GroupSizeY);
        creation.BufferSize = uint2(g_BufferSizeX, g_BufferSizeY);
        creation.Dimension = g_FrameConstants.RenderResolution;

        SigmaRunLdsPreloader(creation, Preload);

        GroupMemoryBarrierWithGroupSync();
    }

    if (isSky || any(input.PixelPos >= g_FrameConstants.RenderResolution))
    {
        return;
    }

    const uint2 ldsPos = input.ThreadPos + SIGMA_BORDER;
    const LdsData centerData = g_Data[ldsPos.y][ldsPos.x];
    float centerSignNoL = float(centerData.Penumbra != 0.0);

    // Early out
    if (centerData.ViewDepth > sigma::g_DenoisingRange)
    {
        return;
    }

#if defined(FIRST_BLUR_PASS)
    if (g_PassConstants.StabilizationStrength != 0.0)
    {
        g_OutHistoryTex[input.PixelPos] = g_HistoryTex[input.PixelPos];
    }
#endif

    // Tile-based early out ( potentially )
    const float2 pixelUv = DispatchThreadIdToUv(input.PixelPos, g_FrameConstants.InvRenderResolution);

    float tileValue = sigma::TextureCubic(g_SmoothTilesTex, pixelUv);
#if defined(FIRST_BLUR_PASS)
    tileValue *= all(input.PixelPos < g_FrameConstants.RenderResolution); // due to USE_MAX_DIMS
#endif

    if (tileValue == 0.0 || centerData.Penumbra == 0.0)
    {
        g_OutPenumbraTex[input.PixelPos] = centerData.Penumbra;
        g_OutShadowTex[input.PixelPos] = sigma::PackShadow(centerData.Shadow);
    
        return;
    }

    // Position
    // ???
    const float3 viewPos = ReconstructViewPositionFromViewDepth(pixelUv, centerData.ViewDepth, g_FrameConstants.Camera.PackedFrustumPlaneSlopes);
    const float3 worldPos = mul(float4(viewPos, 1.0), g_FrameConstants.Camera.InvWorldToView).xyz;

    // Normal
    const float3 worldNormal = g_WorldNormalTex[input.PixelPos].xyz;
    const float3 viewNormal = mul(worldNormal, (float3x3)g_FrameConstants.Camera.WorldToViewForNormals); // ???

    // Parameters
    const float frustumSize = sigma::GetFrustumSizeAtDepth(
        g_FrameConstants.PixelToWorldScale,
        min(g_FrameConstants.RenderResolution.x, g_FrameConstants.RenderResolution.y),
        centerData.ViewDepth
    );
    const float unprojectViewDepth = sigma::PixelRadiusToWorld(1.0, g_FrameConstants.PixelToWorldScale, centerData.ViewDepth);
    const float2 geometryWeightParams = sigma::GetGeometryWeightParams(sigma::g_PlaneDistanceSensitivity, frustumSize, viewPos, viewNormal, 1.0);

#if 1
    // Estimate penumbra size and filter shadow ( pass 1: dense 3x3 or 5x5 )
    float blurredShadow = 0.0;
    float shadowWeightSum = 0.0;

    float blurredPenumbra = 0.0;
    float penumbraWeightSum = 0.0;

    float shadowCenterTap = 0.0;
    
    [unroll]
    for (int j = 0; j <= SIGMA_BORDER * 2; ++j)
    {
        [unroll]
        for (int i = 0; i <= SIGMA_BORDER * 2; ++i)
        {
            const int2 pos = input.ThreadPos + uint2(i, j);

            const LdsData sampleData = g_Data[pos.y][pos.x];
            const float sampleSignNoL = float(sampleData.Penumbra != 0);

            float shadowSample = sampleData.Shadow;
            float shadowWeight = 1.0;

            const bool isCenterSample = i == SIGMA_BORDER && j == SIGMA_BORDER;
            if (isCenterSample)
            {
                shadowCenterTap = sampleData.Shadow;
            }
            else
            {
                const float2 sampleUv = pixelUv + float2(i - SIGMA_BORDER, j - SIGMA_BORDER) * g_FrameConstants.InvRenderResolution;
                const float3 sampleViewPos = ReconstructViewPositionFromViewDepth(sampleUv, sampleData.ViewDepth, g_FrameConstants.Camera.PackedFrustumPlaneSlopes);
                const float sampleNoX = dot(viewNormal, sampleViewPos);

                shadowWeight *= sigma::ComputeWeight(sampleNoX, geometryWeightParams.x, geometryWeightParams.y);
                shadowWeight *= sigma::GetGaussianWeight(length(float2(i - SIGMA_BORDER, j - SIGMA_BORDER) / SIGMA_BORDER));
                shadowWeight *= (float)(sampleData.ViewDepth < sigma::g_DenoisingRange);
                shadowWeight *= (float)(centerSignNoL == sampleSignNoL);

                if (shadowWeight == 0.0)
                {
                    shadowSample = 0.0;
                }
            }

            float penumbraWeight = shadowWeight;
            penumbraWeight *= !sigma::IsLit(sampleData.Penumbra);
            penumbraWeight /= 1.0 + (sampleData.Penumbra / unprojectViewDepth); // Prefer smaller penumbra
            
            blurredShadow += shadowSample * shadowWeight;
            shadowWeightSum += shadowWeight;
            
            blurredPenumbra += sampleData.Penumbra * penumbraWeight;
            penumbraWeightSum += penumbraWeight;
        }
    }

    blurredShadow /= shadowWeightSum;
    shadowWeightSum = 1.0;
    
    blurredPenumbra /= max(penumbraWeightSum, sigma::g_Eps); // Yes, without patching
    penumbraWeightSum = penumbraWeightSum != 0.0;
#endif

    // Avoid 1-pixel wide blur if penumbra size < 1 pixel
    const float penumbraInPixels = blurredPenumbra / unprojectViewDepth;
    const float factor = sigma::LinearStep(0.75, 1.25, penumbraInPixels);
    // blurredShadow = lerp(shadowCenterTap, blurredShadow, factor); // TODO: fixes not blurred pixels

    const float invHitDist = 1.0 / max(blurredPenumbra, sigma::g_Eps);

#if 1
    // Tangent basis with anisotropy
    const float3x3 worldToLocal = sigma::GetOrthonormalBasisFromNormal(viewNormal);
    float3 tangent = worldToLocal[0];
    float3 bitangent = worldToLocal[1];

#if 0
    const float3 worldLightDirection = normalize(g_PassConstants.LightWorldPosition - worldPos);
    const float3 viewLightDirection = mul(worldLightDirection, (float3x3)g_FrameConstants.Camera.WorldToView);
#endif

    const float3 viewSunDirection = mul(g_PassConstants.WorldSunDirection, (float3x3)g_FrameConstants.Camera.WorldToView);
    
#if 1
    const float3 t = cross(viewSunDirection, viewNormal); // TODO: add support for other light types to bring proper anisotropic filtering
    if (length(t) > 0.001)
    {
        tangent = normalize(t);
        bitangent = cross( tangent, viewNormal);

        const float cosa = abs(dot(viewNormal, viewSunDirection));
        const float skewFactor = lerp(0.25, 1.0, cosa);

        //Tv *= skewFactor; // TODO: let's not srink filtering in the other direction
        bitangent /= skewFactor;
    }
#endif

    // Blur radius
    const float worldRadius = sigma::GetKernelPixelRadius(blurredPenumbra, unprojectViewDepth, tileValue) * unprojectViewDepth;

    tangent *= worldRadius;
    bitangent *= worldRadius;

    // Estimate penumbra size and filter shadow ( pass 2: sparse 8-taps )
    const float invEstimatedPenumbra = 1.0 / max(blurredPenumbra, sigma::g_Eps);
    
    [unroll]
    for (uint sampleIndex = 0; sampleIndex < sigma::g_PoissonSampleCount; ++sampleIndex)
    {
#if defined(FIRST_BLUR_PASS)
        const float4 rotator = g_PassConstants.BlurRotator;
#else
        const float4 rotator = g_PassConstants.PostBlurRotator;
#endif

        // Sample coordinates
        const float3 sampleOffset = sigma::g_PoissonSamples[sampleIndex];
        float2 sampleUv = sigma::GetKernelSampleUv(g_FrameConstants.Camera.ViewToClip, sampleOffset.xy, viewPos, tangent, bitangent, rotator);

        // Snap to the pixel center!
        sampleUv = (floor(sampleUv * g_FrameConstants.RenderResolution) + 0.5) * g_FrameConstants.InvRenderResolution;

        // Fetch data
        const float samplePenumbra = g_PenumbraTex.SampleLevel(g_PointClampSampler, sampleUv, 0.0);
        const float sampleViewDepth = g_ViewDepthTex.SampleLevel(g_PointClampSampler, sampleUv, 0.0);
        const float sampleSignNoL = float(samplePenumbra != 0.0);

        // Sample weight
        const float3 sampleViewPos = ReconstructViewPositionFromViewDepth(sampleUv, sampleViewDepth, g_FrameConstants.Camera.PackedFrustumPlaneSlopes);
        const float NoX = dot(viewNormal, sampleViewPos);

        float shadowWeight = sigma::IsInScreenNearest(sampleUv);
        shadowWeight *= sigma::GetGaussianWeight(sampleOffset.z);
        shadowWeight *= sigma::ComputeWeight(NoX, geometryWeightParams.x, geometryWeightParams.y);
        shadowWeight *= float(sampleViewDepth < sigma::g_DenoisingRange);
        shadowWeight *= float(centerSignNoL == sampleSignNoL);

        // Avoid umbra leaking inside wide penumbra
        float t = saturate(samplePenumbra * invEstimatedPenumbra);
        shadowWeight *= smoothstep(0.0, 1.0, t); // TODO: it works surprisingly well, keep an eye on it!

        // Fetch shadow
#if defined(FIRST_BLUR_PASS)
        float sampleShadow = sigma::IsLit(samplePenumbra); // TODO: Rewrite
#else
        float sampleShadow = g_ShadowTex.SampleLevel(g_PointClampSampler, sampleUv, 0.0);
        sampleShadow = sigma::UnpackShadow(sampleShadow);
#endif
        sampleShadow = shadowWeight == 0.0 ? 0.0 : sampleShadow;

        const float penumraInPixels = samplePenumbra / unprojectViewDepth;
        
        float penumbraWeight = shadowWeight;
        penumbraWeight *= !sigma::IsLit(samplePenumbra);
        penumbraWeight /= 1.0 + penumraInPixels; // prefer smaller penumbra

        blurredShadow += sampleShadow * shadowWeight;
        shadowWeightSum += shadowWeight;
        
        blurredPenumbra += samplePenumbra * penumbraWeight;
        penumbraWeightSum += penumbraWeight;
    }
#endif
    
    blurredShadow /= shadowWeightSum;
    blurredPenumbra = penumbraWeightSum == 0.0 ? centerData.Penumbra : blurredPenumbra / penumbraWeightSum;

#if !defined(FIRST_BLUR_PASS)
    if (g_PassConstants.StabilizationStrength != 0)
#endif
    {
        g_OutPenumbraTex[input.PixelPos] = blurredPenumbra;
    }

    g_OutShadowTex[input.PixelPos] = sigma::PackShadow(blurredShadow);
}
