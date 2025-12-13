//
// Created by tohka on 11/22/25.
//

#ifndef PROYECTOFINAL_CGA_RUNNERSYSTEM_H
#define PROYECTOFINAL_CGA_RUNNERSYSTEM_H

#include "ECS/ISystem.h"

namespace Input
{
class Keyboard;
class Joystick;
} // namespace Input

constexpr float gravity = -9.81f;

class RunnerSystem final : public ECS::ISystem
{
    bool enabled = false;
    bool downTriggered = false;
    int targetLane = 0;
    float laneWidth = 2.0f;
    float horizontalSpeed = 10.0f;
    bool leftPressed = false;
    bool rightPressed = false;

  public:
    void Update(ECS::Registry &registry, float deltaTime) override;

    void SetEnabled(bool enable);

    bool IsEnabled() const;
};

#endif // PROYECTOFINAL_CGA_RUNNERSYSTEM_H
