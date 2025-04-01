#include "sandbox/bootstrap.hpp"
#include "sandbox/sandbox_runner.hpp"

#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/tools/render_settings_tool.hpp>
#include <benzin/tools/render_viewport_tool.hpp>
#include <benzin/tools/texture_viewer_tool.hpp>

#include <shaders/joint/mesh_types.hpp>

#include "sandbox/render_passes/deferred_lighting_pass.hpp"
#include "sandbox/render_passes/environment_pass.hpp"
#include "sandbox/render_passes/geometry_pass.hpp"
#include "sandbox/render_passes/global_constants_pass.hpp"
#include "sandbox/render_passes/ray_tracing_shadow_pass.hpp"
#include "sandbox/render_passes/sigma_denoiser_pass.hpp"
#include "sandbox/render_passes/tlas_building_pass.hpp"
#include "sandbox/render_passes/tone_mapping_pass.hpp"
#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

namespace sandbox
{

    enum class Mesh : uint32_t
    {
        Sponza,
        BoomBox,
        DamagedHelmet,
        OrientationTest,
        MilkTruck,
        GltfMeshCount,

        Cylinder = GltfMeshCount,
        UnitSphere,
    };
    BenzinEnableUnaryPlusForEnum(Mesh);

    static void LoadMeshes(std::span<benzin::MeshResource> outMeshResources)
    {
        BenzinLogTimeOnScopeExit("LoadMeshes");

        const auto createCylinderMesh = []
        {
            const joint::Material material
            {
                .AlbedoFactor{ 0.7f, 0.7f, 0.7f, 1.0f },
            };

            const joint::MeshInstance meshInstance
            {
                .SubMeshIndex = 0,
                .MaterialIndex = 0,
            };

            return benzin::MeshResource
            {
                .DebugName = "Cylinder",
                .SubMeshes{ benzin::GetDefaultCyliderMesh() },
                .SubMeshInstances{ meshInstance },
                .Materials{ material },
            };
        };

        const auto createUnitSphereMesh = []
        {
            const joint::Material material
            {
                .AlbedoFactor{ 0.0f, 0.0f, 0.0f, 0.0f },
                .EmissiveFactor{ 1.0f, 1.0f, 1.0f },
            };

            const joint::MeshInstance meshInstance
            {
                .SubMeshIndex = 0,
                .MaterialIndex = 0,
            };

            return benzin::MeshResource
            {
                .DebugName = "UnitSphere",
                .SubMeshes{ benzin::GetUnitGeoSphereMesh() },
                .SubMeshInstances{ meshInstance },
                .Materials{ material },
            };
        };

        const auto loadFromFile = [](std::string_view fileName)
        {
            benzin::MeshResource resource;
            BenzinAssertExpr(benzin::LoadMeshFromGltfFile(fileName, resource));

            return resource;
        };

        std::array<std::string_view, +Mesh::GltfMeshCount> gltfFileNames;
        gltfFileNames[+Mesh::Sponza] = "Sponza/glTF/Sponza.gltf";
        gltfFileNames[+Mesh::BoomBox] = "BoomBox/glTF/BoomBox.gltf";
        gltfFileNames[+Mesh::DamagedHelmet] = "DamagedHelmet/glTF/DamagedHelmet.gltf";
        gltfFileNames[+Mesh::OrientationTest] = "OrientationTest/OrientationTest.gltf";
        gltfFileNames[+Mesh::MilkTruck] = "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf";

        std::array<std::future<void>, +Mesh::GltfMeshCount> gltfFutures;
        for (const uint32_t i : std::views::iota(0u, +Mesh::GltfMeshCount))
        {
            gltfFutures[i] = std::async(std::launch::async, [&, i]
            {
                outMeshResources[i] = loadFromFile(gltfFileNames[i]);
            });
        }

        outMeshResources[+Mesh::Cylinder] = createCylinderMesh();
        outMeshResources[+Mesh::UnitSphere] = createUnitSphereMesh();

        for (auto& future : gltfFutures)
        {
            future.wait();
        }
    }

    static void DrawGBufferSettings(GBufferSettings& settings)
    {
        ImGui::Checkbox("IsFrustumCullingEnabled", &settings.IsFrustumCullingEnabled);

        ImGui_CollapsingHeaderWithIndent("Stats", [&settings]
        {
            // TODO: Move to global space
            struct ThoudandSeperatorApostrophe3 : std::numpunct<char>
            {
                char do_thousands_sep() const override { return '\''; }

                std::string do_grouping() const override { return "\3"; }
            };

            static const std::locale customLocale{ std::locale::classic(), new ThoudandSeperatorApostrophe3 };

            std::locale::global(customLocale);
            BenzinExecuteOnScopeExit([] { std::locale::global(std::locale::classic()); });

            const auto& stats = settings.Stats;

            ImGui::Text(BenzinFormatData("MeshCount: {:L}", stats.MeshCount));
            ImGui::Text(BenzinFormatData("RenderedMeshCount: {:L}", stats.RenderedMeshCount));
            ImGui::Text(BenzinFormatData("RenderedTriangleCount: {:L}", stats.RenderedTriangleCount));
        });
    }

    static void DrawRayTracingShadowsSettings(RayTracing_ShadowSettings& settings)
    {
        ImGui::Checkbox("IsEnabled###RayTracingShadows", &settings.IsEnabled);
        ImGui::Checkbox("IsBlueNoiseUsed", &settings.IsBlueNoiseUsed);
        ImGui::Checkbox("IsNoiseAnimated", &settings.IsNoiseAnimated);
    }

    static void DrawSigmaDenoiserSettings(SigmaDenoiserSettings& settings)
    {
        ImGui::Checkbox("IsEnabled###SigmaDenoiser", &settings.IsEnabled);
        ImGui::DragFloat("PlaneDistanceSensitivity %", &settings.PlaneDistanceSensitivity, 0.0001f, 0.0f, 0.1f);
        ImGui::DragFloat("DisocclusionThreshold %", &settings.DisocclusionThreshold, 0.0001f, 0.0f, 0.2f);

        ImGui::BeginDisabled(true);
        ImGui::SliderInt("HistoryLength", (int*)&settings.HistoryLength, 0, settings.MaxHistoryLength, "%d", ImGuiSliderFlags_NoInput);
        ImGui::EndDisabled();
        ImGui::Text(BenzinFormatData("StabilizationStrength: {:.3f}", settings.StabilizationStrength));

        ImGui::Separator();
        ImGui::Checkbox("IsClearEnabled", &settings.IsClearEnabled);
        ImGui::Checkbox("IsTileSmoothingeEnabled", &settings.IsTileSmoothingEnabled);
        ImGui::Checkbox("IsPostBlurEnabled", &settings.IsPostBlurEnabled);
        ImGui::Checkbox("IsTemporalStabilizationEnabled", &settings.IsTemporalStabilizationEnabled);
    }

    static void DrawToneMappingSettings(ToneMappingSettings& settings)
    {
        ImGui::PushItemWidth(120.0f);
        BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

        ImGui::Checkbox("IsToneMappingEnabled", &settings.IsToneMappingEnabled);

        ImGui_CollapsingHeaderWithIndent("Luminance Histogram", [&settings]
        {
            auto& luminanceHistogram = settings.LuminanceHistogram;

            if (ImGui::InputFloat("MinLogLuminance", &luminanceHistogram.MinLogLuminance))
            {
                luminanceHistogram.MinLogLuminance = std::clamp(
                    luminanceHistogram.MinLogLuminance,
                    luminanceHistogram.MinLogLuminance,
                    luminanceHistogram.MaxLogLuminance
                );
            }

            if (ImGui::InputFloat("MaxLogLuminance", &luminanceHistogram.MaxLogLuminance))
            {
                luminanceHistogram.MaxLogLuminance = std::clamp(
                    luminanceHistogram.MaxLogLuminance,
                    luminanceHistogram.MinLogLuminance,
                    luminanceHistogram.MaxLogLuminance
                );
            }

            ImGui::InputFloat("Tau", &luminanceHistogram.Tau);
        });

        ImGui_CollapsingHeaderWithIndent("PBR Camera", [&settings]
        {
            auto& pbrCamera = settings.PbrCamera;

            ImGui::Checkbox("IsAutoExposureUsed", &settings.IsAutoExposureUsed);

            ImGui::InputFloat("Aperture (in f-stops)", &pbrCamera.Aperture);
            ImGui::InputFloat("Shutter Speed (in sec)", &pbrCamera.ShutterSpeed);
            ImGui::InputFloat("Sensor sensitivity (in ISO)", &pbrCamera.Iso);
        });

        ImGui_CollapsingHeaderWithIndent("Tone Mapping", [&settings]
        {
            ImGui::Checkbox("IsAccurateGammaCorrectionUsed", &settings.IsAccurateGammaCorrectionUsed);

            static const auto toneReproductionTransformNames = magic_enum::enum_names<joint::ToneReproductionTransform>();

            ImGui::Combo(
                "ToneReproductionTransform",
                (int*)&settings.ToneReproductionTransform,
                ImGui_SelectComboName<decltype(toneReproductionTransformNames)>,
                (void*)&toneReproductionTransformNames,
                (int)toneReproductionTransformNames.size()
            );
        });
    }

    //

    SandboxRunner::SandboxRunner()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::SandboxRunner");

        InitRenderPasses();
        InitTools();
        
        InitCamera();
        InitSceneEntities();
    }

    void SandboxRunner::InitRenderPasses()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitRenderPasses");

        // The order in which render passes are added is important
        BenzinAssert(m_RenderPasses.empty());
        m_RenderPasses.push_back(std::make_unique<TlasBuildingPass>(*m_Device, *m_RayTracingScene));
        m_RenderPasses.push_back(std::make_unique<GlobalConstantsPass>(*m_Device, *m_Scene));
        m_RenderPasses.push_back(std::make_unique<GeometryPass>(*m_Scene));
        m_RenderPasses.push_back(std::make_unique<RayTracing_ShadowPass>(*m_Scene));
        m_RenderPasses.push_back(std::make_unique<SigmaDenoiserPass>(*m_Scene));
        m_RenderPasses.push_back(std::make_unique<DeferredLightingPass>());
        m_RenderPasses.push_back(std::make_unique<EnvironmentPass>());
        m_RenderPasses.push_back(std::make_unique<ToneMappingPass>());
    }

    void SandboxRunner::InitTools()
    {
        BenzinAssert(m_RenderSettingsTool != nullptr);
        m_RenderSettingsTool->RegisterSectionDrawCallback<GBufferSettings>(DrawGBufferSettings, ImGuiTreeNodeFlags_DefaultOpen);
        m_RenderSettingsTool->RegisterSectionDrawCallback<RayTracing_ShadowSettings>(DrawRayTracingShadowsSettings, ImGuiTreeNodeFlags_DefaultOpen);
        m_RenderSettingsTool->RegisterSectionDrawCallback<SigmaDenoiserSettings>(DrawSigmaDenoiserSettings, ImGuiTreeNodeFlags_DefaultOpen);
        m_RenderSettingsTool->RegisterSectionDrawCallback<ToneMappingSettings>(DrawToneMappingSettings, ImGuiTreeNodeFlags_DefaultOpen);
    }

    void SandboxRunner::InitCamera()
    {
        auto& perspectiveProjection = m_Scene->GetPerspectiveProjection();
        perspectiveProjection.SetLens(DirectX::XMConvertToRadians(90.0f), 16.0f / 9.0f, 0.1f, 1000.0f);

        auto& camera = m_Scene->GetCamera();
        camera.SetPosition({ -1.649f, 1.007f, -1.555f });
        camera.SetFrontDirection({ 0.769f, 0.129f, 0.627f });
    }

    void SandboxRunner::InitSceneEntities()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitSceneEntities");

        std::array<benzin::MeshResource, magic_enum::enum_count<Mesh>()> meshResources{};
        LoadMeshes(meshResources);

        std::array<entt::entity, magic_enum::enum_count<Mesh>()> meshHandles;
        meshHandles.fill(benzin::g_InvalidEnum<entt::entity>);
        AddMeshesToScene(meshResources, meshHandles);

        AddStaticMeshEntities(meshHandles);
        AddDynamicMeshEntities(meshHandles);
        AddLightEntities(meshHandles);
    }

    void SandboxRunner::AddMeshesToScene(std::span<benzin::MeshResource> meshResources, std::span<entt::entity> outMeshHandles)
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::AddMeshesToScene");

        for (auto&& [outMeshHandle, meshResource] : std::views::zip(outMeshHandles, meshResources))
        {
            outMeshHandle = m_Scene->AddMesh(std::move(meshResource));
        }
    }

    void SandboxRunner::AddStaticMeshEntities(std::span<const entt::entity> meshHandles)
    {
        auto& entityRegistry = m_Scene->GetEntityRegistry();

        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::Sponza];

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetRotation({ 0.0f, DirectX::XM_PI, 0.0f });
            transform.SetTranslation({ 5.0f, 0.0f, 0.0f });
        }

        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::OrientationTest];

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetScale({ 0.05f, 0.05f, 0.05f });
            transform.SetTranslation({ 2.5f, 0.2f, -0.25f });
        }

        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::MilkTruck];

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetScale({ 0.1f, 0.1f, 0.1f });
            transform.SetTranslation({ -1.5f, 0.2f, 0.5f });
        }

        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::Cylinder];

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetScale({ 0.1f, 1.5f, 0.1f });
            transform.SetTranslation({ -1.5f, 0.4f, -0.25f });
        }
    }

    void SandboxRunner::AddDynamicMeshEntities(std::span<const entt::entity> meshHandles)
    {
        auto& entityRegistry = m_Scene->GetEntityRegistry();

        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::BoomBox];

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetRotation({ 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f });
            transform.SetScale({ 30.0f, 30.0f, 30.0f });
            transform.SetTranslation({ 0.0f, 0.6f, 0.0f });

            entityRegistry.emplace<benzin::EntityUpdateCallback>(entity, [this, &entityRegistry, entity]
            {
                auto& transform = entityRegistry.get<benzin::Transform>(entity);

                auto rotation = transform.GetRotation();
                rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                rotation.z += 0.0002f * m_AnimationTimer.GetDeltaTimeInMs();

                transform.SetRotation(rotation);
            });
        }

        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::DamagedHelmet];

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetRotation({ 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f });
            transform.SetScale({ 0.4f, 0.4f, 0.4f });
            transform.SetTranslation({ 1.0f, 0.5f, -0.5f });

            entityRegistry.emplace<benzin::EntityUpdateCallback>(entity, [this, &entityRegistry, entity]
            {
                auto& transform = entityRegistry.get<benzin::Transform>(entity);

                auto rotation = transform.GetRotation();
                rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                rotation.y -= 0.00015f * m_AnimationTimer.GetDeltaTimeInMs();

                transform.SetRotation(rotation);
            });
        }
    }

    void SandboxRunner::AddLightEntities(std::span<const entt::entity> meshHandles)
    {
        entt::registry& entityRegistry = m_Scene->GetEntityRegistry();

        {
            const auto entity = m_Scene->GetSunEntity();

            auto& light = entityRegistry.get_or_emplace<benzin::SunLight>(entity);
            light.SetColor({ 1.0f, 1.0f, 0.7f });
            light.SetIntensity(1'000.0f);

            entityRegistry.emplace<benzin::EntityUpdateCallback>(entity, [this, &entityRegistry, entity]
            {
                const auto animateSunAngle = [this](
                    float minAngleInRadians,
                    float maxAngleInRadians,
                    float& outAngleInRadians,
                    float& outDirection
                )
                {
                    constexpr float speed = 0.01f;

                    outAngleInRadians += outDirection * m_AnimationTimer.GetDeltaTimeInSec() * speed;

                    if (!(minAngleInRadians <= outAngleInRadians && outAngleInRadians <= maxAngleInRadians))
                    {
                        outDirection *= -1.0f;
                    }
                };

                static float elevationDirection = 1.0f;
                static float azimithDirection = 1.0f;

                auto& light = entityRegistry.get<benzin::SunLight>(entity);

                float elevation = light.GetElevationInRadians();;
                animateSunAngle(DirectX::XMConvertToRadians(41.0f), DirectX::XMConvertToRadians(52.0f), elevation, elevationDirection);
                light.SetElevationInRadians(elevation);

                float azimuth = light.GetAzimuthInRadians();
                animateSunAngle(DirectX::XMConvertToRadians(-2.0f), DirectX::XMConvertToRadians(3.0f), azimuth, azimithDirection);
                light.SetAzimuthInRadians(azimuth);
            });
        }

        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::UnitSphere];

            auto& light = entityRegistry.emplace<benzin::SphericalLight>(entity);
            light.SetIntensity(5.0f);
            light.SetPosition({ 0.5f, 2.0f, -0.25f });
            light.SetRadius(0.03f);
            light.SetRange(10.0f);
            light.SetEnabled(false);

            entityRegistry.emplace<benzin::EntityUpdateCallback>(entity, [this, &entityRegistry, entity]
            {
                constexpr float travelRadius = 1.0f;
                constexpr float travelSpeed = 0.5f;

                auto& light = entityRegistry.get<benzin::SphericalLight>(entity);

                static const float startX = light.GetPosition().x;
                static const float startZ = light.GetPosition().z;

                auto position = light.GetPosition();
                position.x = startX + travelRadius * std::cos(travelSpeed * m_AnimationTimer.GetElapsedTimeInSec());
                position.z = startZ + travelRadius * std::sin(travelSpeed * m_AnimationTimer.GetElapsedTimeInSec());

                light.SetPosition(position);
            });
        }

        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::UnitSphere];

            auto& light = entityRegistry.emplace<benzin::SphericalLight>(entity);
            light.SetColor({ 0.7f, 0.8f, 0.3f });
            light.SetIntensity(10.0f);
            light.SetPosition({ 0.0f, 3.0f, 1.25f });
            light.SetRadius(0.01f);
            light.SetRange(30.0f);
            light.SetEnabled(false);
        }

        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::UnitSphere];

            auto& light = entityRegistry.emplace<benzin::SphericalLight>(entity);
            light.SetColor({ 0.9f, 0.7f, 0.8f });
            light.SetIntensity(15.0f);
            light.SetPosition({ 1.5f, 4.0f, -1.0f });
            light.SetRadius(0.02f);
            light.SetRange(20.0f);
            light.SetEnabled(false);

            entityRegistry.emplace<benzin::EntityUpdateCallback>(entity, [this, &entityRegistry, entity]
            {
                constexpr float speed = 1.5f;
                constexpr float min = -1.0f;
                constexpr float max = 12.0f;

                static float direction = 1.0f;

                auto& light = entityRegistry.get<benzin::SphericalLight>(entity);

                auto position = light.GetPosition();
                position.x += direction * speed * m_AnimationTimer.GetDeltaTimeInSec();

                if (position.x < min || position.x > max)
                {
                    position.x = std::clamp(position.x, min, max);
                    direction *= -1.0f;
                }

                light.SetPosition(position);
            });
        }
    }

}
