#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/shader.hpp"

#include "spieler/core/common.hpp"

namespace spieler::renderer
{

    namespace _internal
    {

        static bool CompileFromFile(
            const std::wstring& filename, 
            const std::string& entryPoint, 
            const std::string& target, 
            const DefineContainer& defines,
            ComPtr<ID3DBlob>& byteCode)
        {
            std::vector<D3D_SHADER_MACRO> dx12Defines;

            if (!defines.empty())
            {
                dx12Defines.reserve(defines.size());

                for (const auto& [name, value] : defines)
                {
                    dx12Defines.emplace_back(name.data(), value.c_str());
                }

                dx12Defines.emplace_back(nullptr, nullptr);
            }

#if defined(SPIELER_DEBUG)
            const uint32_t flags{ D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION };
#else
            const uint32_t flags{ 0 };
#endif

            ComPtr<ID3DBlob> error;

            const ::HRESULT result
            {
                D3DCompileFromFile(
                    filename.c_str(),
                    dx12Defines.data(),
                    D3D_COMPILE_STANDARD_FILE_INCLUDE,
                    entryPoint.c_str(),
                    target.c_str(),
                    flags,
                    0,
                    &byteCode,
                    &error
                )
            };

            if (error)
            {
                SPIELER_CRITICAL("{}", static_cast<const char*>(error->GetBufferPointer()));
                return false;
            }

            if (FAILED(result))
            {
                SPIELER_CRITICAL("Failed to compile shader");
                return false;
            }

            return true;
        }

        static const char* GetShaderTarget(ShaderType shaderType)
        {
            switch (shaderType)
            {
                case ShaderType::Vertex: return config::GetVertexShaderTarget();
                case ShaderType::Hull: return config::GetHullShaderTarget();
                case ShaderType::Domain: return config::GetDomainShaderTarget();
                case ShaderType::Geometry: return config::GetGeometryShaderTarget();
                case ShaderType::Pixel: return config::GetPixelShaderTarget();
                case ShaderType::Compute: return config::GetComputeShaderTarget();

                default:
                {
                    SPIELER_WARNING("Unknown ShaderType!");
                    break;
                }
            }

            return nullptr;
        }

    } // namespace _internal

    const void* Shader::GetData() const
    {
        SPIELER_ASSERT(m_DX12Blob);

        return m_DX12Blob->GetBufferPointer();
    }

    uint32_t Shader::GetSize() const
    {
        SPIELER_ASSERT(m_DX12Blob);

        return static_cast<uint32_t>(m_DX12Blob->GetBufferSize());
    }

    Shader::Shader(ShaderType type, const std::wstring& filename, const std::string& entryPoint, const DefineContainer& defines)
    {
        SPIELER_ASSERT(_internal::CompileFromFile(
            filename, 
            entryPoint, 
            _internal::GetShaderTarget(type),
            defines,
            m_DX12Blob
        ));
    }

} // namespace spieler::renderer
