#ifndef POINTLIGHT_H
#define POINTLIGHT_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "shader.h"
#include <string>

class PointLight
{
public:
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float k_c, k_l, k_q;
    int lightNumber;

    PointLight() : position(0), ambient(0), diffuse(0), specular(0), k_c(1), k_l(0), k_q(0), lightNumber(0) {}

    PointLight(float px, float py, float pz,
        float ar, float ag, float ab,
        float dr, float dg, float db,
        float sr, float sg, float sb,
        float kc, float kl, float kq, int num)
    {
        position = glm::vec3(px, py, pz);
        ambient = glm::vec3(ar, ag, ab);
        diffuse = glm::vec3(dr, dg, db);
        specular = glm::vec3(sr, sg, sb);
        k_c = kc; k_l = kl; k_q = kq;
        lightNumber = num;
    }

    void setUpPointLight(const Shader& shader) const
    {
        shader.use();
        std::string base = "pointLights[" + std::to_string(lightNumber - 1) + "].";
        shader.setVec3(base + "position", position);
        shader.setVec3(base + "ambient", ambientOn * ambient);
        shader.setVec3(base + "diffuse", diffuseOn * diffuse);
        shader.setVec3(base + "specular", specularOn * specular);
        shader.setFloat(base + "k_c", k_c);
        shader.setFloat(base + "k_l", k_l);
        shader.setFloat(base + "k_q", k_q);
    }

    void turnOff() { ambientOn = diffuseOn = specularOn = 0.0f; }
    void turnOn() { ambientOn = diffuseOn = specularOn = 1.0f; }
    void turnAmbientOn() { ambientOn = 1.0f; }
    void turnAmbientOff() { ambientOn = 0.0f; }
    void turnDiffuseOn() { diffuseOn = 1.0f; }
    void turnDiffuseOff() { diffuseOn = 0.0f; }
    void turnSpecularOn() { specularOn = 1.0f; }
    void turnSpecularOff() { specularOn = 0.0f; }

private:
    float ambientOn = 1.0f;
    float diffuseOn = 1.0f;
    float specularOn = 1.0f;
};

#endif
#pragma once
