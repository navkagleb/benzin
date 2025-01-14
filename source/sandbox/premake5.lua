project "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"

    targetname "sandbox"

    fatalwarnings { "All" }

    pchheader "sandbox/bootstrap.hpp"
    pchsource "bootstrap.cpp"

    includedirs {
        source_dir,
        nuget_include_dirs,
    }

    links {
        "BenzinFramework",
    }

    files {
        "**.hpp",
        "**.cpp",
    }

    -- TODO: Inheritance from BenzinFramework?
    filter "configurations:Debug"
        defines { "BENZIN_DEBUG_BUILD" }

    filter "configurations:Release"
        defines { "BENZIN_RELEASE_BUILD" }

    filter "platforms:Win64"
        defines {
            "BENZIN_PLATFORM_WIN64",
        }
