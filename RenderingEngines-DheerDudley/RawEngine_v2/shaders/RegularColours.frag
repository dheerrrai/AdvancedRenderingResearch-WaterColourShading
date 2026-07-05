#version 400
in vec2 vuv;
uniform sampler2D textureUniform;
out vec4 frag_colour;
void main() {
        vec4 value = texture(textureUniform, vuv);
        frag_colour = vec4(vec3(value),1.0);
}
