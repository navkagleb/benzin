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


project "TinyGLTF"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"

    files {
        "tinygltf/**.h",
        "tinygltf/**.cc",
    }


third_party_include_dirs["DirectXTex"] = third_party_dir .. "/DirectXTex/include"
third_party_lib_dirs["DirectXTex"] = third_party_dir .. "/DirectXTex/lib"

third_party_include_dirs["nvapi"] = third_party_dir .. "/nvapi/include"
third_party_lib_dirs["nvapi"] = third_party_dir .. "/nvapi/amd64"
