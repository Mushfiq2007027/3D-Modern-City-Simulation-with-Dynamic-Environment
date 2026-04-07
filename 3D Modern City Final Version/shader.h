#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader
{
public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr)
    {
        std::string vertexCode, fragmentCode, geometryCode;
        std::ifstream vShaderFile, fShaderFile, gShaderFile;
        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        gShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try
        {
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vss, fss;
            vss << vShaderFile.rdbuf();
            fss << fShaderFile.rdbuf();
            vShaderFile.close();
            fShaderFile.close();
            vertexCode = vss.str();
            fragmentCode = fss.str();
            if (geometryPath != nullptr)
            {
                gShaderFile.open(geometryPath);
                std::stringstream gss;
                gss << gShaderFile.rdbuf();
                gShaderFile.close();
                geometryCode = gss.str();
            }
        }
        catch (std::ifstream::failure& e)
        {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
        }
        const char* vCode = vertexCode.c_str();
        const char* fCode = fragmentCode.c_str();

        unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        unsigned int geometry = 0;
        if (geometryPath != nullptr)
        {
            const char* gCode = geometryCode.c_str();
            geometry = glCreateShader(GL_GEOMETRY_SHADER);
            glShaderSource(geometry, 1, &gCode, NULL);
            glCompileShader(geometry);
            checkCompileErrors(geometry, "GEOMETRY");
        }

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        if (geometryPath != nullptr) glAttachShader(ID, geometry);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);
        if (geometryPath != nullptr) glDeleteShader(geometry);
    }

    void use() const { glUseProgram(ID); }

    void setBool(const std::string& n, bool  v) const { glUniform1i(glGetUniformLocation(ID, n.c_str()), (int)v); }
    void setInt(const std::string& n, int   v) const { glUniform1i(glGetUniformLocation(ID, n.c_str()), v); }
    void setFloat(const std::string& n, float v) const { glUniform1f(glGetUniformLocation(ID, n.c_str()), v); }
    void setVec2(const std::string& n, const glm::vec2& v) const { glUniform2fv(glGetUniformLocation(ID, n.c_str()), 1, &v[0]); }
    void setVec3(const std::string& n, const glm::vec3& v) const { glUniform3fv(glGetUniformLocation(ID, n.c_str()), 1, &v[0]); }
    void setVec3(const std::string& n, float x, float y, float z) const { glUniform3f(glGetUniformLocation(ID, n.c_str()), x, y, z); }
    void setVec4(const std::string& n, const glm::vec4& v) const { glUniform4fv(glGetUniformLocation(ID, n.c_str()), 1, &v[0]); }
    void setMat4(const std::string& n, const glm::mat4& m) const { glUniformMatrix4fv(glGetUniformLocation(ID, n.c_str()), 1, GL_FALSE, &m[0][0]); }

private:
    void checkCompileErrors(GLuint shader, std::string type)
    {
        GLint success; GLchar log[1024];
        if (type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) { glGetShaderInfoLog(shader, 1024, NULL, log); std::cout << "SHADER_ERROR:" << type << "\n" << log << std::endl; }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) { glGetProgramInfoLog(shader, 1024, NULL, log); std::cout << "PROGRAM_LINK_ERROR\n" << log << std::endl; }
        }
    }
};
#endif
#pragma once
