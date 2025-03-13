#pragma once

#include <imgui.h>

struct ImGuiSharedGlobals
{
    ImGuiContext* Context;
    ImGuiMemAllocFunc MemAllocFunc;
    ImGuiMemFreeFunc MemFreeFunc;
    void* UserData;
};