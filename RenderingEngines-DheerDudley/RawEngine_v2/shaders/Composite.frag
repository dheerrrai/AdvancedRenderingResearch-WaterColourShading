#version 400 core
out vec4 FragColor;
in vec2 vuv;

uniform sampler2D bleedTex;
uniform sampler2D edgeTex;
uniform float EdgeThreshold;

float luminance(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec3 bleedColor = texture(bleedTex, vuv).rgb;
    vec3 edgeColor  = texture(edgeTex, vuv).rgb;
    float edgeLum = luminance(edgeColor);

    float lineMask = smoothstep(EdgeThreshold, EdgeThreshold * 0.5, edgeLum);
    vec3 result = mix(bleedColor, edgeColor, lineMask);

    FragColor = vec4(result, 1.0);
}