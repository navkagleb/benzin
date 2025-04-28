local nuget_include_dirs = projects_dir .. "/packages/**/include"
local nuget_lib_dirs = projects_dir .. "/packages/**/bin/x64/"

local function apply_benzin_config()
    includedirs {
        source_dir,
        nuget_include_dirs,
    }

    filter "configurations:Debug"
        defines { "BENZIN_DEBUG_BUILD" }

    filter "configurations:Release"
        defines { "BENZIN_RELEASE_BUILD" }

    filter "platforms:Win64"
        systemversion "10.0.20348.0:latest" -- From Windows SDK 10.0.20348.0 shader model 6.6 support started

    filter {}
end

project "BenzinFramework"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"

    targetname "benzin_framework"

    fatalwarnings { "All" }

    import {
        ["ADL"] = "Anything",
        ["DirectXTex"] = "Anything",
        ["EnTT"] = "Anything",
        ["ImGui"] = "Anything",
        ["magic_enum"] = "Anything",
        ["meshoptimizer"] = "Anything",
        ["NvAPI"] = "Anything",
        ["TinyGLTF"] = "Anything",
    }

    nuget {
        "Microsoft.Direct3D.D3D12:1.715.1-preview",
        "Microsoft.Direct3D.DXC:1.7.2308.12",
        "WinPixEventRuntime:1.0.231030001",
    }

    defines {
        "BENZIN_PROJECT",
        "BENZIN_AGILE_SDK_VERSION=715",
        "BENZIN_AGILE_SDK_PATH=\"./D3D12\"",
    }

    libdirs {
        nuget_lib_dirs,
    }

    links {
        "d3d12.lib",
        "dxgi.lib",
        "dxguid.lib",
        "dxcompiler.lib",

        "WinPixEventRuntime.lib",
        "DirectXTex.lib",
        "nvapi64.lib",

        "ImGui",
        "meshoptimizer",
        "TinyGLTF",
        "Shaders",
    }

    files {
        "**.hpp",
        "**.inl",
        "**.cpp",
    }

    pchheader "benzin/config/bootstrap.hpp"
    pchsource "config/bootstrap.cpp"

    local add_folder_to_vpath = function(vpath_name)
        vpaths {
            [vpath_name] = {
                vpath_name .. "**.hpp",
                vpath_name .. "**.cpp",
            }
        }
    end

    add_folder_to_vpath("config")
    add_folder_to_vpath("core")
    add_folder_to_vpath("engine")
    add_folder_to_vpath("graphics2")
    add_folder_to_vpath("graphics/ray_tracing")
    add_folder_to_vpath("graphics")
    add_folder_to_vpath("system")
    add_folder_to_vpath("tools")
    add_folder_to_vpath("utility")

    apply_benzin_config()

    export "*"
        apply_benzin_config()

    export {}
