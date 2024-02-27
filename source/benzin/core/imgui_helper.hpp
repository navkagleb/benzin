#pragma once

namespace benzin
{

    template <typename... Args>
    void ImGui_Text(std::format_string<Args...> fmt, Args&&... args)
    {
        ImGui::Text(std::format(fmt, std::forward<Args>(args)...).c_str());
    }

    template <typename... Args>
    void ImGui_BulletText(std::format_string<Args...> fmt, Args&&... args)
    {
        ImGui::BulletText(std::format(fmt, std::forward<Args>(args)...).c_str());
    }

} // namespace benzin
