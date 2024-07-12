-- Fix missing targets file issue in some C++ nuget packages
-- Ref: https://github.com/premake/premake-core/pull/2025

local solution_dir = "../"
local bin_dir = solution_dir .. "bin/"
local build_dir = solution_dir .. "build/"
local source_dir = solution_dir .. "source/"
local projects_dir = source_dir .. "generated_projects/"
local packages_dir = projects_dir .. "packages/" -- nuget packages


local third_party_source_dir = source_dir .. "third_party/"


workspace "Benzin"
    location(projects_dir)

    platforms { "Win64" }
    configurations { "Debug", "Release" }

    startproject "3_Sandbox"

    filter "platforms:Win64"
        -- From Windows SDK 10.0.20348.0 shader model 6.6 support started  
        systemversion "10.0.20348.0:latest"
        architecture "x64"
        characterset "MBCS"
        linkoptions { "/ENTRY:mainCRTStartup" }

        defines {
            "BENZIN_PLATFORM_WIN64",
            "WIN32",
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


local cpp_language = "C++"
local cpp_version = "C++latest" -- Included C++23 features

local benzin_projects_warning_level = "Extra"
local warnings_as_errors_flag = "FatalWarnings"
local multi_processor_compile_flag = "MultiProcessorCompile"


project "0_ThirdParty"
    kind "StaticLib"
    language(cpp_language)
    cppdialect(cpp_version)
    location(projects_dir)

    targetname "third_party"
    targetdir(bin_dir)
    objdir(build_dir .. "/%{prj.name}/%{cfg.buildcfg}")

    flags {
        multi_processor_compile_flag,
    }

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


project "1_BenzinFramework"
    local project_source_dir = source_dir .. "benzin/"

    kind "StaticLib"
    language(cpp_language)
    cppdialect(cpp_version)
    location(projects_dir)

    targetname "benzin_framework"
    targetdir(bin_dir)
    objdir(build_dir .. "%{prj.name}/%{cfg.buildcfg}")

    warnings(benzin_projects_warning_level)

    flags {
        multi_processor_compile_flag,
        warnings_as_errors_flag,
    }

    pchheader "benzin/config/bootstrap.hpp"
    pchsource(project_source_dir .. "config/bootstrap.cpp")

    links {
        "0_ThirdParty",
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
        project_source_dir .. "**.hpp",
        project_source_dir .. "**.inl",
        project_source_dir .. "**.cpp",
    }

    includedirs {
        packages_dir .. "**/include",
        source_dir,
    }


project "2_Shaders"
    kind "None"
    location(projects_dir)

    targetdir(bin_dir)
    objdir(build_dir .. "/%{prj.name}/%{cfg.buildcfg}")

    files {
        source_dir .. "shaders/**.hpp",
        source_dir .. "shaders/**.hlsl",
        source_dir .. "shaders/**.hlsli",
    }


project "3_Sandbox"
    local project_source_dir = source_dir .. "sandbox/"

    kind "ConsoleApp"
    language(cpp_language)
    cppdialect(cpp_version)
    location(projects_dir)

    targetname "sandbox"
    targetdir(bin_dir)
    objdir(build_dir .. "%{prj.name}/%{cfg.buildcfg}")

    debugdir(solution_dir)

    warnings(benzin_projects_warning_level)

    flags {
        multi_processor_compile_flag,
        warnings_as_errors_flag,
    }

    pchheader "sandbox/bootstrap.hpp"
    pchsource(project_source_dir .. "bootstrap.cpp")

    links {
        "1_BenzinFramework",
    }

    files {
        project_source_dir .. "**.hpp",
        project_source_dir .. "**.cpp",
    }

    includedirs {
        packages_dir .. "**/include",
        source_dir,
    }

    libdirs {
        packages_dir .. "**/bin/x64/",
        third_party_source_dir .. "nvapi/amd64",
    }
