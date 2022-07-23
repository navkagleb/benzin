workspace "spieler"
    location "../"

    platforms { "win64_dx12" }

    configurations
    {
        "Debug",
        -- "Release",
        -- "Final"
    }

    startproject "sandbox"

    filter "platforms:win64_dx12"
        system "windows"
        systemversion "latest"
        architecture "x64"
        characterset "ASCII"

        defines { "SPIELER_PLATFORM_WINDOWS", "WIN32", "SPIELER_GRAPHICS_API_DX12" }

    filter "configurations:Debug"
        defines { "DEBUG", "SPIELER_DEBUG" }
        optimize "Off"
        
        staticruntime "on"
        runtime "Debug"

--[[
    filter "configurations:Release"
        defines { "NDEBUG", "SPIELER_RELEASE" }
        optimize "On"

    filter "configurations:Final"
        defines { "NDEBUG", "SPIELER_FINAL" }
        optimize "On"

    filter { "files:**.hlsl" }
		buildaction "None"
--]]


outputdir = "%{cfg.buildcfg}/%{cfg.system}/%{cfg.architecture}"

project "third_party"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    location "../third_party"

    targetdir ("../bin/" ..outputdir)
    objdir ("../bin/int/" .. outputdir)

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

project "spieler"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    location "../spieler"

    targetdir ("../bin/" .. outputdir)
    objdir ("../bin/int/" .. outputdir)

    pchheader "spieler/config/bootstrap.hpp"
    pchsource "../spieler/src/config/bootstrap.cpp"

    files
    {
        "../spieler/include/spieler/**.hpp",
        "../spieler/include/spieler/**.inl",
        "../spieler/src/**.hpp",

        "../spieler/src/**.cpp"
    }

    includedirs
    {
        "../",
        "../spieler/include",
        "../spieler/src"
    }

project "sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    location "../sandbox"

    targetdir ("../bin/" .. outputdir)
    objdir ("../bin/int/" .. outputdir)

    links
    {
        "third_party",
        "spieler"
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
        "../spieler/include",
        "../sandbox/src"
    }

    filter "files:**.hlsl"
        buildaction "None"
