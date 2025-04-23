#pragma once

using ImGui_DrawCallback = std::function<void()>;

bool Imgui_MainCollapsingHeader(std::string_view name, ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None);
void ImGui_CollapsingHeaderWithIndent(std::string_view name, const ImGui_DrawCallback& callback, ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None);

void ImGui_WarningBox(std::string_view text, std::string_view id);

template <typename T>
static bool ImGui_SelectComboName(void* data, int index, const char** outName)
{
    const auto& names = *(T*)data;

    if (index < 0 || index >= names.size())
    {
        return false;
    }

    *outName = names[index].data();
    return true;
};
