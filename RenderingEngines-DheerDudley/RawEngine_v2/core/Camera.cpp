//
// Created by dheer on 13-10-2025.
//
#include "Camera.h"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

namespace core {
    Camera::Camera() {
        Up = glm::vec3(0.0f, 1.0f, 0.0f);
        cameraPos = glm::vec3(0.0f, 0.0f, 10.0f);
        cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
        cameraForward = glm::normalize(cameraTarget-cameraPos);
        cameraRight = glm::normalize(glm::cross(cameraForward, Up));
        cameraUp = glm::normalize(glm::cross(cameraRight, cameraForward));

    }
    // void Camera::Move(glm::vec3 Position) {
    //
    // }
    Camera::~Camera() {

    }

} // core