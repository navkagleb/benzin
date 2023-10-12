#pragma once

namespace cube_helper
{

    float3 GetCubeUV(float2 faceUV, uint32_t faceIndex)
    {
        const float2 transformedFaceUV = 2.0f * float2(faceUV.x, 1.0f - faceUV.y) - 1.0f;

        float3 cubeUV = 0.0f;
        switch (faceIndex)
        {
            case 0:
            {
                cubeUV = float3(1.0, transformedFaceUV.y, -transformedFaceUV.x);
                break;
            }
            case 1:
            {
                cubeUV = float3(-1.0, transformedFaceUV.y, transformedFaceUV.x);
                break;
            }
            case 2:
            {
                cubeUV = float3(transformedFaceUV.x, 1.0, -transformedFaceUV.y);
                break;
            }
            case 3:
            {
                cubeUV = float3(transformedFaceUV.x, -1.0, transformedFaceUV.y);
                break;
            }
            case 4:
            {
                cubeUV = float3(transformedFaceUV.x, transformedFaceUV.y, 1.0);
                break;
            }
            case 5:
            {
                cubeUV = float3(-transformedFaceUV.x, transformedFaceUV.y, -1.0);
                break;
            }
        }

        return cubeUV;
    }

} // namespace cube_helper
