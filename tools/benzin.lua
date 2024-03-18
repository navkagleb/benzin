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

local third_party_source_dir = source_dir .. "third_party/"
local benzin_source_dir = source_dir .. "benzin/"
local sandbox_source_dir = source_dir .. "sandbox/"
local shaders_source_dir = source_dir .. "shaders/"

-- Included C++23 features
local cpp_version = "C++latest"

project "third_party"
    kind "StaticLib"
    language "C++"
    cppdialect(cpp_version)
    location(third_party_source_dir)

    targetdir(bin_dir)
    objdir(build_dir .. "/%{prj.name}/%{cfg.buildcfg}")

    files {
        third_party_source_dir .. "adl/**.h",
        third_party_source_dir .. "directx/**.cpp",
        third_party_source_dir .. "directx/**.h",
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

local vs_solution_dir = "$(SolutionDir)"
local vs_bin_dir = vs_solution_dir .. "bin/"
local vs_packages_dir = vs_solution_dir .. "packages/"

function create_nuget(name, version)
    return {
        id =  name .. ":" .. version,
        vs_dir = vs_packages_dir .. name .. "." .. version .. "/",
    }
end

function vs_copy_file_command(dir, file_path)
    string.format('xcopy /f /Y /D "%s" "%s"', file_path, dir)
end

project "benzin"
    kind "StaticLib"
    language "C++"
    cppdialect(cpp_version)
    location(benzin_source_dir)

    targetdir(bin_dir)
    objdir(build_dir .. "%{prj.name}/%{cfg.buildcfg}")

    pchheader "benzin/config/bootstrap.hpp"
    pchsource(benzin_source_dir .. "config/bootstrap.cpp")

    defines {
        "BENZIN_PROJECT",
        "BENZIN_AGILE_SDK_VERSION=711",
        "BENZIN_AGILE_SDK_PATH=\"./\"",
    }

    files {
        benzin_source_dir .. "**.hpp",
        benzin_source_dir .. "**.inl",
        benzin_source_dir .. "**.cpp",
    }

    includedirs {
        solution_dir,
        source_dir,
        location_dir,
    }

    local agile_sdk_nuget = create_nuget("Microsoft.Direct3D.D3D12", "1.711.3-preview")
    local dxc_nuget = create_nuget("Microsoft.Direct3D.DXC", "1.7.2308.12")

    nuget {
        agile_sdk_nuget.id,
        dxc_nuget.id,
    }

    local agile_sdk_bin_path = agile_sdk_nuget.vs_dir .. "build/native/bin/x64/"
    local dxc_bin_path = dxc_nuget.vs_dir .. "build/native/bin/x64/"

    postbuildcommands {
        vs_copy_file_command(vs_bin_dir, agile_sdk_bin_path .. "D3D12Core.dll"),
        vs_copy_file_command(vs_bin_dir, agile_sdk_bin_path .. "D3D12Core.pdb"),
        vs_copy_file_command(vs_bin_dir, agile_sdk_bin_path .. "d3d12SDKLayers.dll"),
        vs_copy_file_command(vs_bin_dir, agile_sdk_bin_path .. "d3d12SDKLayers.pdb"),
        vs_copy_file_command(vs_bin_dir, dxc_bin_path .. "dxcompiler.dll"),
        vs_copy_file_command(vs_bin_dir, dxc_bin_path .. "dxcompiler.pdb"),
        vs_copy_file_command(vs_bin_dir, dxc_bin_path .. "dxil.dll"),
        vs_copy_file_command(vs_bin_dir, dxc_bin_path .. "dxil.pdb"),
    }

project "sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect(cpp_version)
    location(sandbox_source_dir)

    targetdir(bin_dir)
    objdir(build_dir .. "%{prj.name}/%{cfg.buildcfg}")

    debugdir(solution_abs_dir)

    links {
        "third_party",
        "benzin",
    }

    libdirs {
        third_party_source_dir .. "nvapi/amd64",
    }

    pchheader "bootstrap.hpp"
    pchsource(sandbox_source_dir .. "bootstrap.cpp")

    files {
        sandbox_source_dir .. "**.hpp",
        sandbox_source_dir .. "**.inl",
        sandbox_source_dir .. "**.cpp",

        shaders_source_dir .. "**.hpp",
        shaders_source_dir .. "**.hlsl",
        shaders_source_dir .. "**.hlsli",
    }

    includedirs {
        solution_dir,
        source_dir,
        sandbox_source_dir,
    }

    vpaths {
	    ["shaders/*"] = {
            shaders_source_dir .. "**.hlsl",
            shaders_source_dir .. "**.hlsli"
        }
	}
