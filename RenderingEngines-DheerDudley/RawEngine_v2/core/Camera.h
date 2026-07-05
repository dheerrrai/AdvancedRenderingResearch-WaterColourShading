#pragma once


//
// Created by dheer on 13-10-2025.
//
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#ifndef RAWENGINE_CAMERA_H
#define RAWENGINE_CAMERA_H



namespace core {
    class Camera {
    public:
        glm::vec3 cameraPos;
        glm::vec3 cameraTarget;
        glm::vec3 cameraForward;
        glm::vec3 Up ;
        glm::vec3 cameraRight ;
        glm::vec3 cameraUp;
        Camera();
        ~Camera();
        // void Camera::Move(glm::vec3 Position);
    };
}
#endif //RAWENGINE_CAMERA_H
