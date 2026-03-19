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

    float targetTime = (resTime.totalTime - m_RenderDelay) * 1000.0f; // 转为毫秒

    // 遍历所有可插值的实体。普通网络实体依旧走旧链路；同图模式幽灵实体没有 NetworkIdentity，
    // 需要通过 remoteGhostEntity 单独放行。
    reg.view<C_D_Transform, C_D_InterpBuffer>().each([&](
        EntityID entity, C_D_Transform& tf, C_D_InterpBuffer& buffer) 
    {
        const bool isRemoteGhost = Entity::IsValid(resNet.remoteGhostEntity)
            && entity == resNet.remoteGhostEntity;
        const bool hasNetworkIdentity = reg.Has<C_D_NetworkIdentity>(entity);

        if (resNet.mode == PeerType::SERVER) {
            if (!isRemoteGhost) return;
        } else if (!hasNetworkIdentity && !isRemoteGhost) {
            return;
        }

        if (buffer.count <= 0) return;
        // NOTE: InterpBuffer_AddSnapshot 已保证缓冲区按时间戳严格递增排列，
        // 乱序/重复网络包在写入时即被丢弃，因此此处线性索引(curr + 1)是安全的。
        // 在缓冲区中寻找合适的两个快照进行插值
        int bestIdx = -1;
        int nextIdx = -1;

        for (int i = 0; i < buffer.count - 1; ++i) {
            int curr = (buffer.head - buffer.count + i + C_D_InterpBuffer::CAPACITY) % C_D_InterpBuffer::CAPACITY;
            int next = (curr + 1) % C_D_InterpBuffer::CAPACITY;
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

            float delta = s2.timestamp - s1.timestamp;
            float t = (delta > 0.0001f) ? (targetTime - s1.timestamp) / delta : 1.0f;
            t = std::clamp(t, 0.0f, 1.0f);
            // 位置：使用线性插值 (Lerp)
            tf.position = s1.pos * (1.0f - t) + s2.pos * t;
            // 旋转：使用球面线性插值 (Slerp)，保证旋转平滑且路径最短
            tf.rotation = NCL::Maths::Quaternion::Slerp(s1.rot, s2.rot, t);
        } else if (buffer.count > 0) {
            // 如果没找到（可能太新或太旧），则平滑地移向最新的快照
            int latest = (buffer.head - 1 + C_D_InterpBuffer::CAPACITY) % C_D_InterpBuffer::CAPACITY;
            tf.position = tf.position * (1.0f - dt * 10.0f) + buffer.snapshots[latest].pos * (dt * 10.0f);
            tf.rotation = NCL::Maths::Quaternion::Slerp(tf.rotation, buffer.snapshots[latest].rot, dt * 10.0f);
        }
    });
}

} // namespace ECS
