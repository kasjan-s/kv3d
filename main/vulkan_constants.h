#pragma once

#include "vulkan/vulkan.h"

#include <array>
#include <iostream>

#define VK_CHECK_RESULT(function_call) \
{   \
    VkResult res = (function_call); \
    if (res != VK_SUCCESS) {    \
        std::cerr << "Fatal: VkResult is " << std::to_string(res) << " in " << __FILE__ << " at line " << __LINE__ << std::endl;  \
    }   \
} 

#ifdef NDEBUG
    constexpr inline bool kEnableValidationLayers = false;
#else
    constexpr inline bool kEnableValidationLayers = true;
#endif

constexpr std::array<const char*, 1> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

constexpr const char* kValidationLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};

constexpr inline int kMaxFramesInFlight = 2;