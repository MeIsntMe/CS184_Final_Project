#version 330 core

in vec3 vWorldPos;
in vec3 vCamWorldPos;
in vec3 vNormal;

uniform vec3 lightDir;
uniform vec3 glassColor;

out vec4 FragColor;

void main() {
    vec3 viewDir = normalize(vCamWorldPos - vWorldPos);
    vec3 normal  = normalize(vNormal);

    // Flip normal if we're looking at the inside of a face
    if (dot(normal, viewDir) < 0.0) normal = -normal;

    float NdotV = max(dot(normal, viewDir), 0.0);

    // Fresnel: edges more opaque/reflective, center nearly invisible
    float fresnel = pow(1.0 - NdotV, 3.0);

    // Blinn-Phong specular from directional light
    vec3 ldir    = normalize(lightDir);
    vec3 halfVec = normalize(viewDir + ldir);
    float spec   = pow(max(dot(normal, halfVec), 0.0), 64.0);

    float alpha = 0.15 + fresnel * 0.45;
    vec3  color = mix(glassColor, vec3(1.0), fresnel * 0.4);
    color      += vec3(spec * 0.4);
    color       = clamp(color, 0.0, 1.0);

    FragColor = vec4(color, alpha);
}
