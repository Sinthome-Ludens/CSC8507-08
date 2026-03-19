/**
 * @file Res_AudioConfig.h
 * @brief 音频系统配置与运行时状态资源。
 *
 * 枚举定义所有 BGM/SFX 标识，路径映射集中管理。
 * 代码中只引用枚举（BgmId::Menu / SfxId::Kill），永远不写文件路径字符串。
 * 新增音效只需：加枚举值 + 加路径，Sys_Audio 无需修改。
 */
#pragma once

#include <cstdint>

namespace ECS {

/**
 * BGM 标识枚举（按全局警戒度阶段，非按地图）
 * 严格 1:1 状态机：任何时刻只有一条 BGM 在播放
 */
enum class BgmId : uint8_t {
    None = 0,
    Menu,             ///< 主菜单
    GameplayNormal,   ///< 游玩-正常（Safe/Search 低警戒）
    GameplayTense,    ///< 游玩-二阶段（Alert 中警戒）
    GameplayDanger,   ///< 游玩-三阶段（Hunt 高警戒）
    Victory,          ///< 胜利结算
    Defeat,           ///< 失败（玩家死亡 / 倒计时耗尽 / 积分不达标）
    Countdown,        ///< 30s 倒计时
    COUNT
};

/**
 * SFX 标识枚举
 * one-shot 播放，可互相重叠，可与 BGM 重叠
 */
enum class SfxId : uint8_t {
    None = 0,
    HuntEnter,        ///< 敌人进入 Hunt
    UIClick,          ///< UI 点击（菜单导航/确认/返回统一）
    FinishZone,       ///< 到达结算点
    EnemyKillA,       ///< 敌人死亡变体 A（33% 权重）
    EnemyKillB,       ///< 敌人死亡变体 B（33% 权重）
    EnemyKillC,       ///< 敌人死亡变体 C（33% 权重）
    PlayerDeath,      ///< 玩家死亡
    ItemPickup,       ///< 道具拾取（碰撞触发，含钥匙卡）
    ItemUse,          ///< 道具使用（全道具共用）
    WeaponUse,        ///< 武器使用（全武器共用）
    DialoguePopup,    ///< 对话气泡弹出
    DialogueTimeout,  ///< 对话倒计时提醒
    COUNT
};

/**
 * 音频路径配置（集中管理所有路径，消除硬编码）
 * session 级 ctx 资源，跨场景保持
 */
struct Res_AudioConfig {
    const char* bgmPaths[(int)BgmId::COUNT] = {
        "",                                    // None
        "Audio/BGM/MainMenu-BGM.mp3",   // Menu
        "Audio/BGM/Patrik Andrén,Johan Soderqvist,Embark Studios - The Wanderer (Prologue).mp3", // GameplayNormal
        "Audio/BGM/Normal.mp3",          // GameplayTense
        "Audio/BGM/Hunt.mp3",            // GameplayDanger
        "Audio/BGM/Succeed.mp3",         // Victory
        "Audio/BGM/Fail.mp3",            // Defeat
        "Audio/BGM/30SecCountdown.mp3",  // Countdown
    };

    const char* sfxPaths[(int)SfxId::COUNT] = {
        "",                                                                             // None
        "",                                                                             // HuntEnter（预留）
        "Audio/SFX/button_press.mp3",                                            // UIClick
        "Audio/SFX/sequence_success.mp3",                                        // FinishZone
        "Audio/SFX/windows-xp-error-sound.mp3",                                  // EnemyKillA
        "Audio/SFX/disappear-scream.mp3",                                        // EnemyKillB
        "Audio/SFX/EnemyKilled.mp3",                                             // EnemyKillC
        "",                                                                             // PlayerDeath（预留）
        "Audio/SFX/itemGet.mp3",                                                 // ItemPickup
        "Audio/SFX/WeaponUsage.mp3",                                              // ItemUse
        "Audio/SFX/minecraft-sword-swing.mp3",                                       // WeaponUse
        "Audio/SFX/universfield-new-notification-018-363746.mp3",                 // DialoguePopup
        "Audio/SFX/lesiakower-error-mistake-sound-effect-incorrect-answer-437420.mp3", // DialogueTimeout
    };

    int   maxChannels           = 32;    ///< FMOD 最大同时播放通道数
    float bgmVolumeMul         = 0.8f;   ///< BGM 音量乘数（相对于 masterVolume）

    /**
     * 警戒度 → BGM 映射阈值
     * alertLevel <= alertThresholdNormal → GameplayNormal
     * alertLevel <= alertThresholdTense  → GameplayTense
     * alertLevel >  alertThresholdTense  → GameplayDanger
     */
    float alertThresholdNormal = 50.0f;
    float alertThresholdTense  = 75.0f;
};

/**
 * 运行时音频状态（Sys_Audio 读写，其他系统只写 requestedBgm）
 * session 级 ctx 资源，跨场景保持，不在 OnExit 中 erase
 */
struct Res_AudioState {
    BgmId currentBgm   = BgmId::None;   ///< 当前正在播放的 BGM
    BgmId requestedBgm = BgmId::None;   ///< 请求切换到的 BGM（Sys_Audio 消费后清零）
    BgmId preBgm       = BgmId::None;   ///< 被倒计时临时覆盖前的 BGM
    bool  bgmOverride  = false;          ///< 当前是否处于临时覆盖状态（倒计时）
    bool  isGameplay   = false;          ///< 当前是否在游玩状态（允许警戒度自动驱动 BGM）
};

} // namespace ECS
