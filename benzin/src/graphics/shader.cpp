#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/shader.hpp"

namespace benzin
{

    namespace
    {

        const char* GetShaderTarget(Shader::Type shaderType)
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
                    BENZIN_WARNING("Unknown ShaderType!");
                    break;
                }
            }

            return nullptr;
        }

    } // anonymous namespace

    Shader::Shader(const Config& config)
    {
        CompileFromFile(config.Type, config.Filepath, config.EntryPoint, config.Defines);
    }

    Shader::~Shader()
    {
        SafeReleaseD3D12Object(m_D3D12Shader);
    }

    const void* Shader::GetData() const
    {
        BENZIN_ASSERT(m_D3D12Shader);

        return m_D3D12Shader->GetBufferPointer();
    }

    uint64_t Shader::GetSize() const
    {
        BENZIN_ASSERT(m_D3D12Shader);

        return m_D3D12Shader->GetBufferSize();
    }

    void Shader::CompileFromFile(Type type, const std::wstring_view& filepath, const std::string_view& entryPoint, const std::vector<Define>& defines)
    {
        std::vector<D3D_SHADER_MACRO> d3d12Defines;

        if (!defines.empty())
        {
            d3d12Defines.reserve(defines.size());

            for (const auto& [name, value] : defines)
            {
                d3d12Defines.emplace_back(name.data(), value.c_str());
            }

            d3d12Defines.emplace_back(nullptr, nullptr);
        }

#if defined(BENZIN_DEBUG)
        const uint32_t flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        const uint32_t flags = 0;
#endif

        ComPtr<ID3DBlob> error;

        const char* target = GetShaderTarget(type);
        BENZIN_ASSERT(target);

        const HRESULT result = D3DCompileFromFile(
            filepath.data(),
            d3d12Defines.data(),
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint.data(),
            target,
            flags,
            0,
            &m_D3D12Shader,
            &error
        );

        if (error)
        {
            BENZIN_CRITICAL("{}", static_cast<const char*>(error->GetBufferPointer()));
            return;
        }

        BENZIN_D3D12_ASSERT(result);
    }

} // namespace benzin
