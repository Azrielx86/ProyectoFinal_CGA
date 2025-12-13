//
// Created by tohka on 11/22/25.
//

#ifndef PROYECTOFINAL_CGA_RUNNERCOMPONENT_H
#define PROYECTOFINAL_CGA_RUNNERCOMPONENT_H

#include <glm/glm.hpp>

struct RunnerComponent
{
    bool isJumping = false;
    bool grounded = false;
    float weight = 2.0f;
    float moveSpeed = 5.0f;
    float jumpForce = 8.0f;
    glm::vec3 velocity{0.0f};
    int score = 0;
    int obstacleHits = 0;
    int nextExtraLife = 1000;
};

#endif // PROYECTOFINAL_CGA_RUNNERCOMPONENT_H
