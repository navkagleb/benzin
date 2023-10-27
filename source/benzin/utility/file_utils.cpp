#include "benzin/config/bootstrap.hpp"
#include "benzin/utility/file_utils.hpp"

namespace benzin
{

    namespace fs = std::filesystem;

    std::string CutExtension(std::string_view fileName)
    {
        return fs::path{ fileName }.replace_extension().filename().string();
    }

    void WriteToFile(std::wstring_view fileName, std::span<const std::byte> data)
    {
        if (data.empty())
        {
            return;
        }

        std::ofstream file{ fileName.data(), std::ios::binary | std::ios::trunc };
        BenzinAssert(file.good());

        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
    }

    void WriteToFile(std::wstring_view directory, std::wstring_view fileName, std::span<const std::byte> data)
    {
        WriteToFile(fs::path{ directory } / fileName, data);
    }

    void WriteToFile(const fs::path& filePath, std::span<const std::byte> data)
    {
        BenzinAssert(filePath.has_filename());

        fs::create_directories(filePath.parent_path());

        WriteToFile(std::wstring_view{ filePath.native() }, data);
    }

    std::vector<std::byte> ReadFromFile(const fs::path& filePath)
    {
        std::ifstream file{ filePath.c_str(), std::ios::binary | std::ios::ate };
        BenzinAssert(file.good());

        const auto fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<std::byte> data;
        data.resize(fileSize);

        file.read(reinterpret_cast<char*>(data.data()), fileSize);
        file.close();

        return data;
    }

    void ClearDirectory(const fs::path& directoryPath)
    {
        for (const auto& entryPath : fs::directory_iterator(directoryPath))
        {
            fs::remove_all(entryPath);
        }
    }

    bool IsDestinationFileOlder(const std::filesystem::path& sourceFilePath, const std::filesystem::path& destinationFilePath)
    {
        BenzinAssert(fs::exists(sourceFilePath));

        if (!fs::exists(destinationFilePath))
        {
            return true;
        }

        const auto sourceTime = fs::last_write_time(sourceFilePath);
        const auto destinationTime = fs::last_write_time(destinationFilePath);

        return sourceTime > destinationTime;
    }

    fs::path GetShaderSourceFilePath(std::string_view fileName)
    {
        const fs::path shaderSourceFilePath{ config::g_AbsoluteShaderSourceDirectoryPath / fileName };
        BenzinAssert(fs::exists(shaderSourceFilePath));

        return shaderSourceFilePath;
    }

    fs::path GetShaderBinaryFilePath(size_t shaderKey)
    {
        return fs::path{ config::g_AbsoluteShaderBinaryDirectoryPath / std::format("{}.bin", shaderKey) };
    }

    fs::path GetShaderDebugFilePath(size_t shaderKey)
    {
        return fs::path{ config::g_AbsoluteShaderDebugDirectoryPath / std::format("{}.pdb", shaderKey) };
    }

} // namespace benzin
