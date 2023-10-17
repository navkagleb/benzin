workspace "benzin"
    location "../"

    platforms { "Win64" }
    configurations { "Debug", "Release" }

    startproject "sandbox"

    filter "platforms:Win64"
        -- From Windows SDK 10.0.20348.0 shader model 6.6 support started  
        systemversion "10.0.20348.0:latest"
        architecture "x64"
        characterset "MBCS"
		linkoptions { "/ENTRY:mainCRTStartup" }

        defines {
            "BENZIN_PLATFORM_WINDOWS",
            "WIN32",
        }

    filter "configurations:Debug"
        targetsuffix "_debug"
        defines {
            "DEBUG",
            "BENZIN_DEBUG_BUILD"
        }
        optimize "Off"

    filter "configurations:Release"
        targetsuffix "_release"
        defines {
            "NDEBUG",
            "BENZIN_RELEASE_BUILD",
        }
        optimize "On"

    filter "files:**.hlsl"
        buildaction "None"

    filter "files:**.hlsli"
        buildaction "None"

project "third_party"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest" -- Included C++23 features
    location "../source/third_party"

    targetdir "../bin"
    objdir "../build/third_party/%{cfg.buildcfg}"

    files {
        -- directx
        "../source/third_party/directx/**.h",
        "../source/third_party/directx/**.cpp",

        -- directxmesh

        -- entt
        "../source/third_party/entt/**.h",
        "../source/third_party/entt/**.hpp",
        "../source/third_party/entt/**.cpp",

        --fmt
        "../source/third_party/fmt/**.h",
        "../source/third_party/fmt/**.cc",

        -- imgui
        "../source/third_party/imgui/**.h",
        "../source/third_party/imgui/**.cpp",

        -- magic_enum
        "../source/third_party/magic_enum/*.hpp",

        -- tinygltf
        "../source/third_party/tinygltf/*.h",
        "../source/third_party/tinygltf/*.hpp",
        "../source/third_party/tinygltf/*.cc",
    }

project "benzin"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    location "../source/benzin"

    targetdir "../bin"
    objdir "../build/benzin/%{cfg.buildcfg}"

    pchheader "benzin/config/bootstrap.hpp"
    pchsource "../source/benzin/config/bootstrap.cpp"

    defines { "BENZIN_PROJECT" }

    nuget {
        "Microsoft.Direct3D.D3D12:1.610.3",
        "directxmesh_desktop_win10:2023.4.28.1",
        -- "WinPixEventRuntime:1.0.230302001"
    }

    files {
        "../source/benzin/**.hpp",
        "../source/benzin/**.inl",
        "../source/benzin/**.cpp",
    }

    includedirs {
        "../",
        "../source",
        "../source/benzin",
    }

    prebuildcommands {
        "powershell -ExecutionPolicy Bypass -File $(SolutionDir)/tools/get_latest_dxc.ps1 $(SolutionDir)packages/dxc"
    }

    postbuildcommands {
        -- DirectX Agile SDK
        "xcopy /f /Y /D \"$(SolutionDir)packages/Microsoft.Direct3D.D3D12.1.610.3/build/native/bin/x64/D3D12Core.dll\" \"$(SolutionDir)bin\"",
        "xcopy /f /Y /D \"$(SolutionDir)packages/Microsoft.Direct3D.D3D12.1.610.3/build/native/bin/x64/D3D12Core.pdb\" \"$(SolutionDir)bin\"",
        "xcopy /f /Y /D \"$(SolutionDir)packages/Microsoft.Direct3D.D3D12.1.610.3/build/native/bin/x64/d3d12SDKLayers.dll\" \"$(SolutionDir)bin\"",
        "xcopy /f /Y /D \"$(SolutionDir)packages/Microsoft.Direct3D.D3D12.1.610.3/build/native/bin/x64/d3d12SDKLayers.pdb\" \"$(SolutionDir)bin\"",

        -- DXC
        "xcopy /f /Y /D \"$(SolutionDir)packages/DXC/bin/x64/dxcompiler.dll\" \"$(SolutionDir)bin\"",
        "xcopy /f /Y /D \"$(SolutionDir)packages/DXC/bin/x64/dxil.dll\" \"$(SolutionDir)bin\"",
    }

project "sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    location "../source/sandbox"

    targetdir "../bin"
    objdir "../build/sandbox/%{cfg.buildcfg}"

    debugdir "$(SolutionDir)"

    links {
        "third_party",
        "benzin",
    }

    pchheader "bootstrap.hpp"
    pchsource "../source/sandbox/bootstrap.cpp"

    nuget {
        "Microsoft.Direct3D.D3D12:1.610.3",
        "directxmesh_desktop_win10:2023.4.28.1",
    }

    files {
        "../source/sandbox/**.hpp",
        "../source/sandbox/**.inl",
        "../source/sandbox/**.cpp",

        "../source/shaders/**.hlsl",
        "../source/shaders/**.hlsli",
    }

    includedirs {
        "../",
        "../source/",
        "../source/sandbox",
    }

    vpaths {
	    ["shaders/*"] = { "../source/shaders/**.hlsl", "../source/shaders/**.hlsli" }
	}
