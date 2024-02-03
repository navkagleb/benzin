#pragma once

#include "common.hlsli"

namespace sdf
{

    // Helpers

    float LengthToPow2(float3 p)
    {
        return dot(p, p);
    }

    float LengthToPowNegative8(float2 p)
    {
        p = p * p;
        p = p * p;
        p = p * p;
        
        return pow(p.x + p.y, 1.0f / 8.0f);
    }
    
    // Operations

    float3 RepeatUnlimetly(float3 position, float3 spacing)
    {
        return position - spacing * round(position / spacing);
    }
    
    float Intersect(float distance1, float distance2)
    {
        return max(distance1, distance2);
    }
    
    float Substract(float distance1, float distance2)
    {
        return max(distance1, -distance2);
    }

    float3 TwistY(float3 position, float scale = 1.0f)
    {
        const float cos_ = cos(position.y * scale);
        const float sin_ = sin(position.y * scale);
        const float2x2 rotationMatrix = float2x2(cos_, -sin_, sin_, cos_);
        
        return float3(mul(position.xz, rotationMatrix), position.y);
    }
    
    // Primitives
    
    float Sphere(float3 position, float sphereRadius)
    {
        return length(position) - sphereRadius;
    }

    float Box(float3 position, float3 box)
    {
        const float3 q = abs(position) - box;
        return length(max(q, 0.0f)) + min(0.0f, max(q.x, max(q.y, q.z)));
    }

    float RoundBox(float3 position, float3 box, float radius)
    {
        return length(max(0.0f, abs(position) - box)) - radius;
    }

    float Torus(float3 position, float2 t)
    {
        const float2 q = float2(length(position.xz) - t.x, position.y);
        return length(q) - t.y;
    }

    float Torus82(float3 position, float2 radius2)
    {
        const float2 q = float2(length(position.xz) - radius2.x, position.y);
        return LengthToPowNegative8(q) - radius2.y;
    }

    float VerticalCappedCylinder(float3 position, float height, float radius)
    {
        const float2 d = abs(float2(length(position.xz), position.y)) - float2(radius, height);
        return min(0.0f, max(d.x, d.y)) + length(max(0.0f, d));
    }

    float Octahedron(float3 position, float angleInRadians, float height)
    {
        // angleInRadians = pyramid's inner angle between its side plane and a ground plane.
        // Octahedron position - ground plane intersecting in the middle.
        
        const float angleSin = sin(angleInRadians);
        const float angleCos = cos(angleInRadians);

        const float distanceToSideFromOrigin = dot(
            float2(max(abs(position.x), abs(position.z)), abs(position.y)),
            float2(angleSin, angleCos)
        );

        // Subtract distance to a side when at height from the origin
        return distanceToSideFromOrigin - angleCos * height;
    }

    float Pyramid(float3 position, float angleInRadians, float height)
    {
        // angleInRadians = pyramid's inner angle between its side plane and a ground plane
        // Pyramid position - sitting on a ground plane
        
        const float octahedron = Octahedron(position, angleInRadians, height);
        const float octahedronTopHalf = Substract(octahedron, position.y);
        
        return octahedronTopHalf;
    }

    // Returns a signed distance to a recursive pyramid fractal.
// h = { sin a, cos a, height of a pyramid}.
// a = pyramid's inner angle between its side plane and a ground plane.
// Pyramid position - sitting on a ground plane.
// Pyramid span: {<-a,0,-a>, <a,h.z,a>}, where a = width of base = h.z * h.y / h.x.
// More info here http://blog.hvidtfeldts.net/index.php/2011/08/distance-estimated-3d-fractals-iii-folding-space/
    float FractalPyramid(float3 position, float angleInRadians, float height, uint iterationCount, float scale = 2.0f)
    {
        const float angleSin = sin(angleInRadians);
        const float angleCos = cos(angleInRadians);
        
        // Set pyramid vertices to AABB's extremities
        const float halfBottomEdge = height * angleCos / angleSin;

        const uint pyramidVertexCount = 5;
        const float3 pyramidVertices[pyramidVertexCount] =
        {
            float3(0.0f, height, 0.0f),
            float3(-halfBottomEdge, 0.0f, halfBottomEdge),
            float3(halfBottomEdge, 0.0f, -halfBottomEdge),
            float3(halfBottomEdge, 0.0f, halfBottomEdge),
            float3(-halfBottomEdge, 0.0f, -halfBottomEdge),
        };
        
        for (uint iterationIndex = 0; iterationIndex < iterationCount; ++iterationIndex)
        {
            float minDistance = g_FloatInfinity;
            uint minVertexIndex;

            for (uint i = 0; i < pyramidVertexCount; ++i)
            {
                const float distance = LengthToPow2(position - pyramidVertices[i]);
                if (minDistance > distance)
                {
                    minDistance = distance;
                    minVertexIndex = i;
                }
            }

            // Update to a relative position in the current fractal iteration
            position = scale * position - pyramidVertices[minVertexIndex] * (scale - 1.0f);
        }
        
        const float pyramid = Pyramid(position, angleInRadians, height);

        // Convert the distance from within a fractal iteration to the object space
        return pyramid * pow(scale, -(float)iterationCount);
    }
    
} // namespace sdf
