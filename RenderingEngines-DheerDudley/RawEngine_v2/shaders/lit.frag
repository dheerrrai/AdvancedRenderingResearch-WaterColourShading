#version 400 core

out vec4 FragColor; // please


in vec3 fPos;
in vec4 fNor; // ?
in vec2 uv;
in vec4 fPosWorld;
uniform vec3 lightDirection;

uniform float ambientIntensity;
uniform vec3 ambientCol;
// uniform vec3 LightCol = vec3(1.0f,1.0f,1.0f);
uniform float constAttenuation;
uniform float linearAttenuation;
uniform float quadAttenuation;

uniform vec3 diffuseCol;
uniform vec3 camPos;
uniform vec3 specCol;

uniform float specIntensity;
uniform sampler2D text;


// //
// //
// //
// uniform vec3 lightPos = vec3(1.0f,1.0f,1.0f);
// // uniform vec4 LightCol =vec4(0.5f,1.0f,1.0f,1.0f);
//uniform vec3 lightCol;// =vec3(0.5f,1.0f,1.0f);


void main()
{
        // TODO: uniform -> it's in world space
        vec4 texColor = texture(text, uv);
        vec3 viewDir= fPosWorld.xyz + camPos;

        vec3 direction = lightDirection - fPosWorld.xyz;
        float dotProduct = dot(fNor.xyz,normalize(direction));
        vec3 halfDir = normalize(viewDir + direction);

        float NdotL = max(dotProduct, 0);//, ; // dot product in different spaces???
        float distance = length(lightDirection-fPosWorld.xyz);
//        float distance = 5.0f;
        vec3 specular = pow(max(dot(fNor.xyz, halfDir), 0.0), specIntensity * 2.0) * specCol;
//        vec3 specular = vec3(0,0,0);
        vec3 diffuse = NdotL * diffuseCol * texColor.rgb;



        vec3 attenuation = (diffuse + specular)/ (constAttenuation + linearAttenuation * distance + quadAttenuation * distance * distance);

        vec3 ambient = ambientCol * ambientIntensity * texColor.rgb;
        //FragColor =   vec4(fNor,1.0f);

        FragColor = vec4(attenuation + ambient,texColor.a);
//        FragColor = vec4(1,1,1,1);
//    FragColor = Lights[1].colour + Lights[0].colour;
}

// #version 400 core
// out vec4 FragColor;
// in vec3 fPos;
// in vec3 fNor;
// in vec2 uv;
//
// void main()
// {
//    FragColor = vec4(fNor.x, fNor.y, fNor.z, 1);
// }