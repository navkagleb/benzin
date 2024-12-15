#ifdef SIGMA_USE_BORDER_2
    #define SIGMA_BORDER 2
#else
    #define SIGMA_BORDER 1
#endif

#define SIGMA_USE_TILE_CHECK 1
#define SIGMA_MAX_BLUR_KERNEL_PIXEL_RADIUS 32

#define SIGMA_BLUR_USE_ANISOTROPIC_BLUR 1

#define SIGMA_TS_USE_EARLY_OUT 1
#define SIGMA_TS_MAX_HISTORY_LENGTH 7
#define SIGMA_TS_NORM_DISOCCLUSION_THRESHOLD 0.02 // normalized % // TODO: use CommonSettings::disocclusionThreshold?
#define SIGMA_TS_SIGMA_SCALE 3.0

namespace sigma
{

    static const float g_DenoisingRange = 500000.0;

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
