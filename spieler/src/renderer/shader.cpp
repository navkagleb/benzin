#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/shader.hpp"

#include "spieler/core/logger.hpp"
#include "spieler/core/assert.hpp"
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
            std::vector<D3D_SHADER_MACRO> d3dDefines;

            if (!defines.empty())
            {
                d3dDefines.reserve(defines.size());

                for (const auto& [name, value] : defines)
                {
                    d3dDefines.emplace_back(name.data(), value.c_str());
                }

                d3dDefines.emplace_back(nullptr, nullptr);
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
                    d3dDefines.data(),
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
                SPIELER_CRITICAL(reinterpret_cast<const char*>(error->GetBufferPointer()));
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
                case ShaderType::Vertex:
                {
                    return config::GetVertexShaderTarget();
                }
                case ShaderType::Pixel:
                {
                    return config::GetPixelShaderTarget();
                }
                case ShaderType::Geometry:
                {
                    return config::GetGeometryShaderTarget();
                }
                default:
                {
                    SPIELER_WARNING("Shader type is None or Unknown!");
                    break;
                }
            }

            return nullptr;
        }

    } // namespace _internal

    const void* Shader::GetData() const
    {
        SPIELER_ASSERT(m_ByteCode);

        return m_ByteCode->GetBufferPointer();
    }

    uint32_t Shader::GetSize() const
    {
        SPIELER_ASSERT(m_ByteCode);

        return static_cast<uint32_t>(m_ByteCode->GetBufferSize());
    }

    Shader::Shader(const Config& config)
    {
        SPIELER_ASSERT(_internal::CompileFromFile(
            config.Filename, 
            config.EntryPoint, 
            _internal::GetShaderTarget(config.Type),
            config.Defines,
            m_ByteCode
        ));
    }

} // namespace spieler::renderer