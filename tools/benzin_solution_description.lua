-- Fix missing targets file issue in some C++ nuget packages
-- Ref: https://github.com/premake/premake-core/pull/2025

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
            "BENZIN_PLATFORM_WIN64",
            "WIN32",
            "USE_PIX", -- For WinPixEventRuntime
        }

    filter "configurations:Debug"
        targetsuffix "_debug"
        optimize "Off"

        defines {
            "DEBUG",
            "BENZIN_DEBUG_BUILD"
        }

    filter "configurations:Release"
        targetsuffix "_release"
        optimize "On"

        defines {
            "NDEBUG",
            "BENZIN_RELEASE_BUILD",
        }

    filter "files:**.hlsl"
        buildaction "None"

    filter "files:**.hlsli"
        buildaction "None"

local solution_dir = "../"
local bin_dir = solution_dir .. "bin/"
local build_dir = solution_dir .. "build/"
local source_dir = solution_dir .. "source/"
local packages_dir = solution_dir .. "packages/"

local third_party_source_dir = source_dir .. "third_party/"
local benzin_source_dir = source_dir .. "benzin/"
local sandbox_source_dir = source_dir .. "sandbox/"
local shaders_source_dir = source_dir .. "shaders/"

local cpp_language = "C++"
local cpp_version = "C++latest" -- Included C++23 features

project "third_party"
    kind "StaticLib"
    language(cpp_language)
    cppdialect(cpp_version)
    location(third_party_source_dir)

    targetdir(bin_dir)
    objdir(build_dir .. "/%{prj.name}/%{cfg.buildcfg}")

    files {
        third_party_source_dir .. "adl/**.h",
        third_party_source_dir .. "entt/**.cpp",
        third_party_source_dir .. "entt/**.h",
        third_party_source_dir .. "entt/**.hpp",
        third_party_source_dir .. "imgui/**.cpp",
        third_party_source_dir .. "imgui/**.h",
        third_party_source_dir .. "magic_enum/**.hpp",
        third_party_source_dir .. "nvapi/**.h",
        third_party_source_dir .. "tinygltf/**.cc",
        third_party_source_dir .. "tinygltf/**.h",
        third_party_source_dir .. "tinygltf/**.hpp",
    }

project "benzin"
    kind "StaticLib"
    language(cpp_language)
    cppdialect(cpp_version)
    location(benzin_source_dir)

    targetdir(bin_dir)
    objdir(build_dir .. "%{prj.name}/%{cfg.buildcfg}")

    pchheader "benzin/config/bootstrap.hpp"
    pchsource(benzin_source_dir .. "config/bootstrap.cpp")

    links {
        "third_party",
    }

    nuget {
        "Microsoft.Direct3D.D3D12:1.711.3-preview",
        "Microsoft.Direct3D.DXC:1.7.2308.12",
        "WinPixEventRuntime:1.0.231030001",
    }

    defines {
        "BENZIN_PROJECT",
        "BENZIN_AGILE_SDK_VERSION=711",
        "BENZIN_AGILE_SDK_PATH=\"./D3D12\"",
    }

    files {
        benzin_source_dir .. "**.hpp",
        benzin_source_dir .. "**.inl",
        benzin_source_dir .. "**.cpp",
    }

    includedirs {
        packages_dir .. "**/include",
        source_dir,
    }

project "sandbox"
    kind "ConsoleApp"
    language(cpp_language)
    cppdialect(cpp_version)
    location(sandbox_source_dir)

    targetdir(bin_dir)
    objdir(build_dir .. "%{prj.name}/%{cfg.buildcfg}")

    pchheader "bootstrap.hpp"
    pchsource(sandbox_source_dir .. "bootstrap.cpp")

    links {
        "benzin",
    }

    files {
        sandbox_source_dir .. "**.hpp",
        sandbox_source_dir .. "**.inl",
        sandbox_source_dir .. "**.cpp",

        shaders_source_dir .. "**.hpp",
        shaders_source_dir .. "**.hlsl",
        shaders_source_dir .. "**.hlsli",
    }

    includedirs {
        packages_dir .. "**/include",
        source_dir,
    }

    libdirs {
        packages_dir .. "**/bin/x64/",
        third_party_source_dir .. "nvapi/amd64",
    }

    vpaths {
	    ["shaders/*"] = {
            shaders_source_dir .. "**.hlsl",
            shaders_source_dir .. "**.hlsli"
        }
	}
