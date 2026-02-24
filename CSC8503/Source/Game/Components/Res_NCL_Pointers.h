#pragma once

namespace NCL::CSC8503 {
    class GameWorld;
    class PhysicsSystem;
    class GameTechRendererInterface;
}

struct Res_NCL_Pointers {
    NCL::CSC8503::GameWorld*                 world    = nullptr;
    NCL::CSC8503::PhysicsSystem*             physics  = nullptr;
    NCL::CSC8503::GameTechRendererInterface* renderer = nullptr;
};
