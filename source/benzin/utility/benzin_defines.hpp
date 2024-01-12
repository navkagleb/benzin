#pragma once

#define BenzinDefineFunctionAlias(To, From) \
    inline decltype(auto) To(auto&&... args) \
    { \
        return From(std::forward<decltype(args)>(args)...); \
    }

#define BenzinStringConcatenate(string1, string2) string1##string2
#define BenzinStringConcatenate2(string1, string2) BenzinStringConcatenate(string1, string2)

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
