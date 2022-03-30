#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <sstream>
#include <memory>

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

#define NON_COPYABLE_IMPL(ClassName)                            \
private:                                                        \
    ClassName(const ClassName& other) = delete;                 \
    ClassName& operator =(const ClassName& other) = delete;     

#define NON_MOVEABLE_IMPL(ClassName)                            \
private:                                                        \
    ClassName(ClassName&& other) = delete;                      \
    ClassName& operator =(ClassName&& other) = delete;          

#define SINGLETON_IMPL(ClassName)                               \
protected:                                                      \
    ClassName() = default;                                      \
                                                                \
public:                                                         \
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

        ::OutputDebugString("----------- SPIELER_ERROR -----------\n");
        ::OutputDebugString((oss.str() + "\n").c_str());
        ::OutputDebugString("-------------------------------------\n");
#else
        oss << "Function: " << function
            << "\nFile: " << filename
            << "\nLine: " << std::to_string(line);

        ::MessageBox(nullptr, oss.str().c_str(), "Error", MB_OK);
#endif
    }

    template <typename T>
    inline bool CheckStatus(T result, const std::string& function, const std::string& filename, std::uint32_t line)
    {
        if constexpr (std::is_null_pointer_v<T>)
        {
            ::Spieler::ErrorMessage(function, filename, line);

            return false;
        }
        else if constexpr (std::is_pointer_v<T> || std::is_same_v<bool, T>)
        {
            if (!result)
            {
                ::Spieler::ErrorMessage(function, filename, line);

                return false;
            }
        }
        else if constexpr (std::is_same_v<HRESULT, T>)
        {
            if (FAILED(result))
            {
                ::Spieler::ErrorMessage(function, filename, line);

                return false;
            }
        }
        else if constexpr (std::is_same_v<BOOL, T>)
        {
            if (result == 0)
            {
                ::Spieler::ErrorMessage(function, filename, line);

                return false;
            }
        }
        else if (!result)
        {
            ::Spieler::ErrorMessage(function, filename, line);

            return false;
        }

        return true;
    }

#define SPIELER_RETURN_IF_FAILED(status)                                        \
{                                                                               \
    if (!::Spieler::CheckStatus(status, __FUNCTION__, __FILE__, __LINE__))      \
    {                                                                           \
        return false;                                                           \
    }                                                                           \
}

    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    template <typename T>
    using Reference = std::shared_ptr<T>;

    template <typename T, typename... Args>
    inline Reference<T> CreateReference(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    using Scope = std::unique_ptr<T>;

    template <typename T, typename... Args>
    inline Scope<T> CreateScope(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

} // namespace Spieler