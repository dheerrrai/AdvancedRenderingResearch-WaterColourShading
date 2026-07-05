#version 400 core
out vec4 FragColor;
in vec2 vuv;
uniform sampler2D textureUniform;
uniform float WhiteThreshold;

vec3 boostSaturation(vec3 color, float amount) {
        float luminance = dot(color, vec3(0.299, 0.587, 0.114));
        return mix(vec3(luminance), color, amount);
}
void main() {

        vec4 texColor = texture(textureUniform, vuv);
        vec3 color;
        if (texColor.a < 0.5) {
                color = vec3(1.0);
        } else {
                float luminance = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
                float whiteness = pow(luminance, WhiteThreshold);
                color = mix(vec3(1.0), texColor.rgb, whiteness);
        }
        FragColor = vec4(color, texColor.a);
}