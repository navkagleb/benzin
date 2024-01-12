#pragma once

#include "rt_common.hlsli"
#include "sdf.hlsli"

namespace rt_geometry_generator
{

    enum class GeometryType : uint
    {
        AABB,
        ThreeSpheres,
        Metaballs,
        SDF_MiniSpheres,
        SDF_IntersectedRoundCube,
        SDF_SquareTorus,
        SDF_TwistedTorus,
        SDF_VerticalCappedCylinder,
        SDF_FractalPyramid,
    };
    
    struct Sphere
    {
        float3 Center;
        float Radius;
    };
    
    // AnalyticGeometry
    // - AABB
    // - ThreeSpheres
    
    bool SolveQuadraticEquation(float a, float b, float c, out float outX0, out float outX1)
    {
        // Solve a quadratic equation
        // #REFERENCE: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
        
        outX0 = 0.0f;
        outX1 = 0.0f;
        
        const float discriminant = b * b - 4 * a * c;
        
        if (discriminant < 0.0f)
        {
            return false;
        }
        
        if (discriminant == 0.0f)
        {
            outX0 = -0.5f * b / a;
            outX1 = outX0;
        }
        else
        {
            const float q = (b > 0) ? -0.5f * (b + sqrt(discriminant)) : -0.5f * (b - sqrt(discriminant));
            outX0 = q / a;
            outX1 = c / q;
        }
        
        if (outX0 > outX1)
        {
            common::Swap(outX0, outX1);
        }

        return true;
    }

    bool SolveRaySphereIntersectionEquation(rt_common::Ray ray, Sphere sphere, out float outTMin, out float outTMax)
    {
        // Analytic solution of an unbounded ray sphere intersection points
        // #REFERENCE: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
        
        const float3 l = ray.Origin - sphere.Center;
        
        const float a = dot(ray.Direction, ray.Direction);
        const float b = 2 * dot(ray.Direction, l);
        const float c = dot(l, l) - sphere.Radius * sphere.Radius;
        
        return SolveQuadraticEquation(a, b, c, outTMin, outTMax);
    }
    
    float3 CalculateNormalForRaySphereHit(rt_common::Ray ray, float t, float3 sphereCenter)
    {
        const float3 hitPosition = ray.Origin + t * ray.Direction;
        const float3 normal = normalize(hitPosition - sphereCenter);
        
        return normal;
    }

    bool RaySphereIntersectionTest(rt_common::Ray ray, Sphere sphere, out float outTHit, out float outTMax, out float3 outNormal)
    {
        // Test if a ray with RayFlags and segment <RayTMin(), RayTCurrent()> intersects a hollow sphere

        outTHit = 0.0f;
        outTMax = 0.0f;
        
        // Solutions for t if the ray intersects
        float t0 = 0.0f;
        float t1 = 0.0f;

        if (!SolveRaySphereIntersectionEquation(ray, sphere, t0, t1))
        {
            return false;
        }
        
        outTMax = t1;

        if (t0 < RayTMin())
        {
            // t0 is before RayTMin, let's use t1 instead
            
            if (t1 < RayTMin())
            {
                // Both t0 and t1 are before RayTMin
                return false;
            }

            outNormal = CalculateNormalForRaySphereHit(ray, t1, sphere.Center);
            
            if (rt_common::IsValidHit(ray, t1, outNormal))
            {
                outTHit = t1;
                return true;
            }
        }
        else
        {
            outNormal = CalculateNormalForRaySphereHit(ray, t0, sphere.Center);
            if (rt_common::IsValidHit(ray, t0, outNormal))
            {
                outTHit = t0;
                return true;
            }

            outNormal = CalculateNormalForRaySphereHit(ray, t1, sphere.Center);
            if (rt_common::IsValidHit(ray, t1, outNormal))
            {
                outTHit = t1;
                return true;
            }
        }
        
        return false;
    }

    bool RayThreeSpheresIntersectionTest(rt_common::Ray ray, out float outTHit, out float3 outNormal)
    {
        const uint sphereCount = 3;
        const Sphere spheres[sphereCount] =
        {
            { float3(-0.3f, -0.3f, -0.3f), 0.6f },
            { float3(0.1f, 0.1f, 0.4f), 0.3f },
            { float3(0.35f, 0.35f, 0.0f), 0.15f },
        };
        
        bool hitFound = false;

        // Test for intersection against all spheres and take the closest hit.
        outTHit = RayTCurrent();

        for (uint i = 0; i < sphereCount; ++i)
        {
            float sphereTHit = 0.0f;
            float sphereTMax = 0.0f;
            float3 sphereNormal = 0.0f;
            if (RaySphereIntersectionTest(ray, spheres[i], sphereTHit, sphereTMax, sphereNormal))
            {
                if (sphereTHit < outTHit)
                {
                    outTHit = sphereTHit;
                    outNormal = sphereNormal;
                    
                    hitFound = true;
                }
            }
        }
        
        return hitFound;
    }
    
    bool RayAABBIntersectionTest(rt_common::Ray ray, float3 aabb[2], out float outTMin, out float outTMax)
    {
        // Test if a ray segment <RayTMin(), RayTCurrent()> intersects an AABB.
        // Limitation: this test does not take RayFlags into consideration and does not calculate a surface normal.
        // Ref: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
        
        const uint3 sign3 = ray.Direction > 0.0f;
        
        float3 tmin3;
        float3 tmax3;

        // Handle rays parallel to any x|y|z slabs of the AABB.
        // If a ray is within the parallel slabs, 
        //  the tmin, tmax will get set to -inf and +inf
        //  which will get ignored on tmin/tmax = max/min.
        // If a ray is outside the parallel slabs, -inf/+inf will
        //  make tmax > tmin fail (i.e. no intersection).
        // TODO: handle cases where ray origin is within a slab 
        //  that a ray direction is parallel to. In that case
        //  0 * INF => NaN
        float3 invRayDirection;
        if (all(ray.Direction != 0.0f))
        {
            invRayDirection = 1.0f / ray.Direction;
        }
        else
        {
            invRayDirection = all(ray.Direction > 0.0f) ? common::g_FloatInfinity : -common::g_FloatInfinity;
        }

        tmin3.x = (aabb[1 - sign3.x].x - ray.Origin.x) * invRayDirection.x;
        tmax3.x = (aabb[sign3.x].x - ray.Origin.x) * invRayDirection.x;

        tmin3.y = (aabb[1 - sign3.y].y - ray.Origin.y) * invRayDirection.y;
        tmax3.y = (aabb[sign3.y].y - ray.Origin.y) * invRayDirection.y;
    
        tmin3.z = (aabb[1 - sign3.z].z - ray.Origin.z) * invRayDirection.z;
        tmax3.z = (aabb[sign3.z].z - ray.Origin.z) * invRayDirection.z;
    
        outTMin = max(max(tmin3.x, tmin3.y), tmin3.z);
        outTMax = min(min(tmax3.x, tmax3.y), tmax3.z);
    
        return outTMax > outTMin && outTMax >= RayTMin() && outTMin <= RayTCurrent();
    }

    bool RayAABBIntersectionTest(rt_common::Ray ray, float3 aabb[2], out float outTHit, out float3 outNormal)
    {
        // Test if a ray with RayFlags and segment <RayTMin(), RayTCurrent()> intersects a hollow AABB.

        outTHit = 0.0f;
        outNormal = 0.0f;

        float tmin;
        float tmax;
        if (!RayAABBIntersectionTest(ray, aabb, tmin, tmax))
        {
            return false;
        }
        
        // Only consider intersections crossing the surface from the outside.
        if (tmin < RayTMin() || tmin > RayTCurrent())
        {   
            return false;
        }

        outTHit = tmin;

        // Set a normal to the normal of a face the hit point lays on.
        const float3 hitPosition = rt_common::GetHitPosition(ray, outTHit);
            
        const float3 distanceToBounds[2] =
        {
            abs(aabb[0] - hitPosition),
            abs(aabb[1] - hitPosition),
        };
            
        const float eps = 0.0001;
            
        if (distanceToBounds[0].x < eps)
        {
            outNormal = float3(-1.0f, 0.0f, 0.0f);
        }
        else if (distanceToBounds[0].y < eps)
        {
            outNormal = float3(0.0f, -1.0f, 0.0f);
        }
        else if (distanceToBounds[0].z < eps)
        {
            outNormal = float3(0.0f, 0.0f, -1.0f);
        }
        else if (distanceToBounds[1].x < eps)
        {
            outNormal = float3(1.0f, 0.0f, 0.0f);
        }
        else if (distanceToBounds[1].y < eps)
        {
            outNormal = float3(0.0f, 1.0f, 0.0f);
        }
        else if (distanceToBounds[1].z < eps)
        {
            outNormal = float3(0.0f, 0.0f, 1.0f);
        }

        return rt_common::IsValidHit(ray, outTHit, outNormal);
    }

#if 1
    // VolumetricGeometry
    // - Metaballs

// 3 || 5
#define METABALL_COUNT 3
    
    float CalculateMetaballPotential(float3 position, Sphere blob, out float distance)
    {
        // Calculate a magnitude of an influence from a Metaball charge.
        // Return metaball potential range: <0,1>
        // mbRadius - largest possible area of metaball contribution - AKA its bounding sphere.
        
        distance = length(position - blob.Center);
    
        if (distance <= blob.Radius)
        {
            float d = distance;

            // Quintic polynomial field function.
            // The advantage of this polynomial is having smooth second derivative. Not having a smooth
            // second derivative may result in a sharp and visually unpleasant normal vector jump.
            // The field function should return 1 at distance 0 from a center, and 1 at radius distance,
            // but this one gives f(0) = 0, f(radius) = 1, so we use the distance to radius instead.
            d = blob.Radius - d;

            float r = blob.Radius;
            return 6 * (d * d * d * d * d) / (r * r * r * r * r)
            - 15 * (d * d * d * d) / (r * r * r * r)
            + 10 * (d * d * d) / (r * r * r);
        }
        
        return 0.0f;
    }

    float CalculateMetaballsPotential(float3 position, Sphere blobs[METABALL_COUNT], uint activeMetaballCount)
    {
        float sumFieldPotential = 0.0f;
#if USE_DYNAMIC_LOOPS 
    for (UINT j = 0; j < nActiveMetaballs; j++)
#else
        for (uint i = 0; i < METABALL_COUNT; ++i)
#endif
        {
            float dummy;
            sumFieldPotential += CalculateMetaballPotential(position, blobs[i], dummy);
        }
        
        return sumFieldPotential;
    }

    float3 CalculateMetaballsNormal(float3 position, Sphere blobs[METABALL_COUNT], uint activeMetaballCount)
    {
        // Calculate a normal via central differences.

        const float e = 0.5773 * 0.00001;

        return normalize(float3(
            CalculateMetaballsPotential(position + float3(-e, 0.0f, 0.0f), blobs, activeMetaballCount) - CalculateMetaballsPotential(position + float3(e, 0.0f, 0.0f), blobs, activeMetaballCount),
            CalculateMetaballsPotential(position + float3(0.0f, -e, 0.0f), blobs, activeMetaballCount) - CalculateMetaballsPotential(position + float3(0.0f, e, 0.0f), blobs, activeMetaballCount),
            CalculateMetaballsPotential(position + float3(0.0f, 0.0f, -e), blobs, activeMetaballCount) - CalculateMetaballsPotential(position + float3(0.0f, 0.0f, e), blobs, activeMetaballCount)
        ));
    }

    float CalculateAnimationInterpolant(float elapsedTime, float cycleDuration)
    {
        // Returns a cycling <0 -> 1 -> 0> animation interpolant 

        float currentLinearCycleTime = fmod(elapsedTime, cycleDuration) / cycleDuration;
        currentLinearCycleTime = (currentLinearCycleTime <= 0.5f) ? 2 * currentLinearCycleTime : 1.0f - 2.0f * (currentLinearCycleTime - 0.5f);
        
        return smoothstep(0.0f, 1.0f, currentLinearCycleTime);
    }
    
    void InitializeAnimatedMetaballs(float elapsedTime, float cycleDuration, out Sphere blobs[METABALL_COUNT])
    {
        // Metaball centers at t0 and t1 key frames
        
#if METABALL_COUNT == 3
        const float radiuses[METABALL_COUNT] = { 0.45, 0.55, 0.45 };

        const float3 centers[METABALL_COUNT][2] =
        {
            { float3(-0.3f, -0.3f, -0.4f), float3(0.3f, -0.3f, -0.0f) },
            { float3(0.0f, -0.2f, 0.5f), float3(0.0f, 0.4f, 0.5f) },
            { float3(0.4f, 0.4f, 0.4f), float3(-0.4f, 0.2f, -0.4f) },
        };
#else
        const float radiuses[METABALL_COUNT] = { 0.35, 0.35, 0.35, 0.35, 0.25 };
        
        float3 centers[METABALL_COUNT][2] =
        {
            { float3(-0.7, 0, 0),float3(0.7,0, 0) },
            { float3(0.7 , 0, 0), float3(-0.7, 0, 0) },
            { float3(0, -0.7, 0),float3(0, 0.7, 0) },
            { float3(0, 0.7, 0), float3(0, -0.7, 0) },
            { float3(0, 0, 0),   float3(0, 0, 0) }
        };
#endif

        const float animationTimePoint = CalculateAnimationInterpolant(elapsedTime, cycleDuration);
        
        for (uint i = 0; i < METABALL_COUNT; ++i)
        {
            blobs[i].Center = lerp(centers[i][0], centers[i][1], animationTimePoint);
            blobs[i].Radius = radiuses[i];
        }
    }

    bool RaySolidSphereIntersectionTest(rt_common::Ray ray, Sphere sphere, out float outTHit, out float outTMax)
    {
        // Test if a ray segment <RayTMin(), RayTCurrent()> intersects a solid sphere
        // Limitation: this test does not take RayFlags into consideration and does not calculate a surface normal

        outTHit = 0.0f;
        outTMax = 0.0f;

        float tMin = 0.0f;
        float tMax = 0.0f;

        if (!SolveRaySphereIntersectionEquation(ray, sphere, tMin, tMax))
        {
            return false;
        }

        // Since it's a solid sphere, clip intersection points to ray extents
        outTHit = max(tMin, RayTMin());
        outTMax = min(tMax, RayTCurrent());

        return true;
    }
    
    void FindIntersectingMetaballs(rt_common::Ray ray, inout Sphere blobs[METABALL_COUNT], out float outTMin, out float outTMax, out uint activeMetaballCount)
    {
        // Find all metaballs that ray intersects
        // The passed in array is sorted to the first activeMetaballCount
        
        outTMin = common::g_FloatInfinity;
        outTMax = -common::g_FloatInfinity;
        activeMetaballCount = 0;

        for (uint i = 0; i < METABALL_COUNT; ++i)
        {
            float blobTHit = 0.0f;
            float blobTMax = 0.0f;
            if (RaySolidSphereIntersectionTest(ray, blobs[i], blobTHit, blobTMax))
            {
                outTMin = min(outTMin, blobTHit);
                outTMax = max(outTMax, blobTMax);
                
#if LIMIT_TO_ACTIVE_METABALLS
                blobs[activeMetaballCount++] = blobs[i];
#else
                activeMetaballCount = METABALL_COUNT;
#endif
            }
        }
        
        outTMin = max(outTMin, RayTMin());
        outTMax = min(outTMax, RayTCurrent());
    }

    bool RayMetaballsIntersectionTest(rt_common::Ray ray, float elapsedTime, out float outTHit, out float3 outNormal)
    {
        // Test if a ray with RayFlags and segment <RayTMin(), RayTCurrent()> intersects metaball field
        // The test sphere traces through the metaball field until it hits a threshold isosurface
        
        outTHit = 0.0f;
        outNormal = 0.0f;
        
        // Field potential threshold defining the isosurface
        // Threshold - valid range is (0, 1>, the larger the threshold the smaller the blob
        const float threshold = 0.25f;
        
        Sphere blobs[METABALL_COUNT];
        InitializeAnimatedMetaballs(elapsedTime, 12.0f, blobs);
    
        float tMin = 0.0f;
        float tMax = 0.0f;
        uint activeMetaballCount = 0;
        FindIntersectingMetaballs(ray, blobs, tMin, tMax, activeMetaballCount);

        const uint maxStepCount = 128;
        const float tStep = (tMax - tMin) / maxStepCount;
        
        uint stepIndex = 0;
        float currentT = tMin;

        while (stepIndex++ < maxStepCount)
        {
            const float3 position = rt_common::GetHitPosition(ray, currentT);
            
            float fieldPotentials[METABALL_COUNT];
            float sumFieldPotential = 0.0f;
            
        // Calculate field potentials from all metaballs.
#if USE_DYNAMIC_LOOPS
        for (UINT j = 0; j < nActiveMetaballs; j++)
#else
            for (uint i = 0; i < METABALL_COUNT; ++i)
#endif
            {
                float distance;
                fieldPotentials[i] = CalculateMetaballPotential(position, blobs[i], distance);
                sumFieldPotential += fieldPotentials[i];
            }

            // Have we crossed the isosurface?
            if (sumFieldPotential >= threshold)
            {
                const float3 normal = CalculateMetaballsNormal(position, blobs, activeMetaballCount);
                
                if (rt_common::IsValidHit(ray, currentT, normal))
                {
                    outTHit = currentT;
                    outNormal = normal;
                    
                    return true;
                }
            }
            
            currentT += tStep;
        }

        return false;
    }
#endif

    // Signed Distance Fields
    
    float GetDistanceFromSignedDistancePrimitive(float3 position, GeometryType geometryType)
    {
        switch (geometryType)
        {
            case GeometryType::SDF_MiniSpheres:
            {
                return sdf::Intersect(
                    sdf::Sphere(sdf::RepeatUnlimetly(position + 0.25f, 0.5f), 0.65f / 4.0f),
                    sdf::Box(position, 1.0f)
                );
            }
            case GeometryType::SDF_IntersectedRoundCube:
            {
                return sdf::Substract(
                    sdf::Substract(sdf::RoundBox(position, 0.75f, 0.2f), sdf::Sphere(position, 1.20f)),
                    -sdf::Sphere(position, 1.32f)
                );
            }
            case GeometryType::SDF_SquareTorus:
            {
                return sdf::Torus82(position, float2(0.7f, 0.15f));
            }
            case GeometryType::SDF_TwistedTorus:
            {
                return sdf::Box(sdf::TwistY(position, 0.5f), 0.5f);
            }
            case GeometryType::SDF_VerticalCappedCylinder:
            {
                return sdf::VerticalCappedCylinder(position, 1.0f, 0.5f);
            }
            case GeometryType::SDF_FractalPyramid:
            {
                const float angleInDegrees = 63.38027046f;
                const float height = 2.0f;
                const float iterationCount = 4;
                const float scale = 2.0f;
                
                return sdf::FractalPyramid(
                    position + float3(0.0f, 1.0f, 0.0f),
                    common::DegreesToRadians(angleInDegrees),
                    height,
                    iterationCount,
                    scale
                );
            }
            default:
            {
                break;
            }
        }
        
        return 0.0;
    }
    
    float3 CalculateNormal(float3 position, GeometryType geometryType)
    {
        const float epsilon = 0.001f;
        const float2 e = float2(1.0f, -1.0f) * 0.5773f;
        
        return normalize(
            e.xyy * GetDistanceFromSignedDistancePrimitive(position + e.xyy * epsilon, geometryType) +
            e.yyx * GetDistanceFromSignedDistancePrimitive(position + e.yyx * epsilon, geometryType) +
            e.yxy * GetDistanceFromSignedDistancePrimitive(position + e.yxy * epsilon, geometryType) +
            e.xxx * GetDistanceFromSignedDistancePrimitive(position + e.xxx * epsilon, geometryType)
        );
    }


    bool RaySignedDistancePrimitiveTest(rt_common::Ray ray, GeometryType geometryType, out float outTHit, out float3 outNormal)
    {
        const uint maxStepCount = 512;
        
        const float threshold = 0.001f;
        const float stepScale = 1.0f; // #HARDCODE

        float t = RayTMin();

        // Do sphere tracing through the AABB
        uint stepIndex = 0;
        while (stepIndex++ < maxStepCount && t <= RayTCurrent())
        {
            const float3 position = rt_common::GetHitPosition(ray, t);
            const float distance = GetDistanceFromSignedDistancePrimitive(position, geometryType);

            // Has the ray intersected the primitive? 
            if (distance <= threshold * t)
            {
                const float3 normal = CalculateNormal(position, geometryType);
                if (rt_common::IsValidHit(ray, t, normal))
                {
                    outTHit = t;
                    outNormal = normal;
                    return true;
                }
            }

            // Since distance is the minimum distance to the primitive, 
            // we can safely jump by that amount without intersecting the primitive.
            // We allow for scaling of steps per primitive type due to any pre-applied 
            // transformations that don't preserve true distances.
            t += stepScale * distance;
        }

        outTHit = 0.0f;
        outNormal = 0.0f;
        return false;
    }
    
    bool IsIntersectionTestPassed(rt_common::Ray ray, GeometryType geometryType, float elapsedTime, out float outTHit, out float3 outNormal)
    {
        // Analytic geometry intersection test.
        // AABB local space dimensions: <-1,1>.

        outTHit = 0.0f;
        outNormal = 0.0f;
        
        // #NOTE: Make sure that aabb with is == 2
        const float3 aabbMin = -1.0f;
        const float3 aabbMax = 1.0f;
        
        const float3 unitAABB[2] = { aabbMin, aabbMax };
        
        switch (geometryType)
        {
            case GeometryType::AABB: return RayAABBIntersectionTest(ray, unitAABB, outTHit, outNormal);
            case GeometryType::ThreeSpheres: return RayThreeSpheresIntersectionTest(ray, outTHit, outNormal);
            case GeometryType::Metaballs: return RayMetaballsIntersectionTest(ray, elapsedTime, outTHit, outNormal);
            case GeometryType::SDF_MiniSpheres:
            case GeometryType::SDF_IntersectedRoundCube:
            case GeometryType::SDF_SquareTorus:
            case GeometryType::SDF_TwistedTorus:
            case GeometryType::SDF_VerticalCappedCylinder:
            case GeometryType::SDF_FractalPyramid:
                return RaySignedDistancePrimitiveTest(ray, geometryType, outTHit, outNormal);
            default: break;
        }
        
        return false;
    }
    
} // namespace rt_geometry_generator
