#include "Sys_Interpolation.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_NetworkIdentity.h"
#include "Game/Components/C_D_InterpBuffer.h"
#include "Game/Components/Res_Network.h"
#include "Game/Components/Res_Time.h"
#include "Maths.h"
#include <algorithm>

namespace ECS {

void Sys_Interpolation::OnUpdate(Registry& reg, float dt) {
    // 1. 基础检查：确保网络组件和时间资源存在，否则无法进行插值计算
    if (!reg.has_ctx<Res_Network>() || !reg.has_ctx<Res_Time>()) return;

    auto& resNet = reg.ctx<Res_Network>();
    auto& resTime = reg.ctx<Res_Time>();

    // 我们不渲染远程实体的“最新”位置，而是回溯到 (当前时间 - 延迟) 的时刻
    // 目标回放时刻 = 当前总时间 - 渲染延迟
    float targetTime = (resTime.totalTime - m_RenderDelay) * 1000.0f; // 转为毫秒

    // 遍历所有具有 变换、网络身份、插值缓冲区 的实体
    reg.view<C_D_Transform, C_D_NetworkIdentity, C_D_InterpBuffer>().each([&](
        EntityID entity, C_D_Transform& tf, C_D_NetworkIdentity& net, C_D_InterpBuffer& buffer) 
    {
        // 只插值远程实体
        if (net.ownerClientID == resNet.localClientID) return;
        if (buffer.count < 2) return;
        // TODO: 环形缓冲区顺序处理优化
        // WARNING: 当前实现假设快照是按时间戳(Timestamp)顺序连续存入缓冲区的。
        // 如果网络层收到乱序包（例如先收到了 T+2，后收到了 T+1），
        // 这里的线性索引(curr + 1)会导致插值逻辑崩溃或画面闪烁。
        // 建议：在存储快照时进行按时间戳插入排序，或在此处查找时增加时间戳合法性检查。
        // 在缓冲区中寻找合适的两个快照进行插值
        int bestIdx = -1;
        int nextIdx = -1;

        for (int i = 0; i < buffer.count - 1; ++i) {
            int curr = (buffer.head - buffer.count + i + 10) % 10;
            int next = (curr + 1) % 10;
            // 检查 targetTime 是否落在 curr 和 next 这两个快照的时间戳之间
            if (buffer.snapshots[curr].timestamp <= targetTime && buffer.snapshots[next].timestamp >= targetTime) {
                bestIdx = curr;
                nextIdx = next;
                break; // 找到合适的区间，跳出循环
            }
        }
        // 5. 执行插值逻辑
        if (bestIdx != -1 && nextIdx != -1) {
            auto& s1 = buffer.snapshots[bestIdx];
            auto& s2 = buffer.snapshots[nextIdx];

            float t = (targetTime - s1.timestamp) / (s2.timestamp - s1.timestamp);
            t = std::clamp(t, 0.0f, 1.0f);
            // 位置：使用线性插值 (Lerp)
            tf.position = s1.pos * (1.0f - t) + s2.pos * t;
            // 旋转：使用球面线性插值 (Slerp)，保证旋转平滑且路径最短
            tf.rotation = NCL::Maths::Quaternion::Slerp(s1.rot, s2.rot, t);
        } else if (buffer.count > 0) {
            // 如果没找到（可能太新或太旧），则平滑地移向最新的快照
            int latest = (buffer.head - 1 + 10) % 10;
            tf.position = tf.position * (1.0f - dt * 10.0f) + buffer.snapshots[latest].pos * (dt * 10.0f);
            tf.rotation = NCL::Maths::Quaternion::Slerp(tf.rotation, buffer.snapshots[latest].rot, dt * 10.0f);
        }
    });
}

} // namespace ECS
