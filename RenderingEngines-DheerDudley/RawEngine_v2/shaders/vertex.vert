#version 400 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNor;
layout (location = 2) in vec2 aUv;
out vec3 fPos;
out vec3 fNor;
out vec2 uv;
uniform mat4 mvpMatrix;

void main()
{

       fPos = vec3(modelMatrix * vec4(aPos, 1.0));
       fNor = normalize(mat3(modelMatrix) * aNor);
   uv = aUv;
   gl_Position = mvpMatrix * vec4(aPos, 1.0);
}