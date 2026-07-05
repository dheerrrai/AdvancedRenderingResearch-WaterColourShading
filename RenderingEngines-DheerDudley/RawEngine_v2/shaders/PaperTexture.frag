// PaperTexture.frag
#version 400 core
out vec4 FragColor;
in vec2 vuv;
uniform sampler2D textureUniform;
uniform vec2 ScreenSize;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

void main() {

    vec4 texColor = texture(textureUniform, vuv);
    float grain = hash(vuv * ScreenSize) * 0.05 - 0.025;
    FragColor = vec4(texColor.rgb + grain, texColor.a);
}