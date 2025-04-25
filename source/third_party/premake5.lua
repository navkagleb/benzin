project "ADL"
    kind "None"

    files {
        "adl/**.h",
    }

    export "*"
        includedirs {
            "adl",
        }

    export {}


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

    export "*"
        includedirs {
            "entt/single_include",
        }

    export {}


project "ImGui"
    function apply_config()
        includedirs {
            "imgui",
        }
    end

    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"

    files {
        "imgui/*.h",
        "imgui/*.cpp",
        "imgui/backends/imgui_impl_dx12.h",
        "imgui/backends/imgui_impl_dx12.cpp",
        "imgui/backends/imgui_impl_win32.h",
        "imgui/backends/imgui_impl_win32.cpp",
        "imgui/misc/cpp/imgui_stdlib.h",
        "imgui/misc/cpp/imgui_stdlib.cpp",
    }

    apply_config()

    export "*"
        apply_config()

    export {}


project "magic_enum"
    kind "None"

    files {
        "magic_enum/**.hpp",
    }

    export "*"
        includedirs {
            "magic_enum",
        }

    export {}


project "meshoptimizer"
    function apply_config()
        includedirs {
            "meshoptimizer/src",
        }
    end

    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"

    files {
        "meshoptimizer/src/**.h",
        "meshoptimizer/src/**.cpp",
    }

    apply_config()

    export "*"
        apply_config()

    export {}


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

    export "*"
        includedirs {
            "tinygltf",
        }

    export {}
