#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec3 VertexColor;   // for texMode==3 (color on vertex)

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
        // Streaming river animation
       // pos.y += sin(pos.x * 2.0 + time * 1.85) * 0.07
              // + sin(pos.z * 1.5 - time * 3.0) * 0.06
              // + cos(pos.x * 0.8 - pos.z * 1.2 + time * 2.15) * 0.04;
        
        pos.y += sin(pos.x * 2.0 + time * 2) * 0.10
               + sin(pos.z * 1.5 - time * 3.5) * 0.09
               + cos(pos.x * 0.8 - pos.z * 1.2 + time * 2.5) * 0.07;
    }

    gl_Position = projection * view * model * vec4(pos, 1.0);
    FragPos     = vec3(model * vec4(pos, 1.0));
    Normal      = mat3(transpose(inverse(model))) * aNormal;
    TexCoords   = aTexCoords;

    // Procedural vertex color based on position (used in texMode==3)
    VertexColor = fract(abs(aPos) * vec3(0.12, 0.18, 0.14));
}
