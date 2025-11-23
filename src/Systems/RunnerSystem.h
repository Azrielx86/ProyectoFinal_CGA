//
// Created by tohka on 11/22/25.
//

#ifndef PROYECTOFINAL_CGA_RUNNERSYSTEM_H
#define PROYECTOFINAL_CGA_RUNNERSYSTEM_H

#include "ECS/ISystem.h"

constexpr float gravity = -9.81f;

class RunnerSystem final : public ECS::ISystem
{
    bool downTriggered = false;
    int targetLane = 0;
    float laneWidth = 2.0f;
    float horizontalSpeed = 10.0f;
    bool leftPressed = false;
    bool rightPressed = false;

  public:
    void Update(ECS::Registry &registry, float deltaTime) override;
};

#endif // PROYECTOFINAL_CGA_RUNNERSYSTEM_H
