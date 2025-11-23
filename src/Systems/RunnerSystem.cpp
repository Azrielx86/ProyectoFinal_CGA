

#include "RunnerSystem.h"

#include "../Components/FloorComponent.h"
#include "../Components/RunnerComponent.h"
#include "ECS/Components/Collider.h"
#include "ECS/Components/Transform.h"
#include "Input/Keyboard.h"

#include <GLFW/glfw3.h>

void RunnerSystem::Update(ECS::Registry &registry, float deltaTime)
{
    const auto kb = Input::Keyboard::GetInstance();
    const std::vector<ECS::Entity> entities = registry.View<RunnerComponent, ECS::Components::Transform>();

    if (entities.empty())
    {
        std::cerr << "\033[31mNo player entities found!\033[0m\n";
        return;
    }

    if (entities.size() > 1)
        std::cout << "\033[33mMore than one player found, taking the first and ignoring others...\033[0m\n";

    const ECS::Entity player = entities[0];

    auto &transform = registry.GetComponent<ECS::Components::Transform>(player);
    auto &collider = registry.GetComponent<ECS::Components::AABBCollider>(player);
    auto &runner = registry.GetComponent<RunnerComponent>(player);

    runner.grounded = false;
    if (collider.isColliding)
    {
        const auto playerAABB = collider.GetWorldAABB(transform);

        for (const auto &collidingEntity : collider.collidingEntities)
        {
            if (!(registry.HasComponent<ECS::Components::AABBCollider>(collidingEntity) && registry.HasComponent<FloorComponent>(collidingEntity)))
                continue;

            auto &platformTransform = registry.GetComponent<ECS::Components::Transform>(collidingEntity);
            auto &platformCollider = registry.GetComponent<ECS::Components::AABBCollider>(collidingEntity);
            const auto platformAABB = platformCollider.GetWorldAABB(platformTransform);

            runner.grounded = true;
            downTriggered = false;
        }
    }

    if (runner.grounded)
    {
        runner.velocity.y = 0;
        if (kb->GetKeyPress(GLFW_KEY_SPACE))
            runner.velocity.y = runner.jumpForce;
    }
    else
    {
        runner.velocity.y += gravity * runner.weight * deltaTime;

        if (!downTriggered && kb->GetKeyPress(GLFW_KEY_DOWN))
        {
            runner.velocity.y += -5.0f;
            downTriggered = true;
        }
    }

    transform.translation += runner.velocity * deltaTime;

    const bool leftDown = kb->GetKeyPress(GLFW_KEY_LEFT);
    if (leftDown && !leftPressed)
    {
        targetLane--;
    }
    leftPressed = leftDown;

    const bool rightDown = kb->GetKeyPress(GLFW_KEY_RIGHT);
    if (rightDown && !rightPressed)
    {
        targetLane++;
    }
    rightPressed = rightDown;

    if (targetLane < -1) targetLane = -1;
    if (targetLane > 1) targetLane = 1;

    const float targetZ = static_cast<float>(targetLane) * laneWidth;

    float newZ = transform.translation.z + (targetZ - transform.translation.z) * horizontalSpeed * deltaTime;

    if (std::abs(targetZ - newZ) < 0.01f)
        newZ = targetZ;

    transform.translation.z = newZ;
}