#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/pix_capturer.hpp>

#include <ShlObj.h>

#include <benzin/core/cmd_line_args.hpp>

namespace benzin
{

    static constexpr std::wstring_view g_PixCapturerDllName = L"WinPixGpuCapturer.dll";

    static std::filesystem::path GetLatestWinPixGpuCapturerPath()
    {
        // Ref: https://devblogs.microsoft.com/pix/taking-a-capture/

        wchar_t* programFilesPath = nullptr;
        ::SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, nullptr, &programFilesPath);

        std::filesystem::path pixInstallationPath = programFilesPath;
        pixInstallationPath /= "Microsoft PIX";

        std::wstring pixNewestVersion;
        for (const auto& directoryEntry : std::filesystem::directory_iterator{ pixInstallationPath })
        {
            if (!directoryEntry.is_directory())
            {
                continue;
            }

            if (pixNewestVersion.empty() || pixNewestVersion < directoryEntry.path().filename().c_str())
            {
                pixNewestVersion = directoryEntry.path().filename().c_str();
            }
        }

        BenzinEnsure(!pixNewestVersion.empty());
        return pixInstallationPath / pixNewestVersion / g_PixCapturerDllName;
    }

    //

    void PixCapturer::Initialize()
    {
        if (!CmdLineArgs::IsPixCapturerEnabled())
        {
            return;
        }

        if (::GetModuleHandleW(g_PixCapturerDllName.data()) == nullptr)
        {
            const std::filesystem::path pixCapturerPath = GetLatestWinPixGpuCapturerPath();

            const HMODULE pixHandle = ::LoadLibraryW(pixCapturerPath.c_str());
            BenzinEnsure(pixHandle != nullptr);

            BenzinTrace("PixGpuCapturer DLL loaded. FilePath: {}", pixCapturerPath.string());
        }
    }

    void PixCapturer::Shutdown()
    {
        if (!CmdLineArgs::IsPixCapturerEnabled())
        {
            return;
        }

        const HMODULE pixHandle = ::GetModuleHandleW(g_PixCapturerDllName.data());
        if (pixHandle != nullptr)
        {
            ::FreeLibrary(pixHandle);
            BenzinTrace("PixGpuCapturer DLL unloaded");
        }
    }

}
