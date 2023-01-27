workspace "benzin"
    location "../"

    platforms { "Win64_D3D12" }

    configurations
    {
        "Debug",
        -- "Release",
        -- "Final"
    }

    startproject "sandbox"

    filter "platforms:Win64_D3D12"
        system "windows"
        systemversion "latest"
        architecture "x64"
        characterset "MBCS"

        linkoptions { "/ENTRY:mainCRTStartup" }
        defines { "BENZIN_PLATFORM_WINDOWS", "WIN32", }

    filter "configurations:Debug"
        defines { "DEBUG", "BENZIN_DEBUG" }
        optimize "Off"

    filter "files:**.hlsl"
        buildaction "None"

--[[
    filter "configurations:Release"
        defines { "NDEBUG", "BENZIN_RELEASE" }
        optimize "On"

    filter "configurations:Final"
        defines { "NDEBUG", "BENZIN_FINAL" }
        optimize "On"

    filter { "files:**.hlsl" }
		buildaction "None"
--]]

project "third_party"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    location "../third_party"
    
    targetdir "../build"
    objdir "../build/third_party/%{cfg.platform}_%{cfg.buildcfg}"

    files
    {
        -- directx
        "../third_party/directx/**.h",
        "../third_party/directx/**.cpp",

        --fmt
        "../third_party/fmt/**.h",
        "../third_party/fmt/**.cc",

        -- imgui
        "../third_party/imgui/**.h",
        "../third_party/imgui/**.cpp",

        -- magic_enum
        "../third_party/magic_enum/*.hpp"
    }

project "benzin"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    location "../benzin"

    targetdir "../build"
    objdir "../build/benzin/%{cfg.platform}_%{cfg.buildcfg}"

    pchheader "benzin/config/bootstrap.hpp"
    pchsource "../benzin/src/config/bootstrap.cpp"

    files
    {
        "../benzin/include/benzin/**.hpp",
        "../benzin/include/benzin/**.inl",
        "../benzin/src/**.hpp",

        "../benzin/src/**.cpp"
    }

    includedirs
    {
        "../",
        "../benzin/include",
        "../benzin/src"
    }

project "sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    location "../sandbox"

    targetdir "../build"
    objdir "../build/sandbox/%{cfg.platform}_%{cfg.buildcfg}"

    links
    {
        "third_party",
        "benzin"
    }

    pchheader "bootstrap.hpp"
    pchsource "../sandbox/src/bootstrap.cpp"

    files
    {
        "../sandbox/src/**.hpp",
        "../sandbox/src/**.inl",

        "../sandbox/src/**.cpp",

        "../sandbox/assets/**.hlsl"
    }

    includedirs
    {
        "../",
        "../benzin/include",
        "../sandbox/src"
    }
