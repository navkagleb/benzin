#pragma once

namespace benzin
{

    std::string CutExtension(std::string_view fileName);

    void WriteToFile(std::wstring_view fileName, std::span<const std::byte> data);
    void WriteToFile(std::wstring_view directory, std::wstring_view fileName, std::span<const std::byte> data);
    void WriteToFile(const std::filesystem::path& filePath, std::span<const std::byte> data);

    std::vector<std::byte> ReadFromFile(const std::filesystem::path& filePath);

    void ClearDirectory(const std::filesystem::path& directoryPath);

    bool IsDestinationFileOlder(const std::filesystem::path& sourceFilePath, const std::filesystem::path& destinationFilePath);

    std::filesystem::path GetShaderSourceFilePath(std::string_view fileName);
    std::filesystem::path GetShaderBinaryFilePath(size_t shaderKey);
    std::filesystem::path GetShaderDebugFilePath(size_t shaderKey);

} // namespace benzin
