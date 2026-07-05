#version 400
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNor;
layout (location = 2) in vec2 aUv;
uniform mat4 mvpMatrix;
uniform mat4 mMatrix;
out vec3 fPos;
out vec4 fNor;
out vec2 uv;
out vec4 fPosWorld;

void main() {
  fPos = aPos;
  fPosWorld = vec4(fPos,0) * mMatrix;
  fNor =  mMatrix * vec4(aNor,0);
  uv = aUv;
  gl_Position = mvpMatrix * vec4(aPos, 1.0); // transform from object space to clip space
}