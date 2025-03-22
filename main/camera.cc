#include "main/camera.h"

#include <algorithm>

glm::mat4 Camera::getPerspectiveMatrix() const {
    return perspective_matrix_;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(camera_pos_, camera_pos_ + camera_direction_, up_direction_);
}

glm::vec3 Camera::getUpDirection() const {
    return up_direction_;
}

float Camera::getAspectRatio() const {
    return static_cast<float>(width_) / static_cast<float>(height_);
}

void Camera::setScreenSize(size_t width, size_t height) {
    width_ = width;
    height_ = height;

    perspective_matrix_ = glm::perspective(glm::radians(fov_y_), getAspectRatio(), near_clip_, far_clip_);
}

void Camera::move(float dx, float dy) {
    glm::vec3 x_delta = glm::normalize(glm::cross(camera_direction_, up_direction_)) * dx;
    glm::vec3 y_delta = glm::vec3(0.0f, 1.0f, 0.0f) * dy;

    camera_pos_ += x_delta + y_delta;
}

void Camera::rotateBy(float d_yaw_, float d_pitch_) {
    pitch_ = std::clamp(pitch_ + d_pitch_, -89.9f, 89.9f);
    yaw_ += d_yaw_;

    computeDirection();
}

void Camera::computeDirection() {
    glm::vec3 direction;
    direction.x = std::cos(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));
    direction.y = std::sin(glm::radians(pitch_));
    direction.z = std::sin(glm::radians(yaw_)) * std::cos(glm::radians(pitch_));
    camera_direction_ = glm::normalize(direction);
}