#ifndef DIRECTIONALLIGHT_H
#define DIRECTIONALLIGHT_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "shader.h"

class DirectionalLight
{
public:
    glm::vec3 direction;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    bool enabled;

    DirectionalLight()
        : direction(-0.3f, -1.0f, -0.5f),
        ambient(0.50f, 0.50f, 0.48f),
        diffuse(0.95f, 0.92f, 0.85f),
        specular(1.0f, 1.0f, 0.95f),
        enabled(true) {
    }

    void setUpLight(const Shader& shader) const
    {
        shader.use();
        shader.setVec3("dirLight.direction", direction);
        if (enabled)
        {
            shader.setVec3("dirLight.ambient", ambient);
            shader.setVec3("dirLight.diffuse", diffuse);
            shader.setVec3("dirLight.specular", specular);
        }
        else
        {
            shader.setVec3("dirLight.ambient", glm::vec3(0));
            shader.setVec3("dirLight.diffuse", glm::vec3(0));
            shader.setVec3("dirLight.specular", glm::vec3(0));
        }
    }

    void setDay()
    {
        direction = glm::vec3(-0.3f, -1.0f, -0.5f);
        ambient = glm::vec3(0.52f, 0.52f, 0.48f);
        diffuse = glm::vec3(0.98f, 0.95f, 0.88f);
        specular = glm::vec3(1.0f, 1.0f, 0.95f);
    }

    void setNight()
    {
        direction = glm::vec3(0.1f, -1.0f, 0.2f);
        ambient = glm::vec3(0.02f, 0.02f, 0.05f);
        diffuse = glm::vec3(0.05f, 0.05f, 0.10f);
        specular = glm::vec3(0.1f, 0.1f, 0.15f);
    }
};

#endif
#pragma once
