#pragma once

#ifdef SIGMA_USE_BORDER_2
    #define SIGMA_BORDER 2
#else
    #define SIGMA_BORDER 1
#endif

#define SIGMA_USE_TILE_CHECK 1
#define SIGMA_DENOISING_RANGE 500000.0
#define SIGMA_MAX_BLUR_KERNEL_PIXEL_RADIUS 32

#define SIGMA_BLUR_USE_ANISOTROPIC_BLUR 1
#define SIGMA_BLUR_POISSON_SAMPLE_COUNT 8
#define SIGMA_BLUR_POISSON_SAMPLES sigma::g_PoissonSamples

#define SIGMA_TS_USE_EARLY_OUT 1
#define SIGMA_TS_MAX_HISTORY_LENGTH 7
#define SIGMA_TS_NORM_DISOCCLUSION_THRESHOLD 0.02 // normalized % // TODO: use CommonSettings::disocclusionThreshold?
#define SIGMA_TS_SIGMA_SCALE 3.0

namespace sigma
{

    // Ref: https://www.desmos.com/calculator/abaqyvswem
    static const float3 g_PoissonSamples[SIGMA_BLUR_POISSON_SAMPLE_COUNT] =
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
