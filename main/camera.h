#pragma once

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL 
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::mat4 getPerspectiveMatrix() const;
    glm::mat4 getViewMatrix() const;
    glm::vec3 getUpDirection() const;
    float getAspectRatio() const;
    void setScreenSize(size_t width, size_t height);
    void move(float dx, float dy);

private:
    size_t width_;
    size_t height_;

    // FOV in y direction in degrees.
    float fov_y_ = 45.0f;
    float near_clip_ = 0.1f;
    float far_clip_ = 10000.0f;

    glm::vec3 camera_pos_ = glm::vec3(0.0f, 25.0f, 180.0f);
    glm::vec3 camera_direction_ = glm::vec3(0.0f, 0.0f, 0.0f) - camera_pos_;
    glm::vec3 up_direction_ = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 perspective_matrix_;
};