#version 400 core

in vec2 vuv;
uniform sampler2D textureUniform;
out vec4 frag_colour;

void main()
{
    frag_colour = texture(textureUniform, vuv);
}