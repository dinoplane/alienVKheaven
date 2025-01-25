#pragma once

#include <vk_types.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const float DEFAULT_YAW         = -135.0f;
const float DEFAULT_PITC       =  -45.0f;
const float DEFAULT_SPEED       =  5.0f;
const float DEFAULT_SENSITIVITY =  0.1f;
const float DEFAULT_FOVY        =  20.0f;
const float DEFAULT_NEAR        =  0.1f;
const float DEFAULT_FAR         =  100000.0f;

class Camera {
    // float fov;
    public:
        glm::vec3 position;

        float speed;
        float sensitivity;

        glm::vec3 front;
        glm::vec3 right;
        glm::vec3 worldUp;
        glm::vec3 up;

        // glm::mat4 viewMat;

        // glm::mat4 projMat;


        float yaw;
        float pitch;

        float width;
        float height;

        float fovY;
        float zNear;
        float zFar;


    public:
        Camera(
                float w=1280, float h=720,
                glm::vec3 pos = glm::vec3(5.0f, 5.0f, 5.0f),
                float yaw_val = DEFAULT_YAW, float pitch_val = DEFAULT_PITC,
                float fovY_val = DEFAULT_FOVY, float zNear_val = DEFAULT_NEAR, float zFar_val = DEFAULT_FAR
            ) : speed(DEFAULT_SPEED), sensitivity(DEFAULT_SENSITIVITY){
            // viewMat = glm::mat4(1.0);
            position = pos;
            worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
            yaw = yaw_val;
            pitch = pitch_val;

            fovY = fovY_val;
            zNear = zNear_val;
            zFar = zFar_val;

            setPerspectiveSize(w, h);
            updateCameraVectors();
        }

        void moveLeft(float deltaTime);
        void moveRight(float deltaTime);
        void moveForward(float deltaTime);
        void moveBackward(float deltaTime);
        void moveUp(float deltaTime);
        void moveDown(float deltaTime);


        void processMouseMovement(float xoffset, float yoffset);

        void setPerspectiveSize(float w, float h){
            width = w;
            height = h;
        }

        glm::mat4 getViewMatrix(){
            return glm::lookAt(position, position + front, up);
        }

        glm::mat4 getProjMatrix(){
            return glm::perspective(glm::radians(fovY), width / height, zFar, zNear);
        }

        glm::mat4 GetModelMatrix(){ // We use quaternions omg this is actual brainrot
            glm::mat4 tmpModelMat = glm::identity<glm::mat4>();

            tmpModelMat = glm::translate(tmpModelMat, position);
            tmpModelMat = glm::rotate(tmpModelMat, glm::radians(-yaw), glm::vec3(0.0, 1.0, 0.0));
            tmpModelMat = glm::rotate(tmpModelMat, glm::radians(pitch), glm::vec3(0.0, 0.0, 1.0));
            // // tmpModelMat = glm::rotate(tmpModelMat, glm::radians(roll), glm::vec3(0.0, 0.0, 1.0));

            return tmpModelMat;
            // return getProjMatrix() * getViewMatrix();
        }

        static Frustum createFrustumFromCamera(const Camera& cam);

    private:
        void updateCameraVectors();

};