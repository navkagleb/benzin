project "ADL"
    kind "None"

    files {
        "adl/**.h",
    }


project "DirectXTex"
    kind "None"

    files {
        "DirectXTex/include/DirectX.h",
        "DirectXTex/include/DirectX.inl",
    }

    export "*"
        includedirs {
            "DirectXTex/include",
        }

        libdirs {
            "DirectXTex/lib",
        }

    export {}


project "EnTT"
    kind "None"

    files {
        "entt/**.h",
        "entt/**.hpp",
    }


project "ImGui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"

    files {
        "imgui/**.h",
        "imgui/**.cpp",
    }


project "magic_enum"
    kind "None"

    files {
        "magic_enum/**.hpp",
    }


project "NvAPI"
    kind "None"

    files {
        "nvapi/include/**.h",
    }

    export "*"
        includedirs {
            "nvapi/include",
        }

        libdirs {
            "nvapi/amd64",
        }

    export {}


project "TinyGLTF"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"

    files {
        "tinygltf/**.h",
        "tinygltf/**.cc",
    }
