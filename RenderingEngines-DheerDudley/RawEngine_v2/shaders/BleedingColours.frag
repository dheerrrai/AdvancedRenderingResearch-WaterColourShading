#version 400 core
out vec4 FragColor;
in vec2 vuv;

uniform sampler2D textureUniform;
uniform vec2 ScreenSize;
uniform float BleedStrength;

void main()
{
        vec2 texel = 1.0 / ScreenSize;
        vec4 centerSample = texture(textureUniform, vuv);
        vec3 center = centerSample.rgb;

        const int RADIUS = 6; // wide enough that colour visibly spreads
        const float SIGMA = 3.0;

        vec3 accum = vec3(0.0);
        float totalWeight = 0.0;

        for (int x = -RADIUS; x <= RADIUS; x++) {
                for (int y = -RADIUS; y <= RADIUS; y++) {
                        vec3 sampleColor = texture(textureUniform, vuv + vec2(x, y) * texel).rgb;
                        float dist = length(vec2(x, y));
                        float weight = exp(-(dist * dist) / (2.0 * SIGMA * SIGMA));
                        accum += sampleColor * weight;
                        totalWeight += weight;
                }
        }

        vec3 blurred = accum / totalWeight;
        vec3 result = mix(center, blurred, BleedStrength);
        FragColor = vec4(result, centerSample.a);
}