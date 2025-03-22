#version 450

layout(push_constant) uniform SceneObjectPushConsts {
    uint is_textured;
    vec4 color;
} pushConstants;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec4 outColor;

void main() {
    bool is_textured = bool(pushConstants.is_textured > 0);
    if (is_textured) {
        outColor = texture(texSampler, fragTexCoord);
        // outColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);
    } else {
        outColor = pushConstants.color;
    }
}