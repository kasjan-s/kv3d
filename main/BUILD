cc_binary(
    name = "kv3d",
    srcs = ["kv3d.cc"],
    deps = [
        "//third_party:stb_image",
        "//third_party:tiny_obj_loader",
        "@glfw//:glfw",
        "@glm//:glm",
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
