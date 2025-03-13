#pragma once

using ImGui_DrawCallback = std::function<void()>;

void ImGui_CollapsingHeaderWithIndent(std::string_view name, const ImGui_DrawCallback& callback, ImGuiTreeNodeFlags additionalFlags = ImGuiTreeNodeFlags_None);
