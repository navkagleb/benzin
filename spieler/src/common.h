#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <sstream>

#include <wrl.h>

namespace Spieler
{

#if defined(DEBUG) || defined(_DEBUG)
    #define SPIELER_DEBUG
#endif

#if defined(SPIELER_DEBUG)
    #define SPIELER_ASSERT(expression) assert(expression)
#else
    #define SPIELER_ASSERT(expession)
#endif

#define SPIELER_USE_IMGUI 0

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

    inline void ErrorMessage(const std::string& function, const std::string& filename, std::uint32_t line)
    {
        std::ostringstream oss;

#if defined(SPIELER_DEBUG)
        oss << "    Function: " << function
            << "\n    File: " << filename
            << "\n    Line: " << std::to_string(line);

        OutputDebugString("----------- SPIELER_ERROR -----------\n");
        OutputDebugString((oss.str() + "\n").c_str());
        OutputDebugString("-------------------------------------\n");
#else
        oss << "Function: " << function
            << "\nFile: " << filename
            << "\nLine: " << std::to_string(line);

        ::MessageBox(nullptr, oss.str().c_str(), "Error", MB_OK);
#endif
    }

    template <typename T>
    inline bool CheckStatus(T status, const std::string& function, const std::string& filename, std::uint32_t line)
    {
        if constexpr (std::is_same_v<T, HRESULT>)
        {
            if (FAILED(status))
            {
                ErrorMessage(function, filename, line);

                return false;
            }
        }
        else if constexpr (std::is_same_v<T, BOOL> || std::is_same_v<T, bool>)
        {
            if (!status)
            {
                ErrorMessage(function, filename, line);

                return false;
            }
        }

        return true;
    }

#define SPIELER_CHECK_STATUS(status)                                            \
{                                                                               \
    if (!CheckStatus(status, __FUNCTION__, __FILE__, __LINE__))                 \
    {                                                                           \
        return false;                                                           \
    }                                                                           \
}

    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

} // namespace Spieler