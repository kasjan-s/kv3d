#version 450

layout(push_constant) uniform SceneObjectPushConsts {
    vec3 light_ambient;
    vec3 light_diffuse;
    vec3 light_specular;
    vec3 light_pos;
    vec3 camera_pos;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
    uint is_textured;
} pushConstants;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {

    bool is_textured = bool(pushConstants.is_textured > 0);

    vec3 ambient = pushConstants.light_ambient;
    if (is_textured) {
        ambient *= texture(texSampler, fragTexCoord).rgb;
    } else {
        ambient *= pushConstants.ambient;
    }

    // diffuse
    vec3 norm = normalize(inNormal);
    vec3 lightDir = normalize(pushConstants.light_pos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * pushConstants.light_diffuse;
    if (is_textured) {
        diffuse *= texture(texSampler, fragTexCoord).rgb;
    } else { 
        diffuse *= pushConstants.diffuse;
    }

    // specular
    vec3 view_dir = normalize(pushConstants.camera_pos - fragPos);
    vec3 reflect_dir = reflect(-lightDir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), pushConstants.shininess);
    vec3 specular = spec * pushConstants.light_specular;
    if (is_textured) {
        specular *= texture(texSampler, fragTexCoord).rgb;
    } else { 
        specular *= pushConstants.specular;
    }

    vec3 result = ambient + diffuse + specular;
    
    outColor = vec4(result, 1.0f);
}