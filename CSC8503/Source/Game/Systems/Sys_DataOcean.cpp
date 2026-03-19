/**
 * @file Sys_DataOcean.cpp
 * @brief 数据海洋系统实现：递归细分收集参数 + 分帧创建 + 噪波动画。
 *
 * @details
 * OnAwake：递归细分生成所有柱子的 RuntimeOverrides，预缓存组件函数指针，
 *          存入 Res_DataOcean 的 pending 状态。
 * OnUpdate：
 *   - spawning 阶段：每帧从 pendingSpawns 消费 spawnBatchSize 个实体，
 *     使用 CreateFromCache 零查表创建，创建后设置 pillar 参数。
 *   - 正常阶段：遍历所有 DataOceanPillar 实体，噪波更新 Y 位置。
 */
#include "Sys_DataOcean.h"

#include "Assets.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_Material.h"
#include "Game/Components/C_D_DataOceanPillar.h"
#include "Game/Components/C_D_DebugName.h"
#include "Game/Components/Res_DataOcean.h"
#include "Game/Components/Res_Time.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Utils/NoiseUtil.h"
#include "Game/Utils/Log.h"
#include "Core/Bridge/AssetManager.h"

#include <cstdint>
#include <cstdlib>
#include <cstdio>

using namespace NCL::Maths;

namespace ECS {

namespace {

/**
 * @brief 整数坐标哈希，返回 [0, 1) 范围的伪随机浮点数。
 *
 * 用于递归细分时确定性地决定是否继续细分，
 * 保证同一坐标+深度始终产生相同结果。
 */
inline float HashNorm(int x, int z, int depth) {
    int n = x * 73856093 ^ z * 19349663 ^ depth * 83492791;
    n = (n << 13) ^ n;
    n = n * (n * n * 15731 + 789221) + 1376312589;
    return static_cast<float>(n & 0x7fffffff) / 2147483648.0f;
}

/**
 * @brief 递归细分并收集柱子生成参数（不创建实体）。
 *
 * 将 (x, z, size) 定义的正方形区域递归四分，
 * 叶节点将 RuntimeOverrides 追加到 outSpawns。
 *
 * @param cfg        海洋配置
 * @param cubeMesh   共享的 cube mesh handle
 * @param x          区域左下角 X
 * @param z          区域左下角 Z
 * @param size       区域边长
 * @param depth      当前递归深度
 * @param count      已收集计数（引用，用于 spawnIndex）
 * @param outSpawns  输出：收集的 RuntimeOverrides 列表
 */
void SubdivideAndCollect(const Res_DataOcean& cfg, MeshHandle cubeMesh,
                         float x, float z, float size, int depth, int& count,
                         std::vector<RuntimeOverrides>& outSpawns) {
    float centerX = x + size * 0.5f;
    float centerZ = z + size * 0.5f;

    float centerGapSq = cfg.centerGap * cfg.centerGap;
    if ((centerX * centerX + centerZ * centerZ) < centerGapSq)
        return;

    float minSize = cfg.minSize;

    int ix = static_cast<int>(x * 1000.0f);
    int iz = static_cast<int>(z * 1000.0f);
    float h = HashNorm(ix, iz, depth);

    float splitChance = cfg.splitChanceBase * (1.0f - static_cast<float>(depth) / static_cast<float>(cfg.maxDepth));

    if (size > minSize && depth < cfg.maxDepth && h < splitChance) {
        float half = size * 0.5f;
        SubdivideAndCollect(cfg, cubeMesh, x,        z,        half, depth + 1, count, outSpawns);
        SubdivideAndCollect(cfg, cubeMesh, x + half, z,        half, depth + 1, count, outSpawns);
        SubdivideAndCollect(cfg, cubeMesh, x,        z + half, half, depth + 1, count, outSpawns);
        SubdivideAndCollect(cfg, cubeMesh, x + half, z + half, half, depth + 1, count, outSpawns);
        return;
    }

    float scaleXZ = size / 2.0f;
    float randY = cfg.pillarMinScaleY +
        static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) *
        (cfg.pillarMaxScaleY - cfg.pillarMinScaleY);

    RuntimeOverrides ovr;
    ovr.position   = Vector3(centerX, cfg.yOffset, centerZ);
    ovr.scale      = Vector3(scaleXZ, randY, scaleXZ);
    ovr.meshHandle = cubeMesh;
    ovr.spawnIndex = count;

    outSpawns.push_back(std::move(ovr));
    ++count;
}

} // anonymous namespace

/**
 * @brief 递归细分收集所有柱子参数，预缓存组件函数指针，启动分帧创建。
 *
 * 1. 读取 Res_DataOcean 配置
 * 2. 递归细分 → 收集所有 RuntimeOverrides 到 pendingSpawns
 * 3. ResolveBlueprintCache 缓存函数指针
 * 4. 预 reserve 组件池
 * 5. 设置 spawning=true，OnUpdate 开始分帧消费
 */
void Sys_DataOcean::OnAwake(Registry& registry) {
    if (!registry.has_ctx<Res_DataOcean>()) {
        LOG_INFO("[Sys_DataOcean] OnAwake - Res_DataOcean 未注册，跳过生成。");
        return;
    }

    auto& cfg = registry.ctx<Res_DataOcean>();
    MeshHandle cubeMesh = AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");

    int gridCount = static_cast<int>(cfg.areaExtent * 2.0f / cfg.baseSize);
    int estimatedCount = gridCount * gridCount * 2;
    cfg.pendingSpawns.reserve(estimatedCount);

    int count = 0;
    for (float gx = -cfg.areaExtent; gx < cfg.areaExtent; gx += cfg.baseSize) {
        for (float gz = -cfg.areaExtent; gz < cfg.areaExtent; gz += cfg.baseSize) {
            SubdivideAndCollect(cfg, cubeMesh, gx, gz, cfg.baseSize, 0, count, cfg.pendingSpawns);
        }
    }

    if (!PrefabFactory::ResolveBlueprintCache("Prefab_DataOceanPillar.json", cfg.cachedEmplace)) {
        LOG_WARN("[Sys_DataOcean] OnAwake - ResolveBlueprintCache 失败，回退到逐个创建。");
        cfg.pendingSpawns.clear();
        return;
    }

    registry.reserve<C_D_Transform>(static_cast<int>(cfg.pendingSpawns.size()));
    registry.reserve<C_D_MeshRenderer>(static_cast<int>(cfg.pendingSpawns.size()));
    registry.reserve<C_D_Material>(static_cast<int>(cfg.pendingSpawns.size()));
    registry.reserve<C_D_DataOceanPillar>(static_cast<int>(cfg.pendingSpawns.size()));
    registry.reserve<C_D_DebugName>(static_cast<int>(cfg.pendingSpawns.size()));

    cfg.spawnCursor = 0;
    cfg.spawning    = true;

    LOG_INFO("[Sys_DataOcean] OnAwake - 收集 " << cfg.pendingSpawns.size()
             << " 个柱子参数，开始分帧创建（batchSize=" << cfg.spawnBatchSize << "）。");
}

/**
 * @brief 分帧创建或噪波动画更新。
 *
 * spawning 阶段：每帧从 pendingSpawns[spawnCursor] 开始创建 spawnBatchSize 个实体，
 * 使用 CreateFromCache 零查表创建，创建后设置 pillar 的 baseY/sizeXZ/phaseShift。
 * 全部完成后清除 pending 状态，释放内存。
 *
 * 正常阶段：遍历所有 DataOceanPillar 实体，噪波采样更新 Y 位置。
 */
void Sys_DataOcean::OnUpdate(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_DataOcean>())
        return;

    auto& cfg = registry.ctx<Res_DataOcean>();

    if (cfg.spawning) {
        int total = static_cast<int>(cfg.pendingSpawns.size());
        int end   = std::min(cfg.spawnCursor + cfg.spawnBatchSize, total);

        for (int i = cfg.spawnCursor; i < end; ++i) {
            const auto& ovr = cfg.pendingSpawns[i];
            EntityID entity = PrefabFactory::CreateFromCache(registry, cfg.cachedEmplace, ovr);

            if (entity != Entity::NULL_ENTITY && registry.Has<C_D_DataOceanPillar>(entity)) {
                auto& pillar      = registry.Get<C_D_DataOceanPillar>(entity);
                pillar.baseY      = cfg.yOffset;
                pillar.sizeXZ     = ovr.scale->x * 2.0f;
                int ix = static_cast<int>(ovr.position->x * 1000.0f);
                int iz = static_cast<int>(ovr.position->z * 1000.0f);
                int hashVal       = ix * 73856093 ^ iz * 19349663;
                pillar.phaseShift = static_cast<float>(hashVal & 0xFFFF) / 65535.0f * 6.2831853f;
            }
        }

        cfg.spawnCursor = end;

        if (cfg.spawnCursor >= total) {
            LOG_INFO("[Sys_DataOcean] 分帧创建完成，共 " << total << " 个柱子。");
            cfg.spawning = false;
            cfg.pendingSpawns.clear();
            cfg.pendingSpawns.shrink_to_fit();
            cfg.cachedEmplace.clear();
            cfg.cachedEmplace.shrink_to_fit();
        }
        return;
    }

    if (!registry.has_ctx<Res_Time>())
        return;

    cfg.noiseFrameCounter++;
    if (cfg.noiseFrameCounter < cfg.noiseUpdateInterval)
        return;
    cfg.noiseFrameCounter = 0;

    const auto& time = registry.ctx<Res_Time>();

    registry.view<C_D_DataOceanPillar, C_D_Transform>().each(
        [&](EntityID /*id*/, C_D_DataOceanPillar& pillar, C_D_Transform& tf) {
            float n = NoiseUtil::Noise3D(
                tf.position.x * cfg.noiseScale,
                tf.position.z * cfg.noiseScale,
                time.totalTime * cfg.noiseSpeed + pillar.phaseShift
            );
            tf.position.y = pillar.baseY + n * pillar.amplitude * cfg.baseAmplitude;
        }
    );
}

/**
 * @brief 无需特殊清理，柱子实体由 Registry::Clear() 统一回收。
 */
void Sys_DataOcean::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_DataOcean] OnDestroy");
}

} // namespace ECS
