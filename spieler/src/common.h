#pragma once

#include <cassert>

#include <wrl.h>

namespace Spieler
{

#if defined(DEBUG) || defined(_DEBUG)
    #define SPIELER_DEBUG
#endif

#if defined(SPIELER_DEBUG)
    #define SPIELER_ASSERT(expression) assert(expression)
#endif

#define NON_COPYABLE_IMPL(ClassName)                            \
private:                                                        \
    ClassName(const ClassName& other) = delete;                 \
    ClassName& operator =(const ClassName& other) = delete;     

#define NON_MOVEABLE_IMPL(ClassName)                            \
private:                                                        \
    ClassName(ClassName&& other) = delete;                      \
    ClassName& operator =(ClassName&& other) = delete;          

#define SINGLETON_IMPL(ClassName)                               \
public:                                                         \
    ClassName() = default;                                      \
                                                                \
    static ClassName& GetInstance()                             \
    {                                                           \
        static ClassName instance;                              \
        return instance;                                        \
    }                                                           \
                                                                \
    NON_COPYABLE_IMPL(ClassName)                                \
    NON_MOVEABLE_IMPL(ClassName)                                 

    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

} // namespace Spieler