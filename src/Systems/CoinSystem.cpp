#include "CoinSystem.h"
#include "../Components/CoinComponent.h"
#include "../Components/RunnerComponent.h"
#include "ECS/Components/AudioSource.h"
#include "ECS/Components/Collider.h"
#include <AL/al.h>
#include <GLFW/glfw3.h>

void CoinSystem::Update(ECS::Registry &registry, [[maybe_unused]] float dt)
{
    const auto playerView = registry.View<RunnerComponent>();
    if (playerView.empty())
    {
        return;
    }
    const ECS::Entity player = playerView.front();
    auto &playerCollider = registry.GetComponent<ECS::Components::AABBCollider>(player);

    for (auto entity : registry.View<CoinComponent, ECS::Components::Transform>())
    {
        auto &transform = registry.GetComponent<ECS::Components::Transform>(entity);
        transform.rotation = glm::quat_cast(glm::rotate(glm::mat4(1.0f), glm::radians(static_cast<float>(glfwGetTime()) * 250.0f), {0, 1, 0}));

        if (playerCollider.isColliding && (std::ranges::find(playerCollider.collidingEntities, entity) != playerCollider.collidingEntities.end()))
        {
            auto &runner = registry.GetComponent<RunnerComponent>(player);
            auto &[value] = registry.GetComponent<CoinComponent>(entity);
            runner.score += value;

            if (registry.HasComponent<ECS::Components::AudioSource>(player))
            {
                auto &audioSource = registry.GetComponent<ECS::Components::AudioSource>(player);
                audioSource.filePath =
#if defined(DEBUG) || defined(USE_DEBUG_ASSETS)
                    "."
#endif
                    "./sounds/coin.wav";
                audioSource.isDirty = true;
            }

            registry.DestroyEntity(entity);
        }
    }
}
