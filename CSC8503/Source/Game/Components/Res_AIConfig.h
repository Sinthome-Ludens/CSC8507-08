/**
 * @file Res_AIConfig.h
 * @brief AI 系统全局配置资源：Sys_EnemyAI 的数据驱动参数。
 *
 * 消除 Sys_EnemyAI 中的硬编码魔数，统一由场景 ctx_emplace 注册。
 * 可在运行时调节以平衡 AI 行为，无需修改系统代码。
 *
 * 使用方式：
 *   - 场景 OnEnter 中 registry.ctx_emplace<Res_AIConfig>(Res_AIConfig{});
 *   - 场景 OnExit 中 registry.ctx_erase<Res_AIConfig>();
 *   - Sys_EnemyAI::OnUpdate 中 registry.ctx<Res_AIConfig>() 读取
 */
#pragma once

namespace ECS {

struct Res_AIConfig {
    float contact_distance       = 1.5f;
    float hunt_lock_duration     = 1.5f;
    float global_alert_increment = 15.0f;
    float hysteresis_band        = 5.0f;
    float noise_hearing_range    = 15.0f;
    float noise_boost_factor     = 1.0f;
    float ally_alert_range       = 15.0f;
    float ally_alert_boost       = 20.0f;
};

} // namespace ECS
