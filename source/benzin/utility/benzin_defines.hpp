#pragma once

#define BenzinStringify(str) #str
#define BenzinStringify2(value) BenzinStringify(value)

#define BenzinStringConcatenate(str1, str2) str1##str2
#define BenzinStringConcatenate2(str1, str2) BenzinStringConcatenate(str1, str2)

#define BenzinUniqueVariableName(name) BenzinStringConcatenate2(name, __LINE__)

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
