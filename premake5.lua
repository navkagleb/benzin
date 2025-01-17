require "export"

projects_dir = _WORKING_DIR .. "/source/generated_projects"

workspace "Benzin"
    -- DANGER: Don't use slash '/' as first symbol at value for 'location' function
    -- Otherwise generated project will be at the root of a disk
    location(projects_dir)

    configurations { "Debug", "Release" }
    platforms { "Win64" }

    targetdir "bin"
    objdir "build/%{prj.name}_%{cfg.buildcfg}"

    flags {
        "MultiProcessorCompile",
    }

    warnings "Extra"
    debugdir "./"

    startproject  "Sandbox"

    filter "platforms:Win64"
        system "Windows"
        architecture "x64"
        characterset "MBCS"

    filter "configurations:Debug"
        targetsuffix "_debug"
        optimize "Off"

        defines { "DEBUG" }

    filter "configurations:Release"
        targetsuffix "_release"
        optimize "On"

        defines { "NDEBUG" }

    filter "files:**.hlsl or **.hlsli"
        buildaction "None"

    filter {}


source_dir = _WORKING_DIR .. "/source"
print("source_dir: " .. source_dir)

group "0_Dependencies"
    include "source/third_party"

group "1_Core"
    include "source/benzin"
    include "source/shaders"

group ""
    include "source/sandbox"
