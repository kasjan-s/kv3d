cc_binary(
    name = "kv3d",
    srcs = ["kv3d.cc"],
    deps = [
        ":model",
        ":scene",
        ":vertex",
        ":vulkan_device",
        ":vulkan_swapchain",
        ":vulkan_texture",
        "@glfw//:glfw",
        "@rules_vulkan//vulkan:vulkan_cc_library",
        "@bazel_tools//tools/cpp/runfiles"        
    ],
    data = [
        "//main/shaders:frag_shader",
        "//main/shaders:vert_shader",
        "//main/shaders:data",
        "//main/textures:textures",
        "//main/models:models"
    ],
)

cc_library(
    name = "vertex",
    srcs = ["vertex.cc"],
    hdrs = ["vertex.h"],
    deps = [
        "@glm//:glm",
        "@rules_vulkan//vulkan:vulkan_cc_library",
    ]
)

cc_library(
    name = "vulkan_device",
    srcs = ["vulkan_device.cc"],
    hdrs = ["vulkan_device.h"],
    deps = [
        ":vulkan_buffer",
        ":vulkan_constants",
        "@glfw//:glfw",
        "@rules_vulkan//vulkan:vulkan_cc_library",
    ]
)

cc_library(
    name = "vulkan_swapchain",
    srcs = ["vulkan_swapchain.cc"],
    hdrs = ["vulkan_swapchain.h"],
    deps = [
        ":vulkan_constants",
        ":vulkan_device",
        "@glfw//:glfw",
        "@rules_vulkan//vulkan:vulkan_cc_library",
    ]
)

cc_library(
    name = "vulkan_constants",
    hdrs = ["vulkan_constants.h"]
)

cc_library(
    name = "vulkan_buffer",
    hdrs = ["vulkan_buffer.h"]
)

cc_library(
    name = "vulkan_texture",
    srcs = ["texture.cc"],
    hdrs = ["texture.h"],
    deps = [
        ":vulkan_buffer",
        ":vulkan_constants",
        ":vulkan_device",
        "//third_party:stb_image",
        "@rules_vulkan//vulkan:vulkan_cc_library",
    ]
)

cc_library(
    name = "model",
    srcs = ["model.cc"],
    hdrs = ["model.h"],
    deps = [
        ":vertex",
        ":vulkan_device",
        "//third_party:tiny_obj_loader",
        "@glfw//:glfw",
        "@rules_vulkan//vulkan:vulkan_cc_library",
    ]
)

cc_library(
    name = "scene_object",
    srcs = ["scene_object.cc"],
    hdrs = ["scene_object.h"],
    deps = [
        ":camera",
        ":material",
        ":model",
        ":vulkan_texture",
        ":vulkan_device",
    ]
)

cc_library(
    name = "scene",
    srcs = ["scene.cc"],
    hdrs = ["scene.h"],
    deps = [
        ":camera",
        ":scene_object"
    ]
)

cc_library(
    name = "camera",
    srcs = ["camera.cc"],
    hdrs = ["camera.h"],
    deps = [
        "@glm//:glm",
    ]
)

cc_library(
    name = "material",
    hdrs = ["material.h"],
    srcs = ["material.cc"],
    deps = [
        "@glm//:glm",
    ]
)
