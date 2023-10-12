#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/shaders.hpp"

#include "benzin/graphics/pipeline_state.hpp"

namespace benzin
{

    namespace
    {

        constexpr magic_enum::containers::array<ShaderType, std::wstring_view> g_ShaderTargets
        {
            config::g_VertexShaderTarget,
            config::g_PixelShaderTarget,
            config::g_ComputeShaderTarget,
        };

        struct ShaderPaths
        {
            std::filesystem::path SourceFilePath;
            std::filesystem::path BinaryFilePath;
            std::filesystem::path DebugFilePath;

            ShaderPaths(size_t hash, std::string_view fileName)
                : SourceFilePath{ GetShaderSourceFilePath(fileName) }
                , BinaryFilePath{ GetShaderBinaryFilePath(hash) }
                , DebugFilePath{ GetShaderDebugInfoFilePath(hash) }
            {}
        };

        struct ShaderArgs
        {
            std::wstring_view Target;
            std::wstring EntryPoint;
            std::vector<std::wstring> Defines;

            ShaderArgs(std::wstring_view target)
                : Target{ target }
            {}

            ShaderArgs(std::string_view entryPoint, ShaderType shaderType, const std::span<const std::string>& defines)
                : EntryPoint{ ToWideString(entryPoint) }
                , Target{ g_ShaderTargets[shaderType] }
            {
                if (!defines.empty())
                {
                    Defines.reserve(defines.size());
                    for (const auto& define : defines)
                    {
                        Defines.push_back(ToWideString(define));
                    }
                }
            }
        };

        std::vector<const wchar_t*> GetCompileArgs(const ShaderPaths& paths, const ShaderArgs& args)
        {
            std::vector<const wchar_t*> compileArgs;

            // Fill paths
            {
                // Source file
                compileArgs.push_back(paths.SourceFilePath.c_str());

                // Include directory
                compileArgs.push_back(L"-I");
                compileArgs.push_back(config::g_AbsoluteShaderSourceDirectoryPath.c_str());

                // Optimizations
                if constexpr (BENZIN_IS_DEBUG_BUILD)
                {
                    compileArgs.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);

                    // Generate symbols
                    compileArgs.push_back(DXC_ARG_DEBUG);

                    // PDB file
                    compileArgs.push_back(L"-Fd");
                    compileArgs.push_back(paths.DebugFilePath.c_str());
                }
                else
                {
                    compileArgs.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
                }

                // Binary file
                compileArgs.push_back(L"-Fo");
                compileArgs.push_back(paths.BinaryFilePath.c_str());

                // Remove everything from binary
                compileArgs.push_back(L"-Qstrip_debug");
                compileArgs.push_back(L"-Qstrip_reflect");
                compileArgs.push_back(L"-Qstrip_rootsignature");

                // Other arguments
                compileArgs.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
                compileArgs.push_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR); // To prevent matrix transpose in CPU side
            }

            // Fill args
            {
                // Target
                BenzinAssert(!args.Target.empty());
                compileArgs.push_back(L"-T");
                compileArgs.push_back(args.Target.data());

                if (!args.EntryPoint.empty())
                {
                    // Entry point
                    compileArgs.push_back(L"-E");
                    compileArgs.push_back(args.EntryPoint.c_str());
                }

                // Defines
                for (const auto& define : args.Defines)
                {
                    compileArgs.push_back(L"-D");
                    compileArgs.push_back(define.c_str());
                }
            }

            return compileArgs;
        }

        class ShaderCompiler
        {
        public:
            struct CompileResult
            {
                std::vector<std::byte> BinaryBlob;
                std::vector<std::byte> DebugBlob;
            };

        public:
            ShaderCompiler()
            {
                BenzinAssert(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_DxcUtils)));
                BenzinAssert(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_DxcCompiler)));

                BenzinAssert(m_DxcUtils->CreateDefaultIncludeHandler(&m_DxcIncludeHandler));
            }

        public:
            CompileResult Compile(const ShaderPaths& paths, const ShaderArgs& args) const
            {
                const ComPtr<IDxcBlobEncoding> dxcShaderSource = LoadShaderFromFile(paths.SourceFilePath);

                const DxcBuffer dxcSourceBuffer
                {
                    .Ptr = dxcShaderSource->GetBufferPointer(),
                    .Size = dxcShaderSource->GetBufferSize(),
                    .Encoding = 0,
                };

                const std::vector<const wchar_t*> compileArgs = GetCompileArgs(paths, args);

                ComPtr<IDxcResult> dxcResult;
                BenzinAssert(m_DxcCompiler->Compile(
                    &dxcSourceBuffer,
                    (LPCWSTR*)compileArgs.data(),
                    static_cast<UINT32>(compileArgs.size()),
                    m_DxcIncludeHandler.Get(),
                    IID_PPV_ARGS(&dxcResult)
                ));

                ComPtr<IDxcBlobUtf8> dxcErrorBlob;
                ComPtr<IDxcBlobWide> dummyName; // To avoid warning
                dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&dxcErrorBlob), &dummyName);
                if (dxcErrorBlob && dxcErrorBlob->GetStringLength() > 0)
                {
                    BenzinError("Failed to compile shader: {}", paths.SourceFilePath.string());
                    BenzinError("Error: {}", dxcErrorBlob->GetStringPointer());

                    BenzinAssert(false);
                }

                return ParseDXCResult(dxcResult);
            }

        private:
            ComPtr<IDxcBlobEncoding> LoadShaderFromFile(const std::filesystem::path& filePath) const
            {
                uint32_t codePage = CP_UTF8;
                ComPtr<IDxcBlobEncoding> dxcShaderSource;
                BenzinAssert(m_DxcUtils->LoadFile(filePath.c_str(), &codePage, &dxcShaderSource));

                return dxcShaderSource;
            }

            CompileResult ParseDXCResult(const ComPtr<IDxcResult>& dxcResult) const
            {
                CompileResult result;

                // Get binary blob
                {
                    ComPtr<IDxcBlob> dxcBinaryBlob;
                    BenzinAssert(dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxcBinaryBlob), nullptr));

                    const auto* data = reinterpret_cast<const std::byte*>(dxcBinaryBlob->GetBufferPointer());
                    const size_t size = dxcBinaryBlob->GetBufferSize();

                    result.BinaryBlob.assign(data, data + size);
                }

                // Get PDB
                {
                    ComPtr<IDxcBlob> dxcDebugBlob;
                    BenzinAssert(dxcResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&dxcDebugBlob), nullptr));

                    const auto* data = reinterpret_cast<const std::byte*>(dxcDebugBlob->GetBufferPointer());
                    const size_t size = dxcDebugBlob->GetBufferSize();

                    result.DebugBlob.assign(data, data + size);
                }

                return result;
            }

        private:
            ComPtr<IDxcUtils> m_DxcUtils;
            ComPtr<IDxcCompiler3> m_DxcCompiler;
            ComPtr<IDxcIncludeHandler> m_DxcIncludeHandler;
        };

        size_t GetShaderHash(const ShaderCreation& shaderCreation)
        {
            return std::hash<std::string>{}(fmt::format("{}{}{}", shaderCreation.FileName, shaderCreation.EntryPoint, fmt::join(shaderCreation.Defines, "")));
        }

        size_t GetLibraryHash(std::string_view fileName)
        {
            return std::hash<std::string_view>{}(fileName);
        }

        ShaderCompiler g_ShaderCompiler;

        std::unordered_map<size_t, std::vector<std::byte>> g_ShaderDXILBinaries;
        std::unordered_map<size_t, std::vector<std::byte>> g_LibraryDXILBinaries;

    } // anonymous namespace

    std::span<const std::byte> GetShaderBinary(ShaderType shaderType, const ShaderCreation& shaderCreation)
    {
        const size_t hash = GetShaderHash(shaderCreation);

        if (g_ShaderDXILBinaries.contains(hash))
        {
            return g_ShaderDXILBinaries.at(hash);
        }

        const ShaderPaths paths{ hash, shaderCreation.FileName };
        const ShaderArgs args{ shaderCreation.EntryPoint, shaderType, shaderCreation.Defines };

        if (!IsDestinationFileOlder(paths.SourceFilePath, paths.BinaryFilePath))
        {
            return g_ShaderDXILBinaries[hash] = ReadFromFile(paths.BinaryFilePath);
        }

        const ShaderCompiler::CompileResult result = g_ShaderCompiler.Compile(paths, args);
        BenzinTrace("ShaderCompiled: {}! File: {}, EntryPoint: {}", hash, shaderCreation.FileName, shaderCreation.EntryPoint);

        WriteToFile(paths.BinaryFilePath, result.BinaryBlob);
        WriteToFile(paths.DebugFilePath, result.DebugBlob);

        return g_ShaderDXILBinaries[hash] = result.BinaryBlob;
    }

    std::span<const std::byte> GetLibraryBinary(std::string_view fileName)
    {
        const size_t hash = GetLibraryHash(fileName);

        if (g_LibraryDXILBinaries.contains(hash))
        {
            return g_LibraryDXILBinaries.at(hash);
        }

        const ShaderPaths paths{ hash, fileName };
        const ShaderArgs args{ config::g_LibraryTarget };

        if (!IsDestinationFileOlder(paths.SourceFilePath, paths.BinaryFilePath))
        {
            return g_LibraryDXILBinaries[hash] = ReadFromFile(paths.BinaryFilePath);
        }

        const ShaderCompiler::CompileResult result = g_ShaderCompiler.Compile(paths, args);
        BenzinTrace("LibraryCompiled: {}! File: {}", hash, fileName);

        WriteToFile(paths.BinaryFilePath, result.BinaryBlob);
        WriteToFile(paths.DebugFilePath, result.DebugBlob);

        return g_LibraryDXILBinaries[hash] = result.BinaryBlob;
    }

} // namespace benzin
