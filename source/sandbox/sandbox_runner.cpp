#include <sandbox/bootstrap.hpp>
#include <sandbox/sandbox_runner.hpp>

#include <benzin/core/logger.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/tools/render_viewport_tool.hpp>
#include <benzin/tools/texture_viewer_tool.hpp>
#include <benzin/utility/random.hpp>

#include <shaders/joint/mesh_types.hpp>

#include <sandbox/render_passes/deferred_lighting_pass.hpp>
#include <sandbox/render_passes/environment_pass.hpp>
#include <sandbox/render_passes/geometry_pass.hpp>
#include <sandbox/render_passes/global_consts_pass.hpp>
#include <sandbox/render_passes/procedural_grass_pass.hpp>
#include <sandbox/render_passes/ray_tracing_shadow_pass.hpp>
#include <sandbox/render_passes/sigma_denoiser_pass.hpp>
#include <sandbox/render_passes/tlas_building_pass.hpp>
#include <sandbox/render_passes/tone_mapping_pass.hpp>
#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

BenzinEnableUnaryPlusForEnum(joint::ReadbackStat);

namespace sandbox
{

    // SandboxRunner

    SandboxRunner::~SandboxRunner()
    {
        m_ImGuiManager->UnregisterTool<SettingsTool<GBufferSettings>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<GBufferStats>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<ProceduralGrassSettings>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<ProceduralGrassStats>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<RayTracing_ShadowSettings>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<SigmaDenoiserSettings>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<ToneMappingSettings>>();
    }

    void SandboxRunner::InitRenderPasses()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitRenderPasses");

        auto readbackStatsCallback = [this](std::span<const uint32_t> readbackStats)
        {
            {
                auto& stats = m_RenderSettings->GetSection<GBufferStats>();

                stats.m_TotalMeshletCount = readbackStats[+joint::ReadbackStat::Geometry_TotalMeshletCount];
                stats.m_TotalMeshletVertexCount = readbackStats[+joint::ReadbackStat::Geometry_TotalMeshletVertexCount];
                stats.m_TotalMeshletTriangleCount = readbackStats[+joint::ReadbackStat::Geometry_TotalMeshletTriangleCount];

                stats.m_MeshletCount = readbackStats[+joint::ReadbackStat::Geometry_MeshletCount];
                stats.m_MeshletVertexCount = readbackStats[+joint::ReadbackStat::Geometry_MeshletVertexCount];
                stats.m_MeshletTriangleCount = readbackStats[+joint::ReadbackStat::Geometry_MeshletTriangleCount];

                stats.m_VsInvocationCount = readbackStats[+joint::ReadbackStat::Geometry_VsInvocationCount];
                stats.m_AsInvocationCount = readbackStats[+joint::ReadbackStat::Geometry_AsInvocationCount];  
                stats.m_MsInvocationCount = readbackStats[+joint::ReadbackStat::Geometry_MsInvocationCount];
                stats.m_PsInvocationCount = readbackStats[+joint::ReadbackStat::Geometry_PsInvocationCount];
            }

            {
                auto& stats = m_RenderSettings->GetSection<ProceduralGrassStats>();
                stats.PatchCount = readbackStats[+joint::ReadbackStat::ProceduralGrass_PatchCount];
                stats.BladeCount = readbackStats[+joint::ReadbackStat::ProceduralGrass_BladeCount];
                stats.VertexCount = readbackStats[+joint::ReadbackStat::ProceduralGrass_VertexCount];
                stats.TriangleCount = readbackStats[+joint::ReadbackStat::ProceduralGrass_TriangleCount];
            }
        };

        // The order in which render passes are added is important
        m_RenderPasses.push_back(std::make_unique<TlasBuildingPass>());
        m_RenderPasses.push_back(std::make_unique<GlobalConstsPass>(std::move(readbackStatsCallback)));
        m_RenderPasses.push_back(std::make_unique<GeometryPass>());
        m_RenderPasses.push_back(std::make_unique<ProceduralGrassPass>());
        m_RenderPasses.push_back(std::make_unique<RayTracing_ShadowPass>());
        m_RenderPasses.push_back(std::make_unique<SigmaDenoiserPass>());
        m_RenderPasses.push_back(std::make_unique<DeferredLightingPass>());
        m_RenderPasses.push_back(std::make_unique<EnvironmentPass>());
        m_RenderPasses.push_back(std::make_unique<ToneMappingPass>());

        m_RenderSettings->GetSection<RayTracing_ShadowSettings>().IsEnabled = true;
        m_RenderSettings->GetSection<SigmaDenoiserSettings>().IsEnabled = true;
    }

    void SandboxRunner::InitTools()
    {
        m_ImGuiManager->RegisterTool<SettingsTool<GBufferSettings>>("Settings/GBuffer", m_RenderSettings->GetSection<GBufferSettings>());
        m_ImGuiManager->RegisterTool<SettingsTool<GBufferStats>>("Settings/GBufferStats", m_RenderSettings->GetSection<GBufferStats>());
        m_ImGuiManager->RegisterTool<SettingsTool<ProceduralGrassSettings>>("Settings/ProceduralGrass", m_RenderSettings->GetSection<ProceduralGrassSettings>());
        m_ImGuiManager->RegisterTool<SettingsTool<ProceduralGrassStats>>("Settings/ProceduralGrassStats", m_RenderSettings->GetSection<ProceduralGrassStats>());
        m_ImGuiManager->RegisterTool<SettingsTool<RayTracing_ShadowSettings>>("Settings/RayTracing_Shadow", m_RenderSettings->GetSection<RayTracing_ShadowSettings>());
        m_ImGuiManager->RegisterTool<SettingsTool<SigmaDenoiserSettings>>("Settings/SigmaDenoiser", m_RenderSettings->GetSection<SigmaDenoiserSettings>());
        m_ImGuiManager->RegisterTool<SettingsTool<ToneMappingSettings>>("Settings/ToneMapping", m_RenderSettings->GetSection<ToneMappingSettings>());
    }

    // SponzaRunner

    void SponzaRunner::InitScene()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitScene");

        constexpr auto meshFileNames = std::to_array<std::string_view>(
        {
            "Sponza/glTF/Sponza.gltf",
            "BoomBox/glTF-Binary/BoomBox.glb",
            "DamagedHelmet/glTF/DamagedHelmet.gltf",
            "OrientationTest/OrientationTest.gltf",
            "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf",
        });

        for (const std::string_view fileName : meshFileNames)
        {
            benzin::Mesh mesh;
            std::vector<benzin::MeshDrawPart> meshDrawParts;
            std::vector<benzin::Material> materials;
            std::vector<benzin::TextureImage> textures;
            BenzinAssertExpr(benzin::LoadMeshFromGltfFile(fileName, mesh, meshDrawParts, materials, textures));

            const std::string debugName = benzin::CutExtension(fileName);
            m_Scene->AddMesh(debugName, std::move(mesh), std::move(meshDrawParts), std::move(materials), std::move(textures));
        }

        benzin::PerspectiveCamera& camera = m_Scene->m_Camera;
        camera.SetPosition({ -1.649f, 1.007f, -1.555f });
        camera.SetFrontDirection({ 0.769f, 0.129f, 0.627f });
        camera.SetLens(DirectX::XMConvertToRadians(90.0f), 16.0f / 9.0f, 0.05f);

        benzin::SunLight& sunLight = m_Scene->m_SunLight;
        sunLight.SetColor({ 1.0f, 1.0f, 0.7f });
        sunLight.SetIntensity(10.0f);

        m_Scene->m_UpdateCallbacks.push_back([this]
        {
            const auto animateSunAngle = [this](float minAngle, float maxAngle, float& angle, float& direction)
            {
                angle += direction * m_AnimationTimer.GetDeltaTimeInSec() * 0.01f;

                if (!(minAngle <= angle && angle <= maxAngle))
                {
                    direction *= -1.0f;
                }
            };

            static float elevationDirection = 1.0f;
            static float azimithDirection = 1.0f;

            float elevation = m_Scene->m_SunLight.GetElevationInRadians();
            float azimuth = m_Scene->m_SunLight.GetAzimuthInRadians();

            animateSunAngle(DirectX::XMConvertToRadians(41.0f), DirectX::XMConvertToRadians(52.0f), elevation, elevationDirection);
            animateSunAngle(DirectX::XMConvertToRadians(-2.0f), DirectX::XMConvertToRadians(3.0f), azimuth, azimithDirection);

            m_Scene->m_SunLight.SetElevationInRadians(elevation);
            m_Scene->m_SunLight.SetAzimuthInRadians(azimuth);
        });

        if (m_Scene->m_MeshRangeMap.contains("Sponza"))
        {
            benzin::MeshDraw& draw = m_Scene->m_MeshDraws.emplace_back();
            draw.m_MeshRangeIndex = m_Scene->m_MeshRangeMap.at("Sponza");
            draw.m_Translation.x = 5.0f;
            draw.m_Rotation.y = DirectX::XM_PI;
        }

        if (m_Scene->m_MeshRangeMap.contains("OrientationTest"))
        {
            benzin::MeshDraw& draw = m_Scene->m_MeshDraws.emplace_back();
            draw.m_MeshRangeIndex = m_Scene->m_MeshRangeMap.at("OrientationTest");
            draw.m_Translation = { 2.5f, 0.2f, -0.25f };
            draw.m_Scale = 0.05f;
        }

        if (m_Scene->m_MeshRangeMap.contains("MilkTruck"))
        {
            benzin::MeshDraw& draw = m_Scene->m_MeshDraws.emplace_back();
            draw.m_MeshRangeIndex = m_Scene->m_MeshRangeMap.at("MilkTruck");
            draw.m_Translation = { -1.5f, 0.2f, 0.5f };
            draw.m_Scale = 0.1f;
        }

        if (m_Scene->m_MeshRangeMap.contains("Cylinder"))
        {
            benzin::MeshDraw& draw = m_Scene->m_MeshDraws.emplace_back();
            draw.m_MeshRangeIndex = m_Scene->m_MeshRangeMap.at("Cylinder");
            draw.m_Translation = { -1.5f, 0.4f, -0.25f };
            draw.m_Scale = 0.1f; // actually { 0.1f, 1.5f, 0.1f }
        }

        if (m_Scene->m_MeshRangeMap.contains("BoomBox"))
        {
            const size_t drawIndex = m_Scene->m_MeshDraws.size();

            benzin::MeshDraw& draw = m_Scene->m_MeshDraws.emplace_back();
            draw.m_MeshRangeIndex = m_Scene->m_MeshRangeMap.at("BoomBox");
            draw.m_Translation.y = 0.6f;
            draw.m_Rotation.y = DirectX::XMConvertToRadians(45.0f);
            draw.m_Scale = 30.0f;

            m_Scene->m_UpdateCallbacks.emplace_back([this, drawIndex]
            {
                benzin::MeshDraw& draw = m_Scene->m_MeshDraws[drawIndex];
                draw.m_Rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                draw.m_Rotation.z += 0.0002f * m_AnimationTimer.GetDeltaTimeInMs();
            });
        }

        if (m_Scene->m_MeshRangeMap.contains("DamagedHelmet"))
        {
            const size_t drawIndex = m_Scene->m_MeshDraws.size();

            benzin::MeshDraw& draw = m_Scene->m_MeshDraws.emplace_back();
            draw.m_MeshRangeIndex = m_Scene->m_MeshRangeMap.at("DamagedHelmet");
            draw.m_Translation = { 1.0f, 0.5f, -0.5f };
            draw.m_Rotation.y = DirectX::XMConvertToRadians(45.0f);
            draw.m_Scale = 0.4f;

            m_Scene->m_UpdateCallbacks.emplace_back([this, drawIndex]
            {
                benzin::MeshDraw& draw = m_Scene->m_MeshDraws[drawIndex];
                draw.m_Rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                draw.m_Rotation.y -= 0.00015f * m_AnimationTimer.GetDeltaTimeInMs();
            });
        }

        // Procedural grass
        {
            const int32_t xRadius = 160;
            const int32_t zRadius = 70;
            const int32_t totalCount = (xRadius * 2 + 1) * (zRadius * 2 + 1);

            m_Scene->m_GrassPatches.reserve(totalCount);

            for (auto x = -xRadius; x <= xRadius; ++x)
            {
                for (auto z = -zRadius; z <= zRadius; ++z)
                {
                    const DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVECTOR
                    {
                        benzin::Random::Get<float>(-0.1f, 0.1f),
                        1.0f,
                        benzin::Random::Get<float>(-0.1f, 0.1f),
                        0.0f,
                    });

                    joint::GrassPatch& grassPatch = m_Scene->m_GrassPatches.emplace_back();
                    grassPatch.Pos.x = (float)x * 0.07f + 6.0f;
                    grassPatch.Pos.z = (float)z * 0.07f - 0.3f;
                    grassPatch.Height = benzin::Random::Get<float>(0.07f, 0.13f);
                    DirectX::XMStoreFloat3(&grassPatch.Normal, normal);
                }
            }
        }
    }

    // StanfordDragonRunner

    void StanfordDragonRunner::InitScene()
    {
        benzin::PerspectiveCamera& camera = m_Scene->m_Camera;
        camera.SetPosition({ -2.286f, 3.911f, -18.385f });
        camera.SetFrontDirection({ 0.149f, -0.185f, 0.972f });
        camera.SetLens(DirectX::XMConvertToRadians(90.0f), 16.0f / 9.0f, 0.05f);

        benzin::Mesh dragon;
        std::vector<benzin::MeshDrawPart> dragonDrawParts;
        std::vector<benzin::Material> dragonMaterials;
        std::vector<benzin::TextureImage> dragonTextures;
        BenzinAssertExpr(benzin::LoadMeshFromGltfFile(
            "StanfordDragon/StanfordDragon.glb",
            dragon,
            dragonDrawParts,
            dragonMaterials,
            dragonTextures));

        m_Scene->AddMesh(
            "StanfordDragon",
            std::move(dragon),
            std::move(dragonDrawParts),
            std::move(dragonMaterials),
            std::move(dragonTextures));

        const int32_t radius = 3;

        for (auto x = -radius; x <= radius; ++x)
        {
            for (auto y = -radius; y <= radius; ++y)
            {
                for (auto z = -radius; z <= radius; ++z)
                {
                    benzin::MeshDraw& meshDraw = m_Scene->m_MeshDraws.emplace_back();
                    meshDraw.m_MeshRangeIndex = m_Scene->m_MeshRangeMap.at("StanfordDragon");
                    meshDraw.m_Translation.x = (float)x * 2.5f;
                    meshDraw.m_Translation.y = (float)y * 2.5f;
                    meshDraw.m_Translation.z = (float)z * 2.5f;
                    meshDraw.m_Rotation.x = benzin::Random::Get<float>(0.0f, DirectX::XM_2PI);
                    meshDraw.m_Rotation.y = benzin::Random::Get<float>(0.0f, DirectX::XM_2PI);
                    meshDraw.m_Rotation.z = benzin::Random::Get<float>(0.0f, DirectX::XM_2PI);
                    meshDraw.m_Scale = 1.0f;
                }
            }
        }
    }

}
