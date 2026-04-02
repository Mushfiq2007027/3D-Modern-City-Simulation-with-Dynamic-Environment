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
        ambient(0.3f, 0.3f, 0.3f),
        diffuse(0.7f, 0.7f, 0.6f),
        specular(1.0f, 1.0f, 0.9f),
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
        ambient = glm::vec3(0.35f, 0.35f, 0.32f);
        diffuse = glm::vec3(0.85f, 0.82f, 0.72f);
        specular = glm::vec3(1.0f, 1.0f, 0.95f);
    }

    void setNight()
    {
        direction = glm::vec3(0.1f, -1.0f, 0.2f);
        ambient = glm::vec3(0.06f, 0.06f, 0.12f);
        diffuse = glm::vec3(0.12f, 0.12f, 0.20f);
        specular = glm::vec3(0.2f, 0.2f, 0.3f);
    }
};

#endif
#pragma once
