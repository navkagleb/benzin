#include "renderer/shader.hpp"

#include <d3dcompiler.h>

namespace Spieler
{

    namespace Internal
    {


        static ComPtr<ID3DBlob> CompileFromFile(const std::wstring& filename, const std::string& entryPoint, const std::string& target, const std::vector<ShaderDefine>& defines = {})
        {
            std::vector<D3D_SHADER_MACRO> d3dDefines;

            if (!defines.empty())
            {
                d3dDefines.reserve(defines.size());

                for (const auto& define : defines)
                {
                    d3dDefines.emplace_back(define.Name.c_str(), define.Value.c_str());
                }

                d3dDefines.emplace_back(nullptr, nullptr);
            }

#if defined(SPIELER_DEBUG)
            const std::uint32_t flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
            const std::uint32_t flags = 0;
#endif

            ComPtr<ID3DBlob> byteCode;
            ComPtr<ID3DBlob> error;

            const HRESULT result = D3DCompileFromFile(
                filename.c_str(),
                d3dDefines.data(),
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

    const void* Shader::GetData() const
    {
        SPIELER_ASSERT(m_ByteCode != nullptr);

        return m_ByteCode->GetBufferPointer();
    }

    std::uint32_t Shader::GetSize() const
    {
        SPIELER_ASSERT(m_ByteCode != nullptr);

        return static_cast<std::uint32_t>(m_ByteCode->GetBufferSize());
    }

    bool VertexShader::LoadFromFile(const std::wstring& filename, const std::string& entryPoint)
    {
        m_ByteCode = Internal::CompileFromFile(filename, entryPoint, "vs_5_1");

        SPIELER_RETURN_IF_FAILED(m_ByteCode);

        return true;
    }

    bool VertexShader::LoadFromFile(const std::wstring& filename, const std::string& entryPoint, const std::vector<ShaderDefine>& defines)
    {
        m_ByteCode = Internal::CompileFromFile(filename, entryPoint, "vs_5_1", defines);

        SPIELER_RETURN_IF_FAILED(m_ByteCode);

        return true;
    }

    bool PixelShader::LoadFromFile(const std::wstring& filename, const std::string& entryPoint)
    {
        m_ByteCode = Internal::CompileFromFile(filename, entryPoint, "ps_5_1");

        SPIELER_RETURN_IF_FAILED(m_ByteCode);

        return true;
    }

    bool PixelShader::LoadFromFile(const std::wstring& filename, const std::string& entryPoint, const std::vector<ShaderDefine>& defines)
    {
        m_ByteCode = Internal::CompileFromFile(filename, entryPoint, "ps_5_1", defines);

        SPIELER_RETURN_IF_FAILED(m_ByteCode);

        return true;
    }

} // namespace Spieler