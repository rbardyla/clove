// Simple fragment shader
#version 330 core

in vec2 uv;
out vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 tintColor;

void main() {
    vec4 texColor = texture(texture0, uv);
    fragColor = texColor * tintColor;
}
