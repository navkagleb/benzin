#pragma once

namespace spieler
{

    inline void ErrorMessage(const std::string& function, const std::string& filename, uint32_t line)
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
    inline bool CheckStatus(T result, const std::string& function, const std::string& filename, uint32_t line)
    {
        if constexpr (std::is_null_pointer_v<T>)
        {
            ::spieler::ErrorMessage(function, filename, line);

            return false;
        }
        else if constexpr (std::is_pointer_v<T> || std::is_same_v<bool, T>)
        {
            if (!result)
            {
                ::spieler::ErrorMessage(function, filename, line);

                return false;
            }
        }
        else if constexpr (std::is_same_v<HRESULT, T>)
        {
            if (FAILED(result))
            {
                ::spieler::ErrorMessage(function, filename, line);

                return false;
            }
        }
        else if constexpr (std::is_same_v<BOOL, T>)
        {
            if (result == 0)
            {
                ::spieler::ErrorMessage(function, filename, line);

                return false;
            }
        }
        else if (!result)
        {
            ::spieler::ErrorMessage(function, filename, line);

            return false;
        }

        return true;
    }

#define SPIELER_RETURN_IF_FAILED(status)                                        \
{                                                                               \
    if (!::spieler::CheckStatus(status, __FUNCTION__, __FILE__, __LINE__))      \
    {                                                                           \
        return false;                                                           \
    }                                                                           \
}

    constexpr inline uint64_t ConvertKBToBytes(uint64_t kb)
    {
        return kb * 1024;
    }

    constexpr inline uint64_t ConvertMBToBytes(uint64_t mb)
    {
        return mb * 1024 * 1024;
    }

    constexpr inline uint64_t ConvertGBToBytes(uint64_t gb)
    {
        return gb * 1024 * 1024 * 1024;
    }

    constexpr inline uint64_t ConvertBytesToKB(uint64_t bytes)
    {
        return (bytes / 1024) + (bytes % 1024 != 0);
    }

    constexpr inline uint64_t ConvertBytesToMB(uint64_t bytes)
    {
        const uint64_t kb{ ConvertBytesToKB(bytes) };

        return (kb / 1024) + (kb % 1024 != 0);
    }

    constexpr inline uint64_t ConvertBytesToGB(uint64_t bytes)
    {
        const uint64_t mb{ ConvertBytesToMB(bytes) };

        return (mb / 1024) + (mb % 1024 != 0);
    }

} // namespace spieler
