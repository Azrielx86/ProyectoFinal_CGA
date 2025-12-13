//
// Created by tohka on 12/12/25.

#ifndef PROYECTOFINAL_CGA_BUILDCOMPONENT_H
#define PROYECTOFINAL_CGA_BUILDCOMPONENT_H

/**
 * @brief Component to declare relevant points to
 * generate and destroy buildings in the game.
 */
struct BuildingComponent
{
    /**
     * @brief The center of the building.
     */
    glm::vec3 center = glm::vec3(0.0f);

    /**
     * @brief The absolute value of the borders of the building.
     * This is relevant to declare where to spawn the next build with
     * border + the center of the next build; and when the border is lesser
     * than the point where the elements are destroyed, the entity with this
     * component will be destroyed.
     */
    glm::vec3 border = glm::vec3(0.0f);
};

#endif // PROYECTOFINAL_CGA_BUILDCOMPONENT_H
