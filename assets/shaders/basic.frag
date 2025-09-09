#version 330 core

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform vec3 u_CameraPosition;
uniform float u_Time;

out vec4 FragColor;

void main() {
    // Simple lighting calculation
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 normal = normalize(v_Normal);
    float NdotL = max(dot(normal, lightDir), 0.0);
    
    // Base color with time-based variation (for hot reload testing)
    vec3 baseColor = vec3(0.5 + 0.3 * sin(u_Time), 0.3, 0.7);
    
    // Simple lighting
    vec3 ambient = baseColor * 0.2;
    vec3 diffuse = baseColor * NdotL;
    
    FragColor = vec4(ambient + diffuse, 1.0);
}