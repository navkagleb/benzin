#pragma once

namespace benzin
{

    struct Mesh;

    void RegroupMesh(Mesh& mesh);
    void OptimizeMesh(Mesh& mesh);
    void GenerateMeshlets(Mesh& mesh);
    void GenerateBoundingSpheres(Mesh& mesh);

    bool SaveTextureArrayToDds(std::span<const std::string_view> fileNames, std::string_view outputFileName);

}
