load("@rules_vulkan//glsl:defs.bzl", "glsl_shader")

glsl_shader(
    name = "vert_shader",
    shader = "shader.vert",
    visibility = ["//visibility:public"]
)

glsl_shader(
    name = "frag_shader",
    shader = "shader.frag",
    visibility = ["//visibility:public"]
)

filegroup(
  name = "data",
  srcs = glob(["shader.*"]),
  visibility = ["//visibility:public"],
)