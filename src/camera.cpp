#include "camera.h"
#include <fmt/core.h>
#include <fmt/printf.h>

void Camera::moveLeft(float deltaTime){
    position -= right * deltaTime * speed;
}

void Camera::moveRight(float deltaTime){
    position += right * deltaTime * speed;
}

void Camera::moveForward(float deltaTime){
    position += front * deltaTime * speed;
}

void Camera::moveBackward(float deltaTime){
    position -= front * deltaTime * speed;
}


void Camera::moveUp(float deltaTime){
    position += glm::vec3(0.0, 1.0, 0.0) * deltaTime * speed;
}
void Camera::moveDown(float deltaTime){
    position -= glm::vec3(0.0, 1.0, 0.0) * deltaTime * speed;
}





void Camera::processMouseMovement(float xoffset, float yoffset){
    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    if(pitch > 89.0f)
        pitch = 89.0f;
    if(pitch < -89.0f)
        pitch = -89.0f;

    updateCameraVectors();
}
void Camera::updatePosition(float deltaTime){
    position += (velocity.z*front + velocity.x*right + velocity.y*glm::vec3(0.0f, 1.0f, 0.0f)) * deltaTime * speed;
}

void Camera::updateCameraVectors(){
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(direction);
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
    
}

Frustum Camera::createFrustumFromCamera(const Camera& cam)
{
    const float aspect = cam.width / cam.height;
    const float halfVSide = cam.zFar * tanf(glm::radians(cam.fovY) * .5f);
    const float halfHSide = halfVSide * aspect;
    const glm::vec3 frontMultFar = cam.zFar * cam.front;

    Frustum     frustum{
    /*bottomFace*/ {cam.position,
                            glm::cross(cam.right, frontMultFar - cam.up * halfVSide) },
    /*topFace*/ {cam.position,
                            glm::cross(frontMultFar + cam.up * halfVSide, cam.right) },

    /*leftFace*/ {cam.position,
                            glm::cross(frontMultFar - cam.right * halfHSide, cam.up) },
    /*rightFace*/ {cam.position,
                            glm::cross(cam.up, frontMultFar + cam.right * halfHSide) },


    /*farFace*/ {cam.position + frontMultFar, -cam.front },
    /*nearFace*/ {cam.position + cam.zNear * cam.front, cam.front },
    };

    return frustum;
}