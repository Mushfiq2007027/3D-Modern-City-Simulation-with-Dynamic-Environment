#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// ============================================================
//  Full 6-DOF Bird's-Eye Camera
//  Rotation:  X = Pitch,  Y = Yaw,  Z = Roll
//  Movement:  W/S = forward/back, A/D = left/right, E/R = up/down
//  Orbit:     F key spins the camera around its look-at target
// ============================================================
class Camera
{
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    glm::vec3 Target;       // orbit centre (look-at point)

    float Yaw;              // degrees
    float Pitch;            // degrees
    float Roll;             // degrees
    float Zoom;             // field-of-view in degrees
    float MoveSpeed;
    float RotSpeed;         // degrees per second for key-rotation

    // Construct from position + target
    Camera(glm::vec3 position = glm::vec3(0, 5, 15),
        glm::vec3 target = glm::vec3(0, 0, 0))
        : WorldUp(0, 1, 0), Target(target), Roll(0), Zoom(45), MoveSpeed(55), RotSpeed(120)
    {
        Position = position;
        // Derive initial Yaw/Pitch from the position->target direction
        glm::vec3 dir = glm::normalize(target - position);
        Pitch = glm::degrees(asinf(dir.y));
        Yaw = glm::degrees(atan2f(dir.z, dir.x));
        updateVectors();
    }

    // ---- View matrix ----
    glm::mat4 GetViewMatrix() const
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // ---- Rotation keys ----
    void RotatePitch(float deg) { Pitch += deg; Pitch = glm::clamp(Pitch, -89.f, 89.f); updateVectors(); }
    void RotateYaw(float deg) { Yaw += deg; updateVectors(); }
    void RotateRoll(float deg) { Roll += deg; updateVectors(); }

    // ---- Movement (camera-relative) ----
    void MoveForward(float dt) { Position += Front * MoveSpeed * dt; }
    void MoveBackward(float dt) { Position -= Front * MoveSpeed * dt; }
    void MoveLeft(float dt) { Position -= Right * MoveSpeed * dt; }
    void MoveRight(float dt) { Position += Right * MoveSpeed * dt; }
    void MoveUp(float dt) { Position += Up * MoveSpeed * dt; }
    void MoveDown(float dt) { Position -= Up * MoveSpeed * dt; }

    // ---- Orbit around Target (F key) ----
    void OrbitLeft(float dt)
    {
        float angle = glm::radians(RotSpeed * dt);
        float ca = cosf(angle), sa = sinf(angle);
        glm::vec3 rel = Position - Target;
        // Rotate around WorldUp
        glm::vec3 newRel(
            rel.x * ca + rel.z * sa,
            rel.y,
            -rel.x * sa + rel.z * ca
        );
        Position = newRel + Target;
        Front = glm::normalize(Target - Position);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }

    // ---- Mouse scroll zooms FOV ----
    void ProcessMouseScroll(float yoffset)
    {
        Zoom -= yoffset;
        if (Zoom < 5.0f)  Zoom = 5.0f;
        if (Zoom > 90.0f) Zoom = 90.0f;
    }

private:
    void updateVectors()
    {
        // Front from Yaw + Pitch (Euler angles)
        glm::vec3 f;
        f.x = cosf(glm::radians(Yaw)) * cosf(glm::radians(Pitch));
        f.y = sinf(glm::radians(Pitch));
        f.z = sinf(glm::radians(Yaw)) * cosf(glm::radians(Pitch));
        Front = glm::normalize(f);

        // Base right (no roll)
        glm::vec3 baseRight = glm::normalize(glm::cross(Front, WorldUp));

        // Apply Roll around Front axis
        float rollRad = glm::radians(Roll);
        Right = glm::normalize(
            baseRight * cosf(rollRad) +
            glm::cross(Front, baseRight) * sinf(rollRad)
        );
        Up = glm::normalize(glm::cross(Right, Front));
    }
};
#endif
