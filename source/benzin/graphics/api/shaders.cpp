#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/shaders.hpp"

#include "benzin/utility/string_utils.h"
#include "benzin/graphics/api/common.hpp"

namespace benzin
{

    namespace
    {

        constexpr magic_enum::containers::array<ShaderType, std::string_view> g_ShaderTargets
        {
            config::GetVertexShaderTarget(),
            config::GetPixelShaderTarget(),
            config::GetComputeShaderTarget()
        };

        struct ShaderCompiler
        {
        public:
            ShaderCompiler()
            {
                BENZIN_HR_ASSERT(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_DxcUtils)));
                BENZIN_HR_ASSERT(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_DxcCompiler3)));

                BENZIN_HR_ASSERT(m_DxcUtils->CreateDefaultIncludeHandler(&m_DxcIncludeHandler));
            }

            ComPtr<IDxcBlobEncoding> LoadShaderFromFile(std::string_view fileName) const
            {
                static const std::filesystem::path shaderRootPath{ config::GetShaderPath() };

                const std::filesystem::path shaderFilePath = shaderRootPath / fileName;
                BENZIN_ASSERT(std::filesystem::exists(shaderFilePath));
                const std::wstring wideFilePath = ToWideString(shaderFilePath.string());

                uint32_t codePage = CP_UTF8;
                ComPtr<IDxcBlobEncoding> dxcShaderSource;
                BENZIN_HR_ASSERT(m_DxcUtils->LoadFile(wideFilePath.data(), &codePage, &dxcShaderSource));

                return dxcShaderSource;
            }

            std::vector<std::byte> CompileShader(
                ShaderType shaderType,
                std::string_view entryPoint,
                const std::vector<std::string>& defines,
                const ComPtr<IDxcBlobEncoding>& dxcShaderSourceBlob
            ) const
            {
                std::vector<LPCWSTR> arguments;

                const std::wstring wideEntryPoint = ToWideString(entryPoint);
                arguments.push_back(L"-E");
                arguments.push_back(wideEntryPoint.c_str());

                const std::string_view shaderTarget = g_ShaderTargets[shaderType];
                const std::wstring wideShaderTarget = ToWideString(shaderTarget);
                arguments.push_back(L"-T");
                arguments.push_back(wideShaderTarget.c_str());

                const std::wstring wideIncludeDirectory = ToWideString(config::GetShaderPath());
                arguments.push_back(L"-I");
                arguments.push_back(wideIncludeDirectory.c_str());

                for (const auto& define : defines)
                {
                    arguments.push_back(L"-D");
                    arguments.push_back(ToWideString(define).c_str());
                }

                arguments.push_back(L"-Qstrip_debug");
                arguments.push_back(L"-Qstrip_reflect");
                arguments.push_back(L"-Qstrip_rootsignature");
                arguments.push_back(L"-enable-16bit-types");
                arguments.push_back(L"-HV 2021"); // Templates

#if defined(BENZIN_DEBUG_BUILD)
                arguments.push_back(DXC_ARG_DEBUG);
                arguments.push_back(DXC_ARG_DEBUG_NAME_FOR_SOURCE);
                arguments.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
#endif

                arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
                arguments.push_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR); // To prevent matrix transpose in CPU side

                const DxcBuffer dxcSourceBuffer
                {
                    .Ptr{ dxcShaderSourceBlob->GetBufferPointer() },
                    .Size{ dxcShaderSourceBlob->GetBufferSize() },
                    .Encoding{ 0 }
                };

                ComPtr<IDxcResult> dxcCompileResult;
                BENZIN_HR_ASSERT(m_DxcCompiler3->Compile(
                    &dxcSourceBuffer,
                    arguments.data(),
                    static_cast<UINT32>(arguments.size()),
                    m_DxcIncludeHandler.Get(),
                    IID_PPV_ARGS(&dxcCompileResult)
                ));

                ComPtr<IDxcBlobUtf16> dxcDummyName; // To prevent warnings

                ComPtr<IDxcBlobUtf8> dxcErrorBlob;
                dxcCompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&dxcErrorBlob), &dxcDummyName);
                if (dxcErrorBlob && dxcErrorBlob->GetStringLength() > 0)
                {
                    BENZIN_ERROR("Failed to compile shader: {}", static_cast<const char*>(dxcErrorBlob->GetBufferPointer()));
                    BENZIN_ASSERT(false);
                }

                ComPtr<IDxcBlob> dxcShaderByteCode;
                dxcCompileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxcShaderByteCode), &dxcDummyName);
                BENZIN_ASSERT(dxcShaderByteCode && dxcShaderByteCode->GetBufferSize() > 0);

                const auto* data = reinterpret_cast<std::byte*>(dxcShaderByteCode->GetBufferPointer());
                const auto size = dxcShaderByteCode->GetBufferSize();
                return std::vector<std::byte>{ data, data + size };
            }

        private:
            ComPtr<IDxcUtils> m_DxcUtils;
            ComPtr<IDxcCompiler3> m_DxcCompiler3;
            ComPtr<IDxcIncludeHandler> m_DxcIncludeHandler;
        };

        const ShaderCompiler g_ShaderCompiler;
        std::unordered_map<size_t, std::vector<std::byte>> g_Shaders;

        size_t GetShaderKey(std::string_view fileName, std::string_view entryPoint, const std::vector<std::string>& defines)
        {
            const std::string key = std::accumulate(defines.begin(), defines.end(), fmt::format("{}{}", fileName, entryPoint));
            return std::hash<std::string>{}(key);
        }

    } // anonymous namespace

    std::span<const std::byte> GetShaderBlob(
        ShaderType shaderType,
        std::string_view fileName, 
        std::string_view entryPoint,
        const std::vector<std::string>& defines
    )
    {
        const size_t shaderKey = GetShaderKey(fileName, entryPoint, defines);

        auto shaderIterator = g_Shaders.find(shaderKey);

        if (shaderIterator == g_Shaders.end())
        {
            const ComPtr<IDxcBlobEncoding> dxcShaderSourceBlob = g_ShaderCompiler.LoadShaderFromFile(fileName);
            std::vector<std::byte> shaderByteCode = g_ShaderCompiler.CompileShader(shaderType, entryPoint, defines, dxcShaderSourceBlob);
            
            if (defines.empty())
            {
                BENZIN_TRACE("ShaderCompiled: {}! File: {}, EntryPoint: {}", shaderKey, fileName.data(), entryPoint.data());
            }
            else
            {
                BENZIN_TRACE("ShaderCompiled: {}! File: {}, EntryPoint: {}, Defines: {}", shaderKey, fileName.data(), entryPoint.data(), fmt::join(defines, ", "));
            }

            shaderIterator = g_Shaders.insert(std::make_pair(shaderKey, std::move(shaderByteCode))).first;
        }

        return shaderIterator->second;
    }

} // namespace benzin
