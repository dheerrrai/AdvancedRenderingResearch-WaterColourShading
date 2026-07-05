#version 400 core
out vec4 FragColor;
in vec2 vuv;
uniform sampler2D textureUniform;
uniform vec2 ScreenSize;
uniform float EdgeDarkenStrength; // try 0.3-0.6

void main() {
    vec2 texel = 1.0 / ScreenSize;
    vec3 center = texture(textureUniform, vuv).rgb;


    float lC = dot(center, vec3(0.299, 0.587, 0.114));
    float lL = dot(texture(textureUniform, vuv - vec2(texel.x, 0.0)).rgb, vec3(0.299, 0.587, 0.114));
    float lR = dot(texture(textureUniform, vuv + vec2(texel.x, 0.0)).rgb, vec3(0.299, 0.587, 0.114));
    float lU = dot(texture(textureUniform, vuv + vec2(0.0, texel.y)).rgb, vec3(0.299, 0.587, 0.114));
    float lD = dot(texture(textureUniform, vuv - vec2(0.0, texel.y)).rgb, vec3(0.299, 0.587, 0.114));

    float edge = abs(lL - lR) + abs(lU - lD);
    edge = smoothstep(0.02, 0.15, edge); // tune thresholds to taste

    vec3 color = mix(center, center * (1.0 - EdgeDarkenStrength), edge);
    FragColor = vec4(color, texture(textureUniform, vuv).a);
}

