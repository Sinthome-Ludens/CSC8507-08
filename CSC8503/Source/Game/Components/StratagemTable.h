/**
 * @file StratagemTable.h
 * @brief Helldivers 2 stratagem code lookup table (45 combat stratagems).
 *
 * All codes are globally prefix-free by design, so any subset of 4
 * can be used directly with the chat system's prefix-match input logic.
 *
 * Categories: Orbital Strikes (12), Eagle Strikes (7),
 *             Support Weapons (17), Sentries (9).
 */
#pragma once

#include "Res_ChatState.h"   // DirKey, DirSequence::kMaxKeys

namespace ECS {

struct StratagemEntry {
    DirKey  keys[DirSequence::kMaxKeys];
    uint8_t length;
};

namespace {
    constexpr DirKey U = DirKey::Up;
    constexpr DirKey D = DirKey::Down;
    constexpr DirKey L = DirKey::Left;
    constexpr DirKey R = DirKey::Right;
}

inline constexpr StratagemEntry kStratagems[] = {
    // ── Orbital Strikes (0-11) ── first key: Right ──────────
    /* 0  Orbital Gatling Barrage   */ {{R,D,L,U,U},       5},
    /* 1  Orbital Airburst Strike   */ {{R,R,R},            3},
    /* 2  Orbital 120mm HE Barrage  */ {{R,R,D,L,R,D},     6},
    /* 3  Orbital 380mm HE Barrage  */ {{R,D,U,U,L,D,D},   7},
    /* 4  Orbital Walking Barrage   */ {{R,D,R,D,R,D},     6},
    /* 5  Orbital Laser             */ {{R,D,U,R,D},       5},
    /* 6  Orbital Railcannon Strike */ {{R,U,D,D,R},       5},
    /* 7  Orbital Precision Strike  */ {{R,R,U},            3},
    /* 8  Orbital Gas Strike        */ {{R,R,D,R},          4},
    /* 9  Orbital EMS Strike        */ {{R,R,L,D},          4},
    /* 10 Orbital Smoke Strike      */ {{R,R,D,U},          4},
    /* 11 Orbital Napalm Barrage    */ {{R,R,D,L,R,U},     6},

    // ── Eagle Strikes (12-18) ── first key: Up ─────────────
    /* 12 Eagle Strafing Run        */ {{U,R,R},            3},
    /* 13 Eagle Airstrike           */ {{U,R,D,R},          4},
    /* 14 Eagle Cluster Bomb        */ {{U,R,D,D,R},       5},
    /* 15 Eagle Napalm Airstrike    */ {{U,R,D,U},          4},
    /* 16 Eagle Smoke Strike        */ {{U,R,U,D},          4},
    /* 17 Eagle 110mm Rocket Pods   */ {{U,R,U,L},          4},
    /* 18 Eagle 500kg Bomb          */ {{U,R,D,D,D},       5},

    // ── Support Weapons (19-35) ── first key: Down ─────────
    /* 19 MG-43 Machine Gun         */ {{D,L,D,U,R},       5},
    /* 20 APW-1 Anti-Materiel Rifle */ {{D,L,R,U,D},       5},
    /* 21 M-105 Stalwart            */ {{D,L,D,U,U,L},     6},
    /* 22 EAT-17 Expendable AT      */ {{D,D,L,U,R},       5},
    /* 23 GR-8 Recoilless Rifle     */ {{D,L,R,R,L},       5},
    /* 24 FLAM-40 Flamethrower      */ {{D,L,U,D,U},       5},
    /* 25 AC-8 Autocannon           */ {{D,L,D,U,U,R},     6},
    /* 26 MG-206 Heavy Machine Gun  */ {{D,L,U,D,D},       5},
    /* 27 RS-422 Railgun            */ {{D,R,D,U,L,R},     6},
    /* 28 FAF-14 Spear              */ {{D,D,U,D,D},       5},
    /* 29 GL-21 Grenade Launcher    */ {{D,L,U,L,D},       5},
    /* 30 LAS-98 Laser Cannon       */ {{D,L,D,U,L},       5},
    /* 31 LAS-99 Quasar Cannon      */ {{D,D,U,L,R},       5},
    /* 32 RL-77 Airburst Rocket     */ {{D,U,U,L,R},       5},
    /* 33 MLS-4X Commando           */ {{D,L,U,D,R},       5},
    /* 34 StA-X3 W.A.S.P. Launcher  */ {{D,D,U,D,R},       5},
    /* 35 ARC-3 Arc Thrower         */ {{D,R,D,U,L,L},     6},

    // ── Sentries (36-44) ── first key: Down ────────────────
    /* 36 A/MG-43 Machine Sentry    */ {{D,U,R,R,U},       5},
    /* 37 A/G-16 Gatling Sentry     */ {{D,U,R,L},          4},
    /* 38 A/AC-8 Autocannon Sentry  */ {{D,U,R,U,L,U},     6},
    /* 39 A/M-12 Mortar Sentry      */ {{D,U,R,R,D},       5},
    /* 40 A/MLS-4X Rocket Sentry    */ {{D,U,R,R,L},       5},
    /* 41 A/ARC-3 Tesla Tower       */ {{D,U,R,U,L,R},     6},
    /* 42 A/M-23 EMS Mortar Sentry  */ {{D,U,R,D,R},       5},
    /* 43 A/LAS-98 Laser Sentry     */ {{D,U,R,D,U,R},     6},
    /* 44 A/FLAM-40 Flame Sentry    */ {{D,U,R,D,U,U},     6},
};

inline constexpr int kStratagemCount = 45;

static_assert(sizeof(kStratagems) / sizeof(kStratagems[0]) == kStratagemCount,
              "kStratagems size must match kStratagemCount");

} // namespace ECS
