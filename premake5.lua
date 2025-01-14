local projects_dir = _WORKING_DIR .. "/source/generated_projects"

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

    filter "files:**.hlsl"
        buildaction "None"

    filter "files:**.hlsli"
        buildaction "None"


source_dir = _WORKING_DIR .. "/source"
third_party_dir = source_dir .. "/third_party"

nuget_include_dirs = projects_dir .. "/packages/**/include"
nuget_lib_dirs = projects_dir .. "/packages/**/bin/x64/"

print("source_dir: " .. source_dir)
print("third_party_dir: " .. third_party_dir)
print("nuget_include_dirs: " .. nuget_include_dirs)
print("nuget_lib_dirs: " .. nuget_lib_dirs)

third_party_include_dirs = {}
third_party_lib_dirs = {}
group "0_Dependencies"
    include "source/third_party"
group ""

group "1_Core"
    include "source/benzin"
    include "source/shaders"
group ""

include "source/sandbox"
