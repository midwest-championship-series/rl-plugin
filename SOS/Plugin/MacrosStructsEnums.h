#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"

extern std::shared_ptr<CVarManagerWrapper> globalCvarManager;

// MACROS //
#define SOS_VERSION_BASE "1.6.0-beta.6"
#ifdef USE_NAMEPLATES
    #define SOS_VERSION SOS_VERSION_BASE "-Nameplates"
#else
    #define SOS_VERSION SOS_VERSION_BASE
#endif


// STRUCTS //

struct LastTouchInfo
{
    std::string playerID;
    float speed = 0;
};

struct DummyStatEventContainer
{
    uintptr_t Receiver;
    uintptr_t Victim;
    uintptr_t StatEvent;

    bool operator ==(DummyStatEventContainer y)
    {
        return this->Receiver == y.Receiver && this->Victim == y.Victim && this->StatEvent == y.StatEvent;
    }
};

struct BallHitGoalParams
{
    uintptr_t GoalPointer;
    Vector HitLocation;
};

struct DebugText
{
    std::string Text;
    LinearColor Color = {0, 255, 0, 255};
};
