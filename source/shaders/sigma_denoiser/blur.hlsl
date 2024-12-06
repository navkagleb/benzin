#define SIGMA_USE_BORDER_2

#include "joint/sigma_denoiser_resources.hpp"

#define RenderPassConstantsType joint::SigmaConstants
#include "unified_root_parameters.hlsli"

#include "sigma_denoiser/lds_preloader.hlsli"
#include "sigma_denoiser/sigma_common.hlsli"

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

struct PixelData
{
    float Penumbra;
    float ViewDepth;
    float Shadow;
};

groupshared PixelData g_PixelsData[g_BufferSizeY][g_BufferSizeX];

void Preload(uint2 sharedPos, uint2 pixelPos)
{
    PixelData pixel;
    pixel.Penumbra = g_PenumbraTex[pixelPos];
    pixel.ViewDepth = g_ViewDepthTex[pixelPos];

#if defined(FIRST_BLUR_PASS)
    pixel.Shadow = sigma::IsLit(pixel.Penumbra); // This is ok. Full shadow - 0, No shadow = 1
#else
    pixel.Shadow = sigma::UnpackShadow(g_ShadowTex[pixelPos]);
#endif

    g_PixelsData[sharedPos.y][sharedPos.x] = pixel;
}

struct CsInput
{
    uint2 ThreadPos : SV_GroupThreadID;
    uint2 PixelPos : SV_DispatchThreadID;
    uint FlatThreadIndex : SV_GroupIndex;
};

struct BlurParams
{
    float2 UvToViewScale;
    float2 UvToViewBias;

    PixelData CenterPixel;

    float2 BaseUv;
    float3 BaseViewPosition;
    float3 BaseViewNormal;

    float WorldPixelSize;
    float2 GeometryWeightParams;
};

struct SampleParams
{
    float3 ViewPosition;
    float NormDistanceFromCenter;
};

struct SparseBlurKernel
{
    float3 Tangent;
    float3 Bitangent;
    float4 Rotator; // 2x2 matrix
};

BlurParams GetBlurParams(float2 baseUv, PixelData centerPixel)
{
    const joint::CameraConstants camera = g_FrameConstants.Camera;
    const float pixelToWorldScale = g_FrameConstants.PixelToWorldScale;
    const float3 worldNormal = g_WorldNormalTex.SampleLevel(g_PointClampSampler, baseUv, 0.0).xyz;

    const float worldFrustumSize = sigma::PixelRadiusToWorld(
        min(g_FrameConstants.RenderResolution.x, g_FrameConstants.RenderResolution.y),
        pixelToWorldScale,
        centerPixel.ViewDepth
    );

    BlurParams params;
    params.UvToViewScale = camera.UvToViewScale;
    params.UvToViewBias = camera.UvToViewBias;
    params.CenterPixel = centerPixel;
    params.BaseUv = baseUv;
    params.BaseViewPosition = ReconstructViewPosition(baseUv, centerPixel.ViewDepth, params.UvToViewScale, params.UvToViewBias);
    params.BaseViewNormal = mul(worldNormal, (float3x3)camera.WorldToView);
    params.WorldPixelSize = sigma::GetWorldPixelSize(pixelToWorldScale, centerPixel.ViewDepth);
    params.GeometryWeightParams = sigma::GetGeometryWeightParams(worldFrustumSize, params.BaseViewPosition, params.BaseViewNormal);

    return params;
}

float CalcShadowWeight(BlurParams params, PixelData samplePixel, SampleParams sampleParams)
{
    const float surfaceViewAlignment = dot(params.BaseViewNormal, sampleParams.ViewPosition);

    float shadowWeight = 1.0;
    shadowWeight *= sigma::ComputeWeight(surfaceViewAlignment, params.GeometryWeightParams.x, params.GeometryWeightParams.y);
    shadowWeight *= sigma::GetGaussianWeight(sampleParams.NormDistanceFromCenter);
    shadowWeight *= (float)sigma::IsBothLitOrUmbra(params.CenterPixel.Penumbra, samplePixel.Penumbra);

    return shadowWeight;
}

float CalcPenumbraWeight(BlurParams params, float shadowWeight, PixelData samplePixel)
{
    float penumbraWeight = shadowWeight;
    penumbraWeight *= params.WorldPixelSize / (params.WorldPixelSize + samplePixel.Penumbra); // Prefer smaller penumbra
    penumbraWeight *= !sigma::IsLit(samplePixel.Penumbra); // TODO: If this is removed - removes the flickering

    return penumbraWeight;
}

SparseBlurKernel CalcSparseBlurKernel(BlurParams params, float blurredPenumbra, float tileValue)
{
    // Tangent basis with anisotropy
    const float3x3 worldToLocal = sigma::GetOrthonormalBasisFromNormal(params.BaseViewNormal); // TODO: ViewNormal???

    SparseBlurKernel kernel;
    kernel.Tangent = worldToLocal[0];
    kernel.Bitangent = worldToLocal[1];
#if defined(FIRST_BLUR_PASS)
    kernel.Rotator = g_PassConstants.BlurRotator;
#else
    kernel.Rotator = g_PassConstants.PostBlurRotator;
#endif

    const float3 viewSunDirection = mul(g_PassConstants.WorldSunDirection, (float3x3)g_FrameConstants.Camera.WorldToView); // TODO: Move to cpp side
    const float3 t = cross(viewSunDirection, params.BaseViewNormal); // NRD TODO: add support for other light types to bring proper anisotropic filtering
    if (length(t) > 0.001)
    {
        kernel.Tangent = normalize(t);
        kernel.Bitangent = cross(kernel.Tangent, params.BaseViewNormal);

        const float cosa = abs(dot(params.BaseViewNormal, viewSunDirection));
        const float skewFactor = lerp(0.25, 1.0, cosa);

        //Tv *= skewFactor; // TODO: let's not srink filtering in the other direction
        kernel.Bitangent /= skewFactor;
    }

    const float pixelRadius = sigma::GetKernelPixelRadius(blurredPenumbra, params.WorldPixelSize, tileValue);
    const float worldPixelRadius = params.WorldPixelSize; //TODO: Why we multipy pixelRadius by worldPixelSize

    kernel.Tangent *= worldPixelRadius;
    kernel.Bitangent *= worldPixelRadius;

    return kernel;
}

float2 CalcSparseBlurKernelUv(SparseBlurKernel kernel, float2 offset, float3 viewPosition)
{
    // We can't rotate T and B instead, because T is skewed
    offset.xy = sigma::RotateVectorByRotator(offset, kernel.Rotator);

    viewPosition += offset.x * kernel.Tangent + offset.y * kernel.Bitangent;

    const float4x4 viewToClip = g_FrameConstants.Camera.ViewToClip;

    const float4 clipPos = mul(float4(viewPosition, 1.0), viewToClip); // TODO: Why this don't work?
    // const float4 clipPos = mul(g_FrameConstants.Camera.ViewToClip, float4(viewPosition, 1.0));
    const float2 uv = ClipToUv(clipPos);

    return uv;
}

void RunDenseBlur(CsInput input, BlurParams params, out float2 outShadow, out float2 outPenumbra)
{
    outShadow = 0.0;
    outPenumbra = 0.0;

    [unroll]
    for (int j = -SIGMA_BORDER; j <= SIGMA_BORDER; ++j)
    {
        [unroll]
        for (int i = -SIGMA_BORDER; i <= SIGMA_BORDER; ++i)
        {
            const uint2 sharedPosition = input.ThreadPos + int2(i, j) + SIGMA_BORDER;
            const PixelData pixel = g_PixelsData[sharedPosition.y][sharedPosition.x];

            float shadowWeight = 1.0;

            const bool isCenterSample = i == 0 && j == 0;
            if (!isCenterSample)
            {
                const float2 pixelOffset = float2(i, j);
                const float2 uv = params.BaseUv + pixelOffset * g_FrameConstants.InvRenderResolution;

                SampleParams sampleParams;
                sampleParams.ViewPosition = ReconstructViewPosition(uv, pixel.ViewDepth, params.UvToViewScale, params.UvToViewBias);
                sampleParams.NormDistanceFromCenter = length(pixelOffset / SIGMA_BORDER);

                shadowWeight = CalcShadowWeight(params, pixel, sampleParams);
            }

            const float penumbraWeight = CalcPenumbraWeight(params, shadowWeight, pixel);

            outShadow += float2(pixel.Shadow, 1.0) * shadowWeight;
            outPenumbra += float2(pixel.Penumbra, 1.0) * penumbraWeight;
        }
    }

    outShadow.x /= outShadow.y;
    outShadow.y = 1.0;

    outPenumbra.x /= max(outPenumbra.y, sigma::g_Eps); // Yes, without patching // TODO: What it means?
    outPenumbra.y = outPenumbra.y != 0.0;
}

void RunSparseBlur(BlurParams params, float tileValue, inout float2 outShadow, inout float2 outPenumbra)
{
    // World space sampling

    const SparseBlurKernel sparseKernel = CalcSparseBlurKernel(params, outPenumbra.x, tileValue);

    const float invEstimatedPenumbra = 1.0 / max(outPenumbra.x, sigma::g_Eps);

    for (uint sampleIndex = 0; sampleIndex < sigma::g_PoissonSampleCount; ++sampleIndex)
    {
        const float3 offset = sigma::g_PoissonSamples[sampleIndex]; // TODO: Name this variable with prefix

        float2 uv = CalcSparseBlurKernelUv(sparseKernel, offset.xy, params.BaseViewPosition);
        uv = (floor(uv * g_FrameConstants.RenderResolution) + 0.5) * g_FrameConstants.InvRenderResolution; // Snap to the pixel center

        const uint2 pixelPosition = uv * g_FrameConstants.RenderResolution;
        
        PixelData samplePixel;
#if 0
        samplePixel.ViewDepth = g_ViewDepthTex.SampleLevel(g_PointClampSampler, uv, 0.0);
        samplePixel.Penumbra = g_PenumbraTex.SampleLevel(g_PointClampSampler, uv, 0.0);
#else
        samplePixel.ViewDepth = g_ViewDepthTex[pixelPosition].x;
        samplePixel.Penumbra = g_PenumbraTex[pixelPosition].x;
#endif
#if defined(FIRST_BLUR_PASS)
        samplePixel.Shadow = sigma::IsLit(samplePixel.Penumbra);
#else
    #if 0
        samplePixel.Shadow = g_ShadowTex.SampleLevel(g_PointClampSampler, uv, 0.0);
    #else
        samplePixel.Shadow = g_ShadowTex[pixelPosition].x;
    #endif
        samplePixel.Shadow = sigma::UnpackShadow(samplePixel.Shadow);
#endif

        SampleParams sampleParams;
        sampleParams.ViewPosition = ReconstructViewPosition(uv, samplePixel.ViewDepth, params.UvToViewScale, params.UvToViewBias);
        sampleParams.NormDistanceFromCenter = offset.z;

        float shadowWeight = sigma::IsInScreenNearest(uv);
        // shadowWeight = 1.0;
        shadowWeight *= CalcShadowWeight(params, samplePixel, sampleParams);

        // Avoid umbra leaking inside wide penumbra
        // NRD TODO: it works surprisingly well, keep an eye on it!
        shadowWeight *= saturate(samplePixel.Penumbra * invEstimatedPenumbra); 

        const float penumbraWeight = CalcPenumbraWeight(params, shadowWeight, samplePixel);

        outShadow += float2(samplePixel.Shadow, 1.0) * shadowWeight;
        outPenumbra += float2(samplePixel.Penumbra, 1.0) * penumbraWeight;
    }

    outShadow.x /= outShadow.y;
    outPenumbra.x = outPenumbra.y == 0.0 ? params.CenterPixel.Penumbra : outPenumbra.x / outPenumbra.y;
}

[numthreads(g_GroupSizeX, g_GroupSizeY, 1)]
void CsMain(CsInput input)
{
    bool isSky = SIGMA_USE_TILE_CHECK;
    isSky = isSky && g_SmoothTilesTex[input.PixelPos >> 4].y;

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

    const uint2 sharedPos = input.ThreadPos + SIGMA_BORDER;
    const PixelData centerPixel = g_PixelsData[sharedPos.y][sharedPos.x];

    if (centerPixel.ViewDepth > sigma::g_DenoisingRange)
    {
        return;
    }

    // TODO: History copy moved to another pass?
#if defined(FIRST_BLUR_PASS)
    if (g_PassConstants.StabilizationStrength != 0.0)
    {
        g_OutHistoryTex[input.PixelPos] = g_HistoryTex[input.PixelPos];
    }
#endif

    // Tile-based early out ( potentially )
    const float2 pixelUv = (input.PixelPos + 0.5) * g_FrameConstants.InvRenderResolution;
    const float tileValue = sigma::TextureCubicX(g_SmoothTilesTex, pixelUv, g_FrameConstants.RenderResolution);

    const bool isUmbra = (SIGMA_USE_TILE_CHECK && tileValue == 0.0) || centerPixel.Penumbra == 0.0;
    if (isUmbra)
    {
        g_OutPenumbraTex[input.PixelPos] = centerPixel.Penumbra;
        g_OutShadowTex[input.PixelPos] = sigma::PackShadow(centerPixel.Shadow);

        return;
    }

    const BlurParams params = GetBlurParams(pixelUv, centerPixel);
    
    float2 blurredShadow = 0.0;
    float2 blurredPenumbra = 0.0;

#if 1
    RunDenseBlur(input, params, blurredShadow, blurredPenumbra);
#endif

#if 1
    // Avoid blurry result if penumbra size < BORDER
    const float penumbraInPixels = blurredPenumbra.x / params.WorldPixelSize;
    const float factor = smoothstep(0.0, SIGMA_BORDER, penumbraInPixels);
    blurredShadow.x = lerp(params.CenterPixel.Shadow, blurredShadow.x, factor); // TODO: not the best solution
#endif

#if 1
    // Avoid unnecessary weight increase for the unfiltered center sample if the blur radius is small
    const float f = lerp( 4.0, 1.0, factor); // TODO: adds blurriness
    blurredShadow *= f;
    blurredPenumbra *= f;
#endif

#if 1
    RunSparseBlur(params, tileValue, blurredShadow, blurredPenumbra);
#endif

#if !defined(FIRST_BLUR_PASS)
    if (g_PassConstants.StabilizationStrength != 0)
#endif
    {
        g_OutPenumbraTex[input.PixelPos] = blurredPenumbra.x;
    }

    g_OutShadowTex[input.PixelPos] = sigma::PackShadow(blurredShadow.x);
}
