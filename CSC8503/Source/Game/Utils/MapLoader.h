/**
 * @file MapLoader.h
 * @brief Unified map loader: creates map entity, finish zone, player and enemies from MapLoadConfig.
 */
#pragma once

#include "Core/ECS/Registry.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Components/MapLoadConfig.h"
#include "Vector.h"
#include <vector>
#include <string>

namespace ECS {

/**
 * @brief Result of a complete map load operation.
 *
 * Contains all entity IDs created during map loading, plus the navmesh path
 * for the scene to configure its NavMeshPathfinderUtil.
 */
struct MapLoadResult {
    EntityID              mapEntity    = Entity::NULL_ENTITY;
    EntityID              finishRender = Entity::NULL_ENTITY;
    EntityID              finishDetect = Entity::NULL_ENTITY;
    EntityID              playerEntity = Entity::NULL_ENTITY;
    std::vector<EntityID> enemies;
    std::string           navmeshPath;
};

/**
 * @brief Load a complete map from a MapLoadConfig.
 *
 * Performs the 7-step loading sequence:
 *  1. Load render mesh (*.obj)
 *  2. Load collision geometry (*_collision.obj, fallback to render mesh)
 *  3. Scale vertices + flip winding
 *  4. Create map entity (PrefabFactory::CreateStaticMapEntity)
 *  5. Create finish zone (render + detect)
 *  6. Create player from .startpoints
 *  7. Create enemies from .enemyspawns with patrol routes
 *
 * @param reg       ECS Registry
 * @param config    Map configuration
 * @param cubeMesh  Cube mesh handle (for player and enemy rendering)
 * @return MapLoadResult with all created entity IDs
 */
MapLoadResult LoadMap(Registry& reg, const MapLoadConfig& config, MeshHandle cubeMesh);

} // namespace ECS
