#ifndef COINSYSTEM_H
#define COINSYSTEM_H

#include "ECS/ISystem.h"

class CoinSystem final : public ECS::ISystem {
public:
    void Update(ECS::Registry& registry, float dt) override;
};

#endif //COINSYSTEM_H
