project "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"

    targetname "sandbox"

    fatalwarnings { "All" }

    import {
        ["BenzinFramework"] = "Anything",
    }

    includedirs {
        source_dir,
    }

    links {
        "BenzinFramework",
    }

    files {
        "**.hpp",
        "**.cpp",
    }

    pchheader "sandbox/bootstrap.hpp"
    pchsource "bootstrap.cpp"
