#include <sandbox/bootstrap.hpp>
#include <sandbox/render_settings.hpp>

#include <benzin/core/logger.hpp>
#include <shaders/joint/procedural_grass_resources.hpp>

namespace sandbox
{

    template <>
    void DrawSettings(GBufferSettings& settings)
    {
        ImGui::Checkbox("Indirect draw", &settings.m_IsIndirectDrawEnabled);
        ImGui::Checkbox("Mesh pipeline", &settings.m_IsMeshPipelineUsed);
    }

    template <>
    void DrawSettings(ProceduralGrassSettings& settings)
    {
        ImGui::PushItemWidth(150.0f);
        BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

        ImGui::Checkbox("Enable ### ProceduralGrass", &settings.IsEnabled);

        ImGui::BeginDisabled(!settings.IsEnabled);
        BenzinExecuteOnScopeExit([] { ImGui::EndDisabled(); });

        ImGui::NewLine();
        ImGui::SeparatorText("AMPLIFICATION");
        {
            ImGui::Checkbox("GPU frustum culling", &settings.IsFrustumCullingEnabled);
            ImGui::DragFloat("Patch cull radius", &settings.GrassPatchCullRadius, 0.0001f);
        }

        ImGui::NewLine();
        ImGui::SeparatorText("MESH");
        {
            ImGui::DragFloat("Grass end distance", &settings.GrassEndDistance, 0.01f);

            if (ImGui::DragFloat("Spacing in patch (between blades)", &settings.SpacingInGrassPatch, 0.0001f))
            {
                settings.SpacingInGrassPatch = std::max(settings.SpacingInGrassPatch, 0.001f);
            }

            ImGui::DragFloat("Wind direction", &settings.WindDirection, 0.01f, 0.0f, DirectX::XM_2PI);
            ImGui::DragFloat("Blade width", &settings.BladeWidth, 0.0001f, std::numeric_limits<float>::min());
        }

        ImGui::NewLine();
        ImGui::SeparatorText("PIXEL");
        {
            ImGui::ColorEdit3("Base color", (float*)&settings.BaseColor);
        }
    }

    template <>
    void DrawSettings(ProceduralGrassStats& stats)
    {
        std::locale::global(benzin::Logger::GetThoudandSeperatorApostrophe3());
        BenzinExecuteOnScopeExit([] { std::locale::global(std::locale::classic()); });

        ImGui::FmtText("Patch count: {:L} (Max: {:L})", stats.PatchCount, stats.MaxPatchCount);
        ImGui::FmtText("Blade count: {:L} (Max: {:L})", stats.BladeCount, stats.MaxPatchCount * std::to_underlying(joint::ProceduralGrassConsts::MaxBladeCountPerPatch));
        ImGui::FmtText("Vertex count: {:L}", stats.VertexCount);
        ImGui::FmtText("Triangle count: {:L}", stats.TriangleCount);
    }

    template <>
    void DrawSettings(RayTracing_ShadowSettings& settings)
    {
        ImGui::PushItemWidth(150.0f);
        BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

        ImGui::Checkbox("Enable###RayTracingShadows", &settings.m_IsEnabled);

        ImGui::Checkbox("Use blue noise", &settings.m_IsBlueNoiseUsed);
        ImGui::Checkbox("Animate noise", &settings.m_IsNoiseAnimated);

        ImGui::Checkbox("Freeze blue noise depth", &settings.m_IsBlueNoiseDepthFreezed);

        ImGui::BeginDisabled();
        auto tempBlueNoiseDepthIndex = (int)settings.m_BlueNoiseDepthIndex;
        ImGui::SliderInt("Blue noise depth index", &tempBlueNoiseDepthIndex, 0, settings.m_BlueNoiseDepth - 1);
        ImGui::EndDisabled();
    }

    template <>
    void DrawSettings(SigmaDenoiserSettings& settings)
    {
        ImGui::Checkbox("Enable ### SigmaDenoiser", &settings.m_IsEnabled);

        ImGui::BeginDisabled(!settings.m_IsEnabled);
        BenzinExecuteOnScopeExit([] { ImGui::EndDisabled(); });

        ImGui::Checkbox("Clear pass", &settings.m_IsClearEnabled);

        ImGui::NewLine();
        ImGui::SeparatorText("CLASSIFICATION");
        {
            ImGui::Checkbox("Tile smoothing", &settings.m_IsTileSmoothingEnabled);
        }

        ImGui::NewLine();
        ImGui::SeparatorText("BLUR");
        {
            ImGui::Checkbox("Post blur pass", &settings.m_IsPostBlurEnabled);
            ImGui::DragFloat("Plane distance sensitivity %", &settings.m_PlaneDistanceSensitivity, 0.0001f, 0.0f, 0.1f);
        }

        ImGui::NewLine();
        ImGui::SeparatorText("TEMPORAL STABILIZATION");
        {
            ImGui::Checkbox("Temporal stabilization pass", &settings.m_IsTemporalStabilizationEnabled);
            ImGui::DragFloat("Disocclusion threshold %", &settings.m_DisocclusionThreshold, 0.0001f, 0.0f, 0.2f);

            ImGui::BeginDisabled(true);
            ImGui::SliderInt("History length", (int*)&settings.m_HistoryLength, 0, settings.ms_MaxHistoryLength, "%d", ImGuiSliderFlags_NoInput);
            ImGui::DragFloat("Stabilization strength", &settings.m_StabilizationStrength);
            ImGui::EndDisabled();
        }
    }

    template <>
    void DrawSettings(ToneMappingSettings& settings)
    {
        ImGui::PushItemWidth(120.0f);
        BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

        ImGui::Checkbox("Enable ### ToneMapping", &settings.IsToneMappingEnabled);

        ImGui::CollapsingHeaderWithIndent("Luminance Histogram", [&settings]
        {
            auto& luminanceHistogram = settings.LuminanceHistogram;

            if (ImGui::DragFloat("Min log luminance", &luminanceHistogram.MinLogLuminance))
            {
                luminanceHistogram.MinLogLuminance = std::clamp(
                    luminanceHistogram.MinLogLuminance,
                    luminanceHistogram.MinLogLuminance,
                    luminanceHistogram.MaxLogLuminance);
            }

            if (ImGui::DragFloat("Max log luminance", &luminanceHistogram.MaxLogLuminance))
            {
                luminanceHistogram.MaxLogLuminance = std::clamp(
                    luminanceHistogram.MaxLogLuminance,
                    luminanceHistogram.MinLogLuminance,
                    luminanceHistogram.MaxLogLuminance);
            }

            ImGui::InputFloat("Tau", &luminanceHistogram.Tau);
        });

        ImGui::CollapsingHeaderWithIndent("PBR Camera", [&settings]
        {
            auto& pbrCamera = settings.PbrCamera;

            ImGui::Checkbox("Auto exposure", &settings.IsAutoExposureUsed);

            ImGui::DragFloat("Aperture (in f-stops)", &pbrCamera.Aperture, 0.001f);
            ImGui::DragFloat("Shutter speed (in sec)", &pbrCamera.ShutterSpeed, 0.001f);
            ImGui::DragFloat("Sensor sensitivity (in ISO)", &pbrCamera.Iso, 0.01f);
        });

        ImGui::CollapsingHeaderWithIndent("Tone Mapping", [&settings]
        {
            ImGui::Checkbox("Accurate gamma correction", &settings.IsAccurateGammaCorrectionUsed);

            static const auto toneReproductionTransformNames = magic_enum::enum_names<joint::ToneReproductionTransform>();

            ImGui::Combo(
                "Tone reproduction transform",
                (int*)&settings.ToneReproductionTransform,
                ImGui::SelectComboName<decltype(toneReproductionTransformNames)>,
                (void*)&toneReproductionTransformNames,
                (int)toneReproductionTransformNames.size());
        });
    }

}
