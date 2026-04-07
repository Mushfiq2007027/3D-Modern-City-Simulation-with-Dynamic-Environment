#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

// ============================================================
//  Full 6-DOF Camera
//  Pitch = X key   Yaw = Y key   Roll = Z key  (Shift = inverse)
//  W/S = forward/back  A/D = left/right  E/R = up/down
//  F   = orbit around world centre (0,0,0)
// ============================================================
class Camera
{
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw;       // degrees
    float Pitch;     // degrees
    float Roll;      // degrees
    float Zoom;
    float MoveSpeed;
    float RotSpeed;

    Camera(glm::vec3 position = glm::vec3(0, 5, 15),
        glm::vec3 target = glm::vec3(0, 0, 0))
        : WorldUp(0, 1, 0), Roll(0), Zoom(45), MoveSpeed(220), RotSpeed(85)
    {
        Position = position;
        glm::vec3 dir = glm::normalize(target - position);
        Pitch = glm::degrees(asinf(glm::clamp(dir.y, -1.0f, 1.0f)));
        Yaw = glm::degrees(atan2f(dir.z, dir.x));
        Roll = 0;
        updateVectors();
    }

    glm::mat4 GetViewMatrix() const
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // --- Rotation (degrees) ---
    void RotatePitch(float deg)
    {
        Pitch += deg;
        Pitch = glm::clamp(Pitch, -89.0f, 89.0f);
        updateVectors();
    }
    void RotateYaw(float deg) { Yaw += deg;  updateVectors(); }
    void RotateRoll(float deg) { Roll += deg; updateVectors(); }

    // --- Movement (fully camera-relative) ---
    void MoveForward(float dt) { Position += Front * MoveSpeed * dt; }
    void MoveBackward(float dt) { Position -= Front * MoveSpeed * dt; }
    void MoveLeft(float dt) { Position -= Right * MoveSpeed * dt; }
    void MoveRight(float dt) { Position += Right * MoveSpeed * dt; }
    void MoveUp(float dt) { Position += Up * MoveSpeed * dt; }
    void MoveDown(float dt) { Position -= Up * MoveSpeed * dt; }

    // --- F key: orbit around world centre (0,0,0) ---
    void OrbitLeft(float dt)
    {
        float angle = glm::radians(RotSpeed * dt);
        float ca = cosf(angle), sa = sinf(angle);
        // Rotate Position around world Y-axis
        float nx = Position.x * ca + Position.z * sa;
        float nz = -Position.x * sa + Position.z * ca;
        Position.x = nx;
        Position.z = nz;
        // Always look at the centre
        glm::vec3 dir = glm::normalize(-Position);   // toward (0,0,0)
        Pitch = glm::degrees(asinf(glm::clamp(dir.y, -1.0f, 1.0f)));
        Yaw = glm::degrees(atan2f(dir.z, dir.x));
        Roll = 0;
        updateVectors();
    }

    void ProcessMouseScroll(float yoff)
    {
        Zoom -= yoff;
        Zoom = glm::clamp(Zoom, 5.0f, 90.0f);
    }

private:
    void updateVectors()
    {
        glm::vec3 f;
        f.x = cosf(glm::radians(Yaw)) * cosf(glm::radians(Pitch));
        f.y = sinf(glm::radians(Pitch));
        f.z = sinf(glm::radians(Yaw)) * cosf(glm::radians(Pitch));
        Front = glm::normalize(f);

        glm::vec3 baseR = glm::normalize(glm::cross(Front, WorldUp));
        float rr = glm::radians(Roll);
        Right = glm::normalize(baseR * cosf(rr) +
            glm::cross(Front, baseR) * sinf(rr));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};
#endif
