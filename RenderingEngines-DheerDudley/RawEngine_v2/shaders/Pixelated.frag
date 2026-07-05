#version 400
in vec2 vuv;
uniform sampler2D textureUniform;
uniform int PixelSize;
uniform vec2 ScreenSize;
out vec4 frag_colour;
void main() {
        vec2 PixelatedSpot = vuv * ScreenSize;

        PixelatedSpot = floor(PixelatedSpot/PixelSize)*PixelSize;

        vec2 PixelatedUv = PixelatedSpot/ScreenSize;

        vec4 value = texture(textureUniform, PixelatedUv);
        vec4 Basevalue = texture(textureUniform, vuv);
        frag_colour = vec4(value.xyz,1.0);
}
