/**
 * @file C_D_MapConfig.h
 * @brief Map configuration data component: stores all resource paths and parameters needed for map loading.
 */
#pragma once

/**
 * @brief Map configuration data component
 *
 * Stores all resource filenames and physics parameters for a single map.
 * Populated by scene OnEnter and consumed by MapLoader.
 * Filenames are relative to MESHDIR.
 */
struct C_D_MapConfig {
    char renderMesh[64]    = {};   ///< Render OBJ ("HangerA.obj")
    char collisionMesh[64] = {};   ///< Collision OBJ ("HangerA_collision.obj")
    char navmesh[64]       = {};   ///< NavMesh file ("HangerA.navmesh")
    char finishMesh[64]    = {};   ///< Finish zone OBJ ("HangerA_finish.obj")
    char startPoints[64]   = {};   ///< Spawn points file ("HangerA.startpoints")
    char enemySpawns[64]   = {};   ///< Enemy spawns file ("HangerA.enemyspawns")
    float mapScale         = 1.0f; ///< Map scale factor
    float yOffset          = -6.0f;///< Y offset (multiplied by mapScale before use)
    bool  flipWinding      = true; ///< Whether to flip collision mesh winding order
};
