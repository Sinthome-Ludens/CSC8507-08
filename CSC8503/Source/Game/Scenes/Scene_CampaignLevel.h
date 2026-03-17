/**
 * @file Scene_CampaignLevel.h
 * @brief 战役关卡参数化场景：根据地图索引加载对应地图 + 完整玩法系统。
 */
#pragma once

#include "IScene.h"
#include "Game/Utils/NavMeshPathfinderUtil.h"
#include <memory>

/**
 * @brief 战役关卡（参数化，根据 mapIndex 索引 kCampaignMaps[] 加载对应地图）
 *
 * 合并两个模式：
 *   - 地图加载：复用 Scene_Dock 的 CreateStaticMap + NavMesh 地板 + 边界墙
 *   - 玩法系统：复用 Scene_TutorialLevel 的完整系统列表
 */
class Scene_CampaignLevel : public IScene {
public:
    explicit Scene_CampaignLevel(int mapIndex);
    ~Scene_CampaignLevel() = default;

    void OnEnter(ECS::Registry&          registry,
                 ECS::SystemManager&     systems,
                 const Res_NCL_Pointers& nclPtrs) override;

    void OnExit(ECS::Registry&      registry,
                ECS::SystemManager& systems) override;

    IScene* CreateRestartScene() override { return new Scene_CampaignLevel(m_MapIndex); }

private:
    int m_MapIndex;
    std::unique_ptr<ECS::NavMeshPathfinderUtil> m_Pathfinder;
};
