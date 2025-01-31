project "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"

    targetname "sandbox"

    fatalwarnings { "All" }

    import {
        ["EnTT"] = "Anything",
        ["ImGui"] = "Anything",
        ["magic_enum"] = "Anything",

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
