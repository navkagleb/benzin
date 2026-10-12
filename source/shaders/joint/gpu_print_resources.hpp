#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct GpuPrintConsts
    {
        uint m_PrintBufferHeapIndex;
        uint m_PrintBufferSizeInBytes;
        int2 m_CursorPosition;
    };

    struct GpuPrintRecordHeader
    {
        uint m_SizeInBytes;
        uint m_ArgCount;
    };

    enum class GpuPrintArgCode : uint
    {
        Uint = 0,
        Int,
        Float,
        Bool,
        NewLine,
    };

}
