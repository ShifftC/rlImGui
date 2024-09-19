-- Copyright 2024 ShifftC. Licensed under the Apache License, Version 2.0.

add_requires("imgui docking", "okraylib master")

set_project("rlImGui")
set_version("0.0.1")
set_license("Zlib")
set_languages("c++23")

target("rlImGui", function()
    set_default(false)
    set_kind("static")

    add_packages("imgui", "okraylib", {public = true})

    add_files("rlImGui.cpp")
    add_headerfiles("rlImGui.h", "rlImGuiColors.h", "imgui_impl_raylib.h", "extras/*.h")
    add_includedirs(".", {public = true})
    add_extrafiles(".gitignore", "LICENSE", "README.md", "examples/**", "resources/**")
end)
