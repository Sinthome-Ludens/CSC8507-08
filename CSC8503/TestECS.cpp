/**
 * @file TestECS.cpp
 * @brief Phase 1 验证测试：Hello ECS（简化版）
 *
 * 由于 LOG_INFO 宏在某些 MSVC 配置下存在 operator<< 兼容性问题，
 * 本测试直接使用 std::cout 输出，确保编译通过。
 */

#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Core/ECS/EventBus.h"
#include "Game/Utils/Assert.h"

#include <iostream>
#include <cmath>

// 测试用组件定义
struct C_D_Position {
    float x = 0.0f;
    float y = 0.0f;
};

struct C_D_Velocity {
    float vx = 0.0f;
    float vy = 0.0f;
};

struct C_D_Health {
    float current = 100.0f;
    float maximum = 100.0f;
};

struct C_D_DebugName {
    char name[32] = "";
};

struct Res_Time {
    float deltaTime = 0.0f;
    float totalTime = 0.0f;
};

struct Evt_Test_Collision {
    ECS::EntityID entityA;
    ECS::EntityID entityB;
};

// 测试用 System
class Sys_Movement : public ECS::ISystem {
public:
    void OnAwake(ECS::Registry& reg) override {
        std::cout << "[INFO] Sys_Movement::OnAwake\n";
    }

    void OnUpdate(ECS::Registry& reg, float dt) override {
        auto& res_time = reg.ctx<Res_Time>();
        res_time.totalTime += dt;
        int count = 0;
        reg.view<C_D_Position, C_D_Velocity>().each(
            [dt, &count](ECS::EntityID, C_D_Position& pos, C_D_Velocity& vel) {
                pos.x += vel.vx * dt;
                pos.y += vel.vy * dt;
                ++count;
            }
        );
        if (count > 0) {
            std::cout << "[INFO] Sys_Movement updated " << count << " entities\n";
        }
    }

    void OnDestroy(ECS::Registry& reg) override {
        std::cout << "[INFO] Sys_Movement::OnDestroy\n";
    }
};

void TestECS_Basic() {
    std::cout << "\n========== Test 1: Registry & Component ==========\n";
    ECS::Registry registry;

    auto e0 = registry.Create();
    auto e1 = registry.Create();
    auto e2 = registry.Create();

    registry.Emplace<C_D_Position>(e0, 0.0f, 0.0f);
    registry.Emplace<C_D_Velocity>(e0, 1.0f, 0.5f);

    registry.Emplace<C_D_Position>(e1, 10.0f, 10.0f);

    registry.Emplace<C_D_Position>(e2, 5.0f, 5.0f);
    registry.Emplace<C_D_Velocity>(e2, -0.5f, 1.0f);
    registry.Emplace<C_D_Health>(e2, 100.0f, 100.0f);

    GAME_ASSERT(registry.Has<C_D_Position>(e0), "Entity0 should have Position");
    GAME_ASSERT(registry.Has<C_D_Velocity>(e0), "Entity0 should have Velocity");
    GAME_ASSERT(!registry.Has<C_D_Health>(e0), "Entity0 should NOT have Health");

    int iterCount = 0;
    for (auto&& [id, pos, vel] : registry.view<C_D_Position, C_D_Velocity>()) {
        ++iterCount;
    }
    GAME_ASSERT(iterCount == 2, "Should iterate exactly 2 entities");

    std::cout << "[PASS] Basic Registry & Component test\n";
}

void TestECS_Systems() {
    std::cout << "\n========== Test 2: System & SystemManager ==========\n";
    ECS::Registry registry;
    ECS::SystemManager systems;

    systems.Register<Sys_Movement>(100);

    auto e0 = registry.Create();
    registry.Emplace<C_D_Position>(e0, 0.0f, 0.0f);
    registry.Emplace<C_D_Velocity>(e0, 2.0f, 1.0f);

    auto& res_time = registry.ctx<Res_Time>();
    res_time.deltaTime = 0.016f;

    systems.AwakeAll(registry);

    for (int i = 0; i < 5; ++i) {
        systems.UpdateAll(registry, 0.016f);
    }

    auto& pos = registry.Get<C_D_Position>(e0);
    float expectedX = 2.0f * 0.016f * 5.0f;
    float expectedY = 1.0f * 0.016f * 5.0f;

    GAME_ASSERT(std::abs(pos.x - expectedX) < 0.001f, "Position X should match");
    GAME_ASSERT(std::abs(pos.y - expectedY) < 0.001f, "Position Y should match");

    systems.DestroyAll(registry);

    std::cout << "[PASS] System & SystemManager test\n";
}

void TestECS_Events() {
    std::cout << "\n========== Test 3: EventBus ==========\n";
    ECS::EventBus bus;

    int callbackCount = 0;
    auto subID = bus.subscribe<Evt_Test_Collision>([&callbackCount](const Evt_Test_Collision& evt) {
        callbackCount++;
    });

    bus.publish(Evt_Test_Collision{ 10, 20 });
    GAME_ASSERT(callbackCount == 1, "Immediate publish should trigger callback");

    bus.publish_deferred(Evt_Test_Collision{ 30, 40 });
    GAME_ASSERT(callbackCount == 1, "Deferred publish should NOT trigger callback yet");

    bus.flush();
    GAME_ASSERT(callbackCount == 2, "Flush should trigger deferred callback");

    bus.unsubscribe<Evt_Test_Collision>(subID);
    bus.publish(Evt_Test_Collision{ 50, 60 });
    GAME_ASSERT(callbackCount == 2, "After unsubscribe, no new callbacks");

    std::cout << "[PASS] EventBus test\n";
}

void TestECS_LifeCycle() {
    std::cout << "\n========== Test 4: Entity Lifecycle ==========\n";
    ECS::Registry registry;

    auto e0 = registry.Create();
    auto e1 = registry.Create();

    registry.Emplace<C_D_Position>(e0);
    registry.Emplace<C_D_Position>(e1);

    GAME_ASSERT(registry.EntityCount() == 2, "Should have 2 entities");

    registry.Destroy(e0);
    GAME_ASSERT(registry.Valid(e0), "Entity should still be valid before ProcessPendingDestroy");

    registry.ProcessPendingDestroy();
    GAME_ASSERT(!registry.Valid(e0), "Entity should be invalid after destruction");
    GAME_ASSERT(registry.EntityCount() == 1, "Count should be 1");

    registry.DestroyImmediate(e1);
    GAME_ASSERT(!registry.Valid(e1), "Entity1 should be invalid immediately");
    GAME_ASSERT(registry.EntityCount() == 0, "Count should be 0");

    std::cout << "[PASS] Entity Lifecycle test\n";
}

void RunECSTests() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "   Phase 1 - ECS Core Test Suite       \n";
    std::cout << "========================================\n";

    try {
        TestECS_Basic();
        TestECS_Systems();
        TestECS_Events();
        TestECS_LifeCycle();

        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "   ALL ECS TESTS PASSED               \n";
        std::cout << "========================================\n\n";

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Test failed: " << e.what() << "\n";
        throw;
    }
}
