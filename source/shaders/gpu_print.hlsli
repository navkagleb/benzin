#pragma once

// Ref: Shader Printf in HLSL and DX12 - https://therealmjp.github.io/posts/hlsl-printf/

#include "common.hlsli"
#include "joint/gpu_print_resources.hpp"

struct GpuPrinter
{
    static const uint ms_BufferSizeInBytes = 32 * sizeof(uint); // 128 bytes per print

    uint m_RawBytes[ms_BufferSizeInBytes / sizeof(uint)];
    uint m_SizeInBytes;
    uint m_ArgCount;

    void Init()
    {
        for (uint i = 0; i < ms_BufferSizeInBytes / sizeof(uint); ++i)
            m_RawBytes[i] = 0;

        m_SizeInBytes = 0;
        m_ArgCount = 0;
    }

    void PushByte(uint c)
    {
        if (m_SizeInBytes >= ms_BufferSizeInBytes)
            return;

        const uint bufferPosition = m_SizeInBytes / 4;
        const uint shift = (m_SizeInBytes % 4) * 8;

        m_RawBytes[bufferPosition] |= ((c & 0xFF) << shift);
        ++m_SizeInBytes;
    }

    template <typename T, uint ComponentCount>
    void PushArgWithCode(joint::GpuPrintArgCode argCode, T arg[ComponentCount])
    {
        if (m_SizeInBytes + sizeof(arg) > ms_BufferSizeInBytes)
            return;

        PushByte((uint)argCode);
        PushByte((uint)ComponentCount);

        for (uint componentIndex = 0; componentIndex < ComponentCount; ++componentIndex)
        {
            for (uint byteIndex = 0; byteIndex < sizeof(T); ++byteIndex)
            {
                PushByte(asuint(arg[componentIndex]) >> (byteIndex * 8)); // little-endian order
            }
        }

        ++m_ArgCount;
    }

    // uint
    void PushArg(uint arg)
    {
        const uint args[1] = { arg };
        PushArgWithCode(joint::GpuPrintArgCode::Uint, args);
    }

    void PushArg(uint2 arg)
    {
        const uint args[2] = { arg.x, arg.y };
        PushArgWithCode(joint::GpuPrintArgCode::Uint, args);
    }

    void PushArg(uint3 arg)
    {
        const uint args[3] = { arg.x, arg.y, arg.z };
        PushArgWithCode(joint::GpuPrintArgCode::Uint, args);
    }

    void PushArg(uint4 arg)
    {
        const uint args[4] = { arg.x, arg.y, arg.z, arg.w };
        PushArgWithCode(joint::GpuPrintArgCode::Uint, args);
    }

    // int
    void PushArg(int arg)
    {
        const int args[1] = { arg };
        PushArgWithCode(joint::GpuPrintArgCode::Int, args);
    }

    void PushArg(int2 arg)
    {
        const int args[2] = { arg.x, arg.y };
        PushArgWithCode(joint::GpuPrintArgCode::Int, args);
    }

    void PushArg(int3 arg)
    {
        const int args[3] = { arg.x, arg.y, arg.z };
        PushArgWithCode(joint::GpuPrintArgCode::Int, args);
    }

    void PushArg(int4 arg)
    {
        const int args[4] = { arg.x, arg.y, arg.z, arg.w };
        PushArgWithCode(joint::GpuPrintArgCode::Int, args);
    }

    // float
    void PushArg(float arg)
    {
        const float args[1] = { arg };
        PushArgWithCode(joint::GpuPrintArgCode::Float, args);
    }

    void PushArg(float2 arg)
    {
        const float args[2] = { arg.x, arg.y };
        PushArgWithCode(joint::GpuPrintArgCode::Float, args);
    }

    void PushArg(float3 arg)
    {
        const float args[3] = { arg.x, arg.y, arg.z };
        PushArgWithCode(joint::GpuPrintArgCode::Float, args);
    }

    void PushArg(float4 arg)
    {
        const float args[4] = { arg.x, arg.y, arg.z, arg.w };
        PushArgWithCode(joint::GpuPrintArgCode::Float, args);
    }

    // bool
    void PushArg(bool arg)
    {
        const uint args[1] = { arg };
        PushArgWithCode(joint::GpuPrintArgCode::Bool, args);
    }

    // Non-typed arg codes
    void PushArg(joint::GpuPrintArgCode arg)
    {
        PushByte((uint)arg);
        ++m_ArgCount;
    }

    // Args
    template<typename T0>
    void PushArgs(T0 arg0)
    {
        PushArg(arg0);
    }

    template<typename T0, typename T1>
    void PushArgs(T0 arg0, T1 arg1)
    {
        PushArg(arg0);
        PushArg(arg1);
    }

    template <typename T0, typename T1, typename T2>
    void PushArgs(T0 arg0, T1 arg1, T2 arg2)
    {
        PushArg(arg0);
        PushArg(arg1);
        PushArg(arg2);
    }

    void Commit()
    {
        m_SizeInBytes = RoundUp(m_SizeInBytes, sizeof(uint)); // Round up to the next multiple of 4 since we work with 4-byte alignment for each print

        // Increment the atomic counter to allocate space to store the bytes
        const uint writeSizeInBytes = m_SizeInBytes + sizeof(joint::GpuPrintRecordHeader);

        RWByteAddressBuffer gpuPrintBuffer = ResourceDescriptorHeap[g_GpuPrintConsts.m_PrintBufferHeapIndex];

        uint offsetInBytes = 0;
        gpuPrintBuffer.InterlockedAdd(0, writeSizeInBytes, offsetInBytes);

        offsetInBytes += sizeof(uint); // Account for the atomic counter at the beginning of the buffer

        if (offsetInBytes + writeSizeInBytes > g_GpuPrintConsts.m_PrintBufferSizeInBytes)
            return;

        joint::GpuPrintRecordHeader header;
        header.m_SizeInBytes = m_SizeInBytes;
        header.m_ArgCount = m_ArgCount;

        gpuPrintBuffer.Store<joint::GpuPrintRecordHeader>(offsetInBytes, header);
        offsetInBytes += sizeof(joint::GpuPrintRecordHeader);

        for (uint i = 0; i < m_SizeInBytes / sizeof(uint); ++i)
        {
            gpuPrintBuffer.Store(offsetInBytes, m_RawBytes[i]);
            offsetInBytes += sizeof(uint);
        }
    }
};

struct GpuPrintContext
{
    static uint2 ms_PixelPosition;
};

uint2 GpuPrintContext::ms_PixelPosition = 0;

#define BENZIN_GPU_PRINT_ENABLED 1

#if BENZIN_GPU_PRINT_ENABLED
    #define BenzinGpuPrintSetFilter(pixelPosition) \
        GpuPrintContext::ms_PixelPosition = (uint2)pixelPosition

    #define BenzinGpuPrint(...) \
        do { \
            if (all(g_GpuPrintConsts.m_CursorPosition == GpuPrintContext::ms_PixelPosition)) \
            { \
                GpuPrinter printer; \
                printer.Init(); \
                printer.PushArgs(__VA_ARGS__); \
                printer.Commit(); \
            } \
        } while (0)
#else
    #define BenzinGpuPrintSetFilter(pixelPosition)
    #define BenzinGpuPrint(...)
#endif
