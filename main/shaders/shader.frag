#version 450

layout(push_constant) uniform SceneObjectPushConsts {
    vec3 light_color;
    vec3 light_pos;
    vec3 camera_pos;
    uint is_textured;
} pushConstants;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(set = 0, binding = 2) uniform UniformBufferObject {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
} material;


layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    float ambientStrength = 0.1f;
    vec3 ambient = ambientStrength * pushConstants.light_color;

    bool is_textured = bool(pushConstants.is_textured > 0);
    vec3 color = inColor;
    if (is_textured) {
        color = texture(texSampler, fragTexCoord).rgb;
    } 

    vec3 norm = normalize(inNormal);

    vec3 lightDir = normalize(pushConstants.light_pos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * pushConstants.light_color;

    float specular_strength = 0.5;
    vec3 view_dir = normalize(pushConstants.camera_pos - fragPos);
    vec3 reflect_dir = reflect(-lightDir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
    vec3 specular = specular_strength * spec * pushConstants.light_color;

    vec3 result = (ambient + diffuse + specular) * color;
    outColor = vec4(result, 1.0f);
}