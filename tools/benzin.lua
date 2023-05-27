workspace "benzin"
    location "../"

    platforms { "win64" }
    configurations { "debug", "release" }

    startproject "sandbox"

    filter "platforms:win64"
        -- From Windows SDK 10.0.20348.0 shader model 6.6 support started  
        systemversion "10.0.20348.0:latest"
        architecture "x64"
        characterset "MBCS"
		linkoptions { "/ENTRY:mainCRTStartup" }

        defines {
            "BENZIN_PLATFORM_WINDOWS",
            "WIN32",
        }

    filter "configurations:debug"
        defines {
            "DEBUG",
            "BENZIN_DEBUG_BUILD"
        }
        optimize "Off"

    filter "configurations:release"
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
    cppdialect "C++20"
    location "../source/third_party"

    targetname "third_party_%{cfg.buildcfg}"

    targetdir "../lib"
    objdir "../build/third_party/%{cfg.buildcfg}"

    files {
        -- directx
        "../source/third_party/directx/**.h",
        "../source/third_party/directx/**.cpp",

        -- entt
        "../source/third_party/entt/**.h",
        "../source/third_party/entt/**.hpp",
        "../source/third_party/entt/**.cpp",

        -- dxc
        -- "../source/third_party/dxc/**.h",

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

    targetname "benzin_%{cfg.buildcfg}"

    targetdir "../lib"
    objdir "../build/benzin/%{cfg.buildcfg}"

    pchheader "benzin/config/bootstrap.hpp"
    pchsource "../source/benzin/config/bootstrap.cpp"

    defines { "BENZIN_PROJECT" }

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

project "sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    location "../source/sandbox"

    targetname "sandbox_%{cfg.buildcfg}"

    targetdir "../bin"
    objdir "../build/sandbox/%{cfg.buildcfg}"

    links {
        "third_party",
        "benzin",
    }

    pchheader "bootstrap.hpp"
    pchsource "../source/sandbox/bootstrap.cpp"

    files {
        "../source/sandbox/**.hpp",
        "../source/sandbox/**.inl",
        "../source/sandbox/**.cpp",

        "../source/shaders/**.hlsl",
        "../source/shaders/**.hlsli",
    }

    includedirs {
        "../source/",
        "../source/sandbox",
    }

    vpaths {
	    ["shaders/*"] = { "../source/shaders/**.hlsl", "../source/shaders/**.hlsli" }
	}
