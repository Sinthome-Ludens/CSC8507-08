/**
 * @file MapPointsLoader.h
 * @brief .points 文件解析工具：加载地图起始点和结束点坐标。
 */
#pragma once

#include "Vector.h"
#include <vector>
#include <string>

namespace ECS {

/**
 * @brief .points 文件解析结果
 *
 * 存储从 .points 文件中读取的起始点和结束点坐标（navmesh 局部空间）。
 * 坐标系与 .navmesh 文件相同，使用前需按 kMapScale 缩放并加 Y 偏移。
 */
struct MapPointsData {
    std::vector<NCL::Maths::Vector3> startPoints;   ///< 起始点列表（navmesh 局部坐标）
    std::vector<NCL::Maths::Vector3> finishPoints;   ///< 结束点列表（navmesh 局部坐标）
    bool loaded = false;                              ///< 是否成功加载
};

/**
 * @brief 从 .points 文件加载起始点和结束点
 *
 * 文件格式（Unity 导出）：
 * @code
 * # 注释行
 * startPointCount N
 * finishPointCount M
 *
 * startPoints
 * x y z  × N
 *
 * finishPoints
 * x y z  × M
 * @endcode
 *
 * 也兼容 .startpoints 格式：
 * @code
 * # 注释行
 * count N
 *
 * startPoints
 * x y z  × N
 * @endcode
 *
 * @param filePath .points 或 .startpoints 文件绝对路径
 * @return 解析结果；文件不存在时返回 loaded=false（不报错）
 */
MapPointsData LoadMapPoints(const std::string& filePath);

} // namespace ECS
