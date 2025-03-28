#pragma once

namespace benzin
{

    bool Assert(std::string_view conditionString, const std::source_location& sourceLocation, std::string_view message1 = {}, std::string_view messagte2 = {});

}

#define BENZIN_IS_ASSERTS_ENABLED BENZIN_IS_DEBUG_BUILD

#ifdef _MSC_VER
    #define BenzinDebugBreak() __debugbreak()
#else
    #error "BenzinDebugBreak is only supported on MSVC"
#endif

#define BenzinEnsure(condition, ...) \
    do \
    { \
        if (const bool isOk{ condition }; !isOk) \
        { \
            const std::string message = benzin::ArgsToFormatString(__VA_ARGS__); \
            if (benzin::Assert(#condition, std::source_location::current(), message)) \
            { \
                BenzinDebugBreak(); \
            }\
        } \
    } while (0)

#if BENZIN_IS_ASSERTS_ENABLED
    #define BenzinAssert(condition, ...) BenzinEnsure(condition, __VA_ARGS__)
    #define BenzinAssertExpr(expression, ...) BenzinEnsure(expression, __VA_ARGS__)
#else
    #define BenzinAssert(condition, ...)
    #define BenzinAssertExpr(expression, ...) BenzinUnused(expression)
#endif
