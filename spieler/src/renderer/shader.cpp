#include "renderer/shader.h"

#include <d3dcompiler.h>

namespace Spieler
{

    namespace Internal
    {

        static ComPtr<ID3DBlob> CompileFromFile(const std::wstring& filename, const std::string& entryPoint, const std::string& target)
        {
#if defined(SPIELER_DEBUG)
            const std::uint32_t flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
            const std::uint32_t flags = 0;
#endif

            ComPtr<ID3DBlob> byteCode;
            ComPtr<ID3DBlob> error;

            const HRESULT result = D3DCompileFromFile(
                filename.c_str(),
                nullptr,
                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                entryPoint.c_str(),
                target.c_str(),
                flags,
                0,
                &byteCode,
                &error
            );

            if (error)
            {
                ::OutputDebugString(reinterpret_cast<const char*>(error->GetBufferPointer()));
                return nullptr;
            }

            if (result != S_OK)
            {
                return nullptr;
            }

            return byteCode;
        }

    } // namespace Internal

    bool VertexShader::LoadFromFile(const std::wstring& filename, const std::string& entryPoint)
    {
        m_ByteCode = Internal::CompileFromFile(filename, entryPoint, "vs_5_1");

        if (!m_ByteCode)
        {
            return false;
        }

        return true;
    }

    bool PixelShader::LoadFromFile(const std::wstring& filename, const std::string& entryPoint)
    {
        m_ByteCode = Internal::CompileFromFile(filename, entryPoint, "ps_5_1");

        if (!m_ByteCode)
        {
            return false;
        }

        return true;
    }

} // namespace Spieler