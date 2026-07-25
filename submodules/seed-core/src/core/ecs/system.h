#pragma once

namespace seed::ecs {

class World;

class System {
public:
    virtual ~System() = default;
    virtual void onInit(World* /*world*/) {}
    virtual void onUpdate(World* world, float deltaTime) = 0;
    virtual void onShutdown(World* /*world*/) {}
    virtual int priority() const { return 0; }
};

} // namespace seed::ecs
