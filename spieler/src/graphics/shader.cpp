#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/shader.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include "spieler/core/common.hpp"

namespace spieler
{

    namespace _internal
    {

        static const char* GetShaderTarget(Shader::Type shaderType)
        {
            switch (shaderType)
            {
                case Shader::Type::Vertex: return config::GetVertexShaderTarget();
                case Shader::Type::Hull: return config::GetHullShaderTarget();
                case Shader::Type::Domain: return config::GetDomainShaderTarget();
                case Shader::Type::Geometry: return config::GetGeometryShaderTarget();
                case Shader::Type::Pixel: return config::GetPixelShaderTarget();
                case Shader::Type::Compute: return config::GetComputeShaderTarget();

                default:
                {
                    SPIELER_WARNING("Unknown ShaderType!");
                    break;
                }
            }

            return nullptr;
        }

    } // namespace _internal

    std::shared_ptr<Shader> Shader::Create(const Config& config)
    {
        return std::make_shared<Shader>(config);
    }

    Shader::Shader(const Config& config)
    {
        CompileFromFile(config.Type, config.Filepath, config.EntryPoint, config.Defines);
    }

    Shader::~Shader()
    {
        if (m_DX12Shader)
        {
            m_DX12Shader->Release();
        }
    }

    const void* Shader::GetData() const
    {
        SPIELER_ASSERT(m_DX12Shader);

        return m_DX12Shader->GetBufferPointer();
    }

    uint64_t Shader::GetSize() const
    {
        SPIELER_ASSERT(m_DX12Shader);

        return m_DX12Shader->GetBufferSize();
    }

    void Shader::CompileFromFile(Type type, const std::wstring_view& filepath, const std::string_view& entryPoint, const DefineContainer& defines)
    {
        std::vector<D3D_SHADER_MACRO> dx12Defines;

        if (!defines.empty())
        {
            dx12Defines.reserve(defines.size());

            for (const auto& [name, value] : defines)
            {
                dx12Defines.emplace_back(name.data(), value.data());
            }

            dx12Defines.emplace_back(nullptr, nullptr);
        }

#if defined(SPIELER_DEBUG)
        const uint32_t flags{ D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION };
#else
        const uint32_t flags{ 0 };
#endif

        ComPtr<ID3DBlob> error;

        const char* target{ _internal::GetShaderTarget(type) };
        SPIELER_ASSERT(target);

        const ::HRESULT result
        {
            D3DCompileFromFile(
                filepath.data(),
                dx12Defines.data(),
                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                entryPoint.data(),
                target,
                flags,
                0,
                &m_DX12Shader,
                &error
            )
        };

        if (error)
        {
            SPIELER_CRITICAL("{}", static_cast<const char*>(error->GetBufferPointer()));
            return;
        }

        if (FAILED(result))
        {
            SPIELER_CRITICAL("Failed to compile shader");
            return;
        }
    }

} // namespace spieler
