#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/shaders.hpp"

#include "benzin/graphics/pipeline_state.hpp"

namespace benzin
{

    namespace
    {

        constexpr auto GetShaderTargets()
        {
            magic_enum::containers::array<ShaderType, std::wstring_view> shaderTargets;
            shaderTargets[ShaderType::Vertex] = config::g_VertexShaderTarget;
            shaderTargets[ShaderType::Pixel] = config::g_PixelShaderTarget;
            shaderTargets[ShaderType::Compute] = config::g_ComputeShaderTarget;
            shaderTargets[ShaderType::Library] = config::g_ShaderLibraryTarget;

            return shaderTargets;
        }

        struct ShaderPaths
        {
            std::filesystem::path SourceFilePath;
            std::filesystem::path BinaryFilePath;
            std::filesystem::path DebugFilePath;

            ShaderPaths(size_t hash, std::string_view fileName)
                : SourceFilePath{ GetShaderSourceFilePath(fileName) }
                , BinaryFilePath{ GetShaderBinaryFilePath(hash) }
                , DebugFilePath{ GetShaderDebugFilePath(hash) }
            {}
        };

        struct ShaderArgs
        {
            static constexpr auto s_ShaderTargets = GetShaderTargets();

            std::wstring_view Target;
            std::wstring EntryPoint;
            std::vector<std::wstring> Defines;

            ShaderArgs() = default;

            ShaderArgs(ShaderType shaderType)
                : Target{ s_ShaderTargets[shaderType] }
            {}

            ShaderArgs(ShaderType shaderType, std::string_view entryPoint, const std::span<const std::string>& defines)
                : Target{ s_ShaderTargets[shaderType] }
                , EntryPoint{ ToWideString(entryPoint) }
                , Defines{ std::from_range, defines | std::views::transform(ToWideString) }
            {}
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
                compileArgs.push_back(config::g_IsShaderDebugEnabled ? DXC_ARG_SKIP_OPTIMIZATIONS : DXC_ARG_OPTIMIZATION_LEVEL3);

                if constexpr (config::g_IsShaderSymbolsEnabled)
                {
                    compileArgs.push_back(DXC_ARG_DEBUG); // Generate symbols

                    // PDB file
                    compileArgs.push_back(L"-Fd");
                    compileArgs.push_back(paths.DebugFilePath.c_str());
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
                BenzinAssert(dxcShaderSource.Get());

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
                    BenzinError("ErrorMessage: \n{}", dxcErrorBlob->GetStringPointer());

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

                {
                    ComPtr<IDxcBlob> dxcBinaryBlob;
                    BenzinAssert(dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxcBinaryBlob), nullptr));
                    BenzinAssert(dxcBinaryBlob.Get() && dxcBinaryBlob->GetBufferSize() != 0);

                    const auto* data = reinterpret_cast<const std::byte*>(dxcBinaryBlob->GetBufferPointer());
                    const size_t size = dxcBinaryBlob->GetBufferSize();

                    result.BinaryBlob.assign(data, data + size);
                }

                if constexpr (config::g_IsShaderSymbolsEnabled)
                {
                    ComPtr<IDxcBlob> dxcDebugBlob;
                    BenzinAssert(dxcResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&dxcDebugBlob), nullptr));
                    BenzinAssert(dxcDebugBlob.Get() && dxcDebugBlob->GetBufferPointer());

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

        size_t GetShaderHash(ShaderType shaderType, const ShaderCreation& shaderCreation)
        {
            size_t hash = 0;

            hash = HashCombine(magic_enum::enum_integer(shaderType), hash);
            hash = HashCombine(shaderCreation.FileName, hash);
            hash = HashCombine(shaderCreation.EntryPoint, hash);

            for (const auto& define : shaderCreation.Defines)
            {
                hash = HashCombine(define, hash);
            }

            return hash;
        }

        ShaderCompiler g_ShaderCompiler;

        std::unordered_map<size_t, std::vector<std::byte>> g_ShaderBinaries;

    } // anonymous namespace

    std::span<const std::byte> GetShaderBinary(ShaderType shaderType, const ShaderCreation& shaderCreation)
    {
        const size_t hash = GetShaderHash(shaderType, shaderCreation);

        if (g_ShaderBinaries.contains(hash))
        {
            return g_ShaderBinaries.at(hash);
        }

        const ShaderPaths paths{ hash, shaderCreation.FileName };

#if 0
        // #TODO: Don't work if change the included file
        if (!IsDestinationFileOlder(paths.SourceFilePath, paths.BinaryFilePath))
        {
            return g_ShaderBinaries[hash] = ReadFromFile(paths.BinaryFilePath);
        }
#endif

        const ShaderArgs args;
        if (shaderType == ShaderType::Library)
        {
            new (&const_cast<ShaderArgs&>(args)) ShaderArgs{ shaderType };
        }
        else
        {
            new (&const_cast<ShaderArgs&>(args)) ShaderArgs{ shaderType, shaderCreation.EntryPoint, shaderCreation.Defines };
        }

        const auto [us, compileResult] = BenzinProfileFunction(g_ShaderCompiler.Compile(paths, args));
        const float ms = us.count() / 1000.0f;

        if (shaderType == ShaderType::Library)
        {
            BenzinTrace("LibraryCompiled: {}! File: {}. Time: {}ms", hash, shaderCreation.FileName, ms);
        }
        else
        {
            BenzinTrace("ShaderCompiled: {}! File: {}, EntryPoint: {}. Time: {}ms", hash, shaderCreation.FileName, shaderCreation.EntryPoint, ms);
        }

        WriteToFile(paths.BinaryFilePath, compileResult.BinaryBlob);

        if constexpr (config::g_IsShaderSymbolsEnabled)
        {
            WriteToFile(paths.DebugFilePath, compileResult.DebugBlob);
        }

        return g_ShaderBinaries[hash] = compileResult.BinaryBlob;
    }

} // namespace benzin
