#ifndef SPOTLIGHT_H
#define SPOTLIGHT_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "shader.h"

class SpotLight
{
public:
    glm::vec3 position;
    glm::vec3 direction;
    float cutOff;
    float outerCutOff;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float k_c, k_l, k_q;
    bool enabled;

    SpotLight()
        : position(0, 10, 0), direction(0, -1, 0),
        cutOff(glm::cos(glm::radians(20.0f))),
        outerCutOff(glm::cos(glm::radians(30.0f))),
        ambient(0.05f, 0.05f, 0.05f),
        diffuse(0.9f, 0.9f, 0.8f),
        specular(1, 1, 1),
        k_c(1.0f), k_l(0.045f), k_q(0.0075f),
        enabled(true) {
    }

    void setUpLight(const Shader& shader) const
    {
        shader.use();
        shader.setVec3("spotLight.position", position);
        shader.setVec3("spotLight.direction", direction);
        shader.setFloat("spotLight.cutOff", cutOff);
        shader.setFloat("spotLight.outerCutOff", outerCutOff);
        shader.setFloat("spotLight.k_c", k_c);
        shader.setFloat("spotLight.k_l", k_l);
        shader.setFloat("spotLight.k_q", k_q);
        if (enabled)
        {
            shader.setVec3("spotLight.ambient", ambient);
            shader.setVec3("spotLight.diffuse", diffuse);
            shader.setVec3("spotLight.specular", specular);
        }
        else
        {
            shader.setVec3("spotLight.ambient", glm::vec3(0));
            shader.setVec3("spotLight.diffuse", glm::vec3(0));
            shader.setVec3("spotLight.specular", glm::vec3(0));
        }
    }
};

#endif
#pragma once
