#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool isWater;
uniform float time;

void main()
{
    vec3 pos = aPos;

    if (isWater)
    {
        // Streaming river animation: gentle crossing waves
        pos.y += sin(pos.x * 2.0 + time * 1.5) * 0.07
               + sin(pos.z * 1.5 - time * 2.5) * 0.06
               + cos(pos.x * 0.8 - pos.z * 1.2 + time * 1.8) * 0.04;
    }

    gl_Position = projection * view * model * vec4(pos, 1.0);
    FragPos     = vec3(model * vec4(pos, 1.0));
    Normal      = mat3(transpose(inverse(model))) * aNormal;
}
