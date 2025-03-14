load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "rules_vulkan",
    remote = "https://github.com/jadarve/rules_vulkan.git",
    tag = "v0.0.6"
)

load("@rules_vulkan//vulkan:repositories.bzl", "vulkan_repositories")

vulkan_repositories()

# ===== glfw =====

git_repository(
    name = "glfw",
    remote = "https://github.com/glfw/glfw.git",
    commit = "e7ea71be039836da3a98cea55ae5569cb5eb885c",
    build_file = "@//:third_party/glfw.BUILD",
)

# ===== glm =====

git_repository(
    name = "glm",
    remote = "https://github.com/g-truc/glm.git",
    commit = "2d4c4b4dd31fde06cfffad7915c2b3006402322f",
    build_file = "@//:third_party/glm.BUILD",
)

# load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

git_repository(
    name = "fmeum_rules_runfiles",
    remote = "https://github.com/fmeum/rules_runfiles.git",
    commit = "aae9b6877a90922cba96bdc410e02f48117edd8c",
)