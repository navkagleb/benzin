#pragma once

#define BenzinDefineNonCopyable(ClassName) \
    ClassName(const ClassName& other) = delete; \
    ClassName& operator =(const ClassName& other) = delete

#define BenzinDefineNonMoveable(ClassName) \
    ClassName(ClassName&& other) = delete; \
    ClassName& operator =(ClassName&& other) = delete

#define BenzinDefineNonConstructable(ClassName) \
    ClassName() = delete; \
    BenzinDefineNonCopyable(ClassName); \
    BenzinDefineNonMoveable(ClassName)
