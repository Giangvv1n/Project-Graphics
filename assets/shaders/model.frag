#version 330 core

in vec3 vFragPos;
in vec3 vNormal;

uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform vec3 uObjectColor;
uniform vec3 uLightColor;

out vec4 FragColor;

void main() {
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(-uLightDir);

    float ambientStrength = 0.25;
    vec3 ambient = ambientStrength * uLightColor;

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor;

    float specularStrength = 0.35;
    vec3 viewDir = normalize(uViewPos - vFragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * uLightColor;

    vec3 result = (ambient + diffuse + specular) * uObjectColor;
    FragColor = vec4(result, 1.0);
}