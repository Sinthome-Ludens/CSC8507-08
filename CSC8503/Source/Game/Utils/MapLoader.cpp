/**
 * @file MapLoader.cpp
 * @brief Unified map loader implementation.
 *
 * Orchestrates the 8-step map loading sequence, delegating entity creation
 * to PrefabFactory and geometry loading to AssetManager / MapPointsLoader /
 * EnemySpawnLoader / DoorKeyLoader. All coordinate transforms (scale,
 * Y-offset, winding fix) are applied here before passing to PrefabFactory.
 */
#include "MapLoader.h"

#include "Assets.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Utils/MapPointsLoader.h"
#include "Game/Utils/EnemySpawnLoader.h"
#include "Game/Utils/DoorKeyLoader.h"
#include "Game/Utils/ItemSpawnLoader.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/C_D_PatrolRoute.h"
#include "Game/Utils/Log.h"

#include <algorithm>
#include <cstring>
#include <fstream>

using namespace NCL::Maths;

namespace ECS {

/// Check if a file exists on disk (quick open test).
static bool FileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

MapLoadResult LoadMap(Registry& reg, const MapLoadConfig& config, MeshHandle cubeMesh)
{
    MapLoadResult result;
    const float scale = config.mapScale;
    const float worldY = config.yOffset * scale;
    const Vector3 worldOffset(0.0f, worldY, 0.0f);

    // ── Step 1: Load render mesh (GLTF preferred, OBJ fallback) ─────────
    MeshHandle renderMesh = 0;
    bool renderFromGltf = false;

    if (config.renderMeshGltf[0] != '\0') {
        std::string gltfPath = NCL::Assets::MESHDIR + config.renderMeshGltf;
        if (FileExists(gltfPath)) {
            LOG_INFO("[MapLoader] render → AssetManager::LoadMesh('" << gltfPath << "')");
            renderMesh = AssetManager::Instance().LoadMesh(gltfPath);
            renderFromGltf = true;
            LOG_INFO("[MapLoader] render mesh '" << config.renderMeshGltf
                     << "' loaded OK, handle=" << renderMesh);
        } else {
            LOG_WARN("[MapLoader] render mesh '" << config.renderMeshGltf
                     << "' not found at '" << gltfPath << "', falling back to OBJ");
        }
    }
    if (!renderFromGltf) {
        std::string objPath = NCL::Assets::MESHDIR + config.renderMesh;
        LOG_INFO("[MapLoader] render → AssetManager::LoadMesh('" << objPath << "')");
        renderMesh = AssetManager::Instance().LoadMesh(objPath);
        LOG_INFO("[MapLoader] render mesh '" << config.renderMesh
                 << "' loaded OK, handle=" << renderMesh);
    }

    // ── Step 2: Load collision geometry (GLTF preferred, OBJ fallback) ──
    std::string collPath;
    std::vector<Vector3> collVerts;
    std::vector<int>     collIndices;
    bool collLoaded = false;

    // Try GLTF collision mesh first
    if (config.collisionMeshGltf[0] != '\0') {
        collPath = NCL::Assets::MESHDIR + config.collisionMeshGltf;
        if (FileExists(collPath)) {
            LOG_INFO("[MapLoader] collision → AssetManager::LoadCollisionGeometry('" << collPath << "')");
            collLoaded = AssetManager::Instance().LoadCollisionGeometry(collPath, collVerts, collIndices);
            if (collLoaded && !collVerts.empty()) {
                LOG_INFO("[MapLoader] collision mesh '" << config.collisionMeshGltf
                         << "' loaded OK (" << collVerts.size() << " verts, "
                         << collIndices.size() / 3 << " tris)");
            } else {
                LOG_WARN("[MapLoader] collision mesh '" << config.collisionMeshGltf
                         << "' returned no geometry, falling back to OBJ");
                collLoaded = false;
                collVerts.clear();
                collIndices.clear();
            }
        } else {
            LOG_WARN("[MapLoader] collision mesh '" << config.collisionMeshGltf
                     << "' not found at '" << collPath << "', falling back to OBJ");
        }
    }

    // Fallback: OBJ collision mesh
    if (!collLoaded) {
        collPath = NCL::Assets::MESHDIR + config.collisionMesh;
        LOG_INFO("[MapLoader] collision → AssetManager::LoadCollisionGeometry('" << collPath << "')");
        collLoaded = AssetManager::Instance().LoadCollisionGeometry(collPath, collVerts, collIndices);
        if (collLoaded && !collVerts.empty()) {
            LOG_INFO("[MapLoader] collision mesh '" << config.collisionMesh
                     << "' loaded OK (" << collVerts.size() << " verts, "
                     << collIndices.size() / 3 << " tris)");
        } else {
            // Last resort: use render mesh for collision
            collPath = NCL::Assets::MESHDIR + config.renderMesh;
            LOG_WARN("[MapLoader] collision mesh '" << config.collisionMesh
                     << "' failed, last resort → render mesh '" << config.renderMesh << "'");
            collLoaded = AssetManager::Instance().LoadCollisionGeometry(collPath, collVerts, collIndices);
        }
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

    // ── Step 5: Finish zone (GLTF preferred, OBJ fallback) ─────────────
    if (config.finishMesh[0] != '\0' || config.finishMeshGltf[0] != '\0') {
        MeshHandle finishMesh = 0;
        std::string finishGeomPath;
        bool finishFromGltf = false;

        // Try GLTF first for finish mesh
        if (config.finishMeshGltf[0] != '\0') {
            std::string gltfPath = NCL::Assets::MESHDIR + config.finishMeshGltf;
            if (FileExists(gltfPath)) {
                LOG_INFO("[MapLoader] finish render → AssetManager::LoadMesh('" << gltfPath << "')");
                finishMesh = AssetManager::Instance().LoadMesh(gltfPath);
                finishGeomPath = gltfPath;
                finishFromGltf = true;
                LOG_INFO("[MapLoader] finish mesh '" << config.finishMeshGltf
                         << "' loaded OK, handle=" << finishMesh);
            } else {
                LOG_WARN("[MapLoader] finish mesh '" << config.finishMeshGltf
                         << "' not found at '" << gltfPath << "', falling back to OBJ");
            }
        }

        // Fallback to OBJ
        if (!finishFromGltf && config.finishMesh[0] != '\0') {
            std::string objPath = NCL::Assets::MESHDIR + config.finishMesh;
            LOG_INFO("[MapLoader] finish render → AssetManager::LoadMesh('" << objPath << "')");
            finishMesh = AssetManager::Instance().LoadMesh(objPath);
            finishGeomPath = objPath;
            LOG_INFO("[MapLoader] finish mesh '" << config.finishMesh
                     << "' loaded OK, handle=" << finishMesh);
        }

        // Load collision geometry for detect position (unified through AssetManager)
        std::vector<Vector3> finVerts;
        std::vector<int> finIdx;
        if (!finishGeomPath.empty()) {
            LOG_INFO("[MapLoader] finish collision → AssetManager::LoadCollisionGeometry('" << finishGeomPath << "')");
            bool ok = AssetManager::Instance().LoadCollisionGeometry(finishGeomPath, finVerts, finIdx);
            if (ok) {
                LOG_INFO("[MapLoader] finish collision loaded OK ("
                         << finVerts.size() << " verts, " << finIdx.size() / 3 << " tris)");
            } else {
                LOG_WARN("[MapLoader] finish collision '" << finishGeomPath
                         << "' failed, no collision geometry for finish zone");
            }
        }

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

    // ── Step 8: Doors & keys from .doors ────────────────────────────
    if (config.doorKeys[0] != '\0') {
        auto doorData = LoadDoorKeys(NCL::Assets::MESHDIR + config.doorKeys);
        if (doorData.loaded) {
            for (int i = 0; i < static_cast<int>(doorData.doors.size()); ++i) {
                const auto& d = doorData.doors[i];
                Vector3 doorPos(
                    d.position.x * scale,
                    d.position.y * scale + worldY,
                    d.position.z * scale);
                Vector3 halfExtents(
                    d.scale.x * 0.5f * scale,
                    d.scale.y * 0.5f * scale,
                    d.scale.z * 0.5f * scale);
                PrefabFactory::CreateLockedDoor(reg, cubeMesh, d.keyId, doorPos, halfExtents);
            }
            for (int i = 0; i < static_cast<int>(doorData.keys.size()); ++i) {
                const auto& k = doorData.keys[i];
                Vector3 keyPos(
                    k.position.x * scale,
                    k.position.y * scale + worldY,
                    k.position.z * scale);
                PrefabFactory::CreateKeyCard(reg, cubeMesh, k.keyId, keyPos);
            }
            LOG_INFO("[MapLoader] Loaded " << doorData.doors.size() << " doors, "
                     << doorData.keys.size() << " keys.");
        } else {
            LOG_WARN("[MapLoader] .doors file configured but failed to load: " << config.doorKeys);
        }
    }

    // ── Step 9: Item spawns from .itemspawns ───────────────────
    if (config.itemSpawns[0] != '\0') {
        auto itemData = LoadItemSpawns(NCL::Assets::MESHDIR + config.itemSpawns);
        if (itemData.loaded) {
            for (int i = 0; i < static_cast<int>(itemData.spawns.size()); ++i) {
                const auto& spawn = itemData.spawns[i];

                // Skip already-unlocked weapons (no need to spawn on map)
                bool isWeapon = (spawn.itemId == ItemID::RoamAI || spawn.itemId == ItemID::TargetStrike);
                if (isWeapon && reg.has_ctx<Res_UIState>()) {
                    if (reg.ctx<Res_UIState>().savedUnlocked[static_cast<int>(spawn.itemId)]) {
                        continue;
                    }
                }

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
