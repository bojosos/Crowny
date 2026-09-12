#pragma once

#include "cwpch.h"

#include <Crowny.h>

// ImGui headers are included by most editor translation units; precompiling
// them removes the largest per-file parse cost.
// ImGuizmo requires ImGui's types to be declared first.
// clang-format off
#include <imgui.h>
#include <ImGuizmo.h>
// clang-format on
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#ifdef CW_WITH_NODES
#include <ImNodeFlow.h>
#include <imgui_node_editor.h>
#endif
