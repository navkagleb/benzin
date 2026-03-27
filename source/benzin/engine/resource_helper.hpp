#pragma once

namespace benzin
{

    struct MeshGeometry;

    void OptimizeMeshGeometry(MeshGeometry& geometry);
    void GenerateMeshlets(MeshGeometry& geometry);

    bool SaveTextureArrayToDds(std::span<const std::string_view> fileNames, std::string_view outputFileName);

}
