/**
 * @file MapLoader.cpp
 * @brief Unified map loader implementation.
 *
 * Orchestrates the 7-step map loading sequence, delegating entity creation
 * to PrefabFactory and geometry loading to AssimpLoader / MapPointsLoader /
 * EnemySpawnLoader. All coordinate transforms (scale, Y-offset, winding fix)
 * are applied here before passing to PrefabFactory.
 */
#include "MapLoader.h"

#include "Assets.h"
#include "Core/Bridge/AssetManager.h"
#include "Core/Bridge/AssimpLoader.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Utils/MapPointsLoader.h"
#include "Game/Utils/EnemySpawnLoader.h"
#include "Game/Utils/ItemSpawnLoader.h"
#include "Game/Components/C_D_PatrolRoute.h"
#include "Game/Utils/Log.h"

#include <algorithm>
#include <cstring>

using namespace NCL::Maths;

namespace ECS {

MapLoadResult LoadMap(Registry& reg, const MapLoadConfig& config, MeshHandle cubeMesh)
{
    MapLoadResult result;
    const float scale = config.mapScale;
    const float worldY = config.yOffset * scale;
    const Vector3 worldOffset(0.0f, worldY, 0.0f);

    // ── Step 1: Load render mesh (*.obj — correct face normals) ─────────
    MeshHandle renderMesh = AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + config.renderMesh);
    LOG_INFO("[MapLoader] render mesh '" << config.renderMesh << "' handle=" << renderMesh);

    // ── Step 2: Load collision geometry (*_collision.obj) ───────────────
    std::string collPath = NCL::Assets::MESHDIR + config.collisionMesh;
    std::vector<Vector3> collVerts;
    std::vector<int>     collIndices;
    bool collLoaded = AssimpLoader::LoadCollisionGeometry(collPath, collVerts, collIndices);

    if (!collLoaded || collVerts.empty()) {
        collPath = NCL::Assets::MESHDIR + config.renderMesh;
        collLoaded = AssimpLoader::LoadCollisionGeometry(collPath, collVerts, collIndices);
        LOG_WARN("[MapLoader] _collision.obj not found, falling back to " << config.renderMesh);
    }

    // ── Step 3: Scale vertices + flip winding ──────────────────────────
    if (collLoaded && !collVerts.empty()) {
        for (auto& v : collVerts) {
            v.x *= scale;
            v.y *= scale;
            v.z *= scale;
        }
        if (config.flipWinding) {
            for (size_t i = 0; i + 2 < collIndices.size(); i += 3) {
                std::swap(collIndices[i + 1], collIndices[i + 2]);
            }
        }
        LOG_INFO("[MapLoader] collision geometry: " << collVerts.size() << " verts, "
                 << collIndices.size() / 3 << " tris");
    } else {
        LOG_WARN("[MapLoader] Failed to load any collision geometry!");
    }

    // ── Step 4: Create map entity (single entity: render + TriMesh) ────
    result.mapEntity = PrefabFactory::CreateStaticMapEntity(
        reg, renderMesh, collVerts, collIndices, worldOffset, scale);

    // ── Step 5: Finish zone (render + detect) ──────────────────────────
    if (config.finishMesh[0] != '\0') {
        MeshHandle finishMesh = AssetManager::Instance().LoadMesh(
            NCL::Assets::MESHDIR + config.finishMesh);

        std::vector<Vector3> finVerts;
        std::vector<int> finIdx;
        AssimpLoader::LoadCollisionGeometry(
            NCL::Assets::MESHDIR + config.finishMesh, finVerts, finIdx);

        Vector3 objCenter(0, 0, 0);
        if (!finVerts.empty()) {
            for (const auto& v : finVerts) {
                objCenter.x += v.x;
                objCenter.y += v.y;
                objCenter.z += v.z;
            }
            float n = static_cast<float>(finVerts.size());
            objCenter.x /= n;
            objCenter.y /= n;
            objCenter.z /= n;
        }

        Vector3 detectPos(
            objCenter.x * scale,
            objCenter.y * scale + worldY,
            objCenter.z * scale);

        if (finishMesh != 0) {
            result.finishRender = PrefabFactory::CreateFinishZoneRender(
                reg, finishMesh, worldOffset, scale);
        }

        result.finishDetect = PrefabFactory::CreateFinishZoneDetect(reg, detectPos);

        LOG_INFO("[MapLoader] Finish zone: detect at ("
                 << detectPos.x << "," << detectPos.y << "," << detectPos.z << ")");
    }

    // ── Step 6: Player spawn from .startpoints ─────────────────────────
    if (config.startPoints[0] != '\0') {
        auto points = LoadMapPoints(NCL::Assets::MESHDIR + config.startPoints);
        if (!points.loaded) {
            std::string fallback = std::string(config.startPoints);
            auto dotPos = fallback.rfind('.');
            if (dotPos != std::string::npos) {
                fallback = fallback.substr(0, dotPos) + ".points";
                points = LoadMapPoints(NCL::Assets::MESHDIR + fallback);
            }
        }
        if (points.loaded && !points.startPoints.empty()) {
            const auto& sp = points.startPoints[0];
            Vector3 spawnPos(
                sp.x * scale,
                sp.y * scale + worldY + 1.5f,
                sp.z * scale);
            result.playerEntity = PrefabFactory::CreatePlayer(reg, cubeMesh, spawnPos);
            LOG_INFO("[MapLoader] Player spawned at ("
                     << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");
        }
    }

    // ── Step 7: Enemy spawns from .enemyspawns ─────────────────────────
    if (config.enemySpawns[0] != '\0') {
        auto enemyData = LoadEnemySpawns(NCL::Assets::MESHDIR + config.enemySpawns);
        if (enemyData.loaded) {
            for (int i = 0; i < static_cast<int>(enemyData.spawns.size()); ++i) {
                const auto& spawn = enemyData.spawns[i];

                Vector3 enemyPos(
                    spawn.position.x * scale,
                    spawn.position.y * scale + worldY + 1.5f,
                    spawn.position.z * scale);

                EntityID enemy = PrefabFactory::CreateNavEnemy(reg, cubeMesh, i, enemyPos);
                result.enemies.push_back(enemy);

                if (spawn.patrolPoints.size() >= 2) {
                    int count = std::min(
                        static_cast<int>(spawn.patrolPoints.size()),
                        PATROL_MAX_WAYPOINTS);
                    Vector3 waypoints[PATROL_MAX_WAYPOINTS];
                    for (int p = 0; p < count; ++p) {
                        const auto& pt = spawn.patrolPoints[p];
                        waypoints[p] = Vector3(
                            pt.x * scale,
                            pt.y * scale + worldY + 1.5f,
                            pt.z * scale);
                    }
                    PrefabFactory::AttachPatrolRoute(reg, enemy, waypoints, count, enemyPos);
                }
            }
            LOG_INFO("[MapLoader] Spawned " << enemyData.spawns.size()
                     << " enemies with patrol routes.");
        }
    }

    // ── Step 8: Item spawns from .itemspawns ───────────────────
    if (config.itemSpawns[0] != '\0') {
        auto itemData = LoadItemSpawns(NCL::Assets::MESHDIR + config.itemSpawns);
        if (itemData.loaded) {
            for (int i = 0; i < static_cast<int>(itemData.spawns.size()); ++i) {
                const auto& spawn = itemData.spawns[i];
                Vector3 itemPos(
                    spawn.position.x * scale,
                    spawn.position.y * scale + worldY + 1.5f,
                    spawn.position.z * scale);
                EntityID pickup = PrefabFactory::CreateItemPickup(
                    reg, cubeMesh, spawn.itemId, spawn.quantity, i, itemPos);
                result.itemPickups.push_back(pickup);
            }
            LOG_INFO("[MapLoader] Spawned " << itemData.spawns.size() << " item pickups.");
        } else {
            LOG_WARN("[MapLoader] Failed to load item spawns: " << config.itemSpawns);
        }
    }

    // Store navmesh path for scene to configure pathfinder
    result.navmeshPath = NCL::Assets::MESHDIR + config.navmesh;

    return result;
}

} // namespace ECS
