#pragma once

using ImGui_DrawCallback = std::function<void()>;

void ImGui_CollapsingHeaderWithIndent(std::string_view name, const ImGui_DrawCallback& callback, ImGuiTreeNodeFlags additionalFlags = ImGuiTreeNodeFlags_None);

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
