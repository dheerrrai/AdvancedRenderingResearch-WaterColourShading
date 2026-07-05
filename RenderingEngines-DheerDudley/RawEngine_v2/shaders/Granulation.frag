// Granulation.frag
#version 400 core
out vec4 FragColor;
in vec2 vuv;

uniform sampler2D textureUniform;
uniform vec2 ScreenSize;
uniform float GranulationStrength=0.15f; // Clamp to 0.15-0.3

float hash(vec2 p) {
        return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// cheap value noise, a bit smoother than raw hash so it doesn't look like TV static
float noise(vec2 p) {
        vec2 i = floor(p);
        vec2 f = fract(p);
        float a = hash(i);
        float b = hash(i + vec2(1.0, 0.0));
        float c = hash(i + vec2(0.0, 1.0));
        float d = hash(i + vec2(1.0, 1.0));
        vec2 u = f * f * (3.0 - 2.0 * f);
        return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

//void main()
//{
//        FragColor = texture(textureUniform, vuv);
//}
void main()
{
        vec4 texColor = texture(textureUniform, vuv);

        // sample noise at a texel-scale frequency so grain reads as paper-fine, not blotchy
        float grain = noise(vuv * ScreenSize * 0.5) - 0.5;

        // weight by inverse luminance: pigment settles more where color is denser/darker
        float luminance = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
        float density = 1.0 - luminance;

        vec3 color = texColor.rgb - grain * density * GranulationStrength;

        FragColor = vec4(clamp(color, 0.0, 1.0), texColor.a);
}