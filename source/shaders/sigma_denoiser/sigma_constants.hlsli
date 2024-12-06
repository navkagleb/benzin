#pragma once

#define SIGMA_USE_TILE_CHECK 1
#define SIGMA_USE_SPARSE_BLUR 1

namespace sigma
{

    static const float g_DenoisingRange = 500000.0;

    static const float g_MaxKernelPixelRadius = 32.0;

    // (normalized %) - represents maximum allowed deviation from local tangent plane
    static const float g_PlaneDistanceSensitivity = 0.005;

    // TemporalStabilization
    static const float g_PenumbraWeightScale = 10.0;
    static const uint g_PoissonSampleCount = 8;

    // Ref: https://www.desmos.com/calculator/abaqyvswem
    static const float3 g_PoissonSamples[g_PoissonSampleCount] =
    {
        float3(-1.00, 0.00, 1.0),
        float3(0.00, 1.00, 1.0),
        float3(1.00, 0.00, 1.0),
        float3(0.00, -1.00, 1.0),
        float3(-0.25 * sqrt(2.0), 0.25 * sqrt(2.0), 0.5),
        float3(0.25 * sqrt(2.0), 0.25 * sqrt(2.0), 0.5),
        float3(0.25 * sqrt(2.0), -0.25 * sqrt(2.0), 0.5),
        float3(-0.25 * sqrt(2.0), -0.25 * sqrt(2.0), 0.5),
    };
    
}
