#pragma once
#pragma comment( lib, "PluginSDK.lib" )

#include "Classes/WebsocketManager.h"
#include "Classes/NameplatesManager.h"
#include "Classes/BallSpeedManager.h"
#include "Classes/ClockManager.h"
//#include "Classes/ReplayManager.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "MacrosStructsEnums.h"
#include "json.hpp"

//class ReplayManager;
using json = nlohmann::json;

#define SOS_DATADIR gameWrapper->GetDataFolderW()

// SOS CLASS
class SOS : public BakkesMod::Plugin::BakkesModPlugin
{
public:
    void onLoad() override;
    void onUnload() override;
private:
    std::string CurrentMatchGuid;

    std::vector<DummyStatEventContainer> addrs;

    // CVARS
    std::shared_ptr<bool>  cvarEnabled;
    std::shared_ptr<int>   cvarPort;
    std::shared_ptr<float> cvarUpdateRate;
    std::shared_ptr<bool>  bEnableDebugRendering;

    // MANAGERS
    std::shared_ptr<WebsocketManager> Websocket;
    std::shared_ptr<NameplatesManager> Nameplates;
    std::shared_ptr<BallSpeedManager> BallSpeed;
    std::shared_ptr<ClockManager> Clock;
   // std::shared_ptr<ReplayManager> Replay;

    // ORIGINAL SOS VARIABLES
    bool firstCountdownHit = false;
    bool matchCreated = false;
    bool isCurrentlySpectating = false;
    bool bInGoalReplay = false;
    bool bInPreReplayLimbo = false;
    bool bBallHasBeenHit = false;
    bool bPendingRestartFromKickoff = false;

    // GOAL SCORED VARIABLES
    LastTouchInfo lastTouch;
    Vector2F GoalImpactLocation = {0,0}; // top-left (0,0) bottom right (1,1)
	void GetGameStateInfo(json& state);
    Vector2F GetGoalImpactLocation(BallWrapper ball, void* params);

    // MAIN FUNCTION (GameState.cpp)
    void UpdateGameState();
    //void GetGameStateInfo(json& state);

    // HOOKS (EventHooks.cpp)
    void HookAllEvents();
    void HookViewportTick();
    void HookBallExplode();
    void HookOnHitGoal(BallWrapper ball, void* params);
    void HookInitTeams();
    void HookMatchCreated();
    void HookMatchDestroyed();
    void HookMatchEnded();
    void HookCountdownInit();
    void HookRoundStarted();
    void HookPodiumStart();
    void HookReplayCreated();
    void HookGoalReplayStart();
    void HookGoalReplayEnd();
    void HookStatEvent(ServerWrapper caller, void* params);
    void HookReplayScoreDataChanged(ActorWrapper caller);

    // TIME HOOKS
    void HookOnTimeUpdated();
    void HookOnOvertimeStarted(); // Is this not needed?
    void HookOnPauseChanged();
    void HookCarBallHit(CarWrapper car, void* params);
    void SetBallHit(bool bHit);

    // DATA GATHERING FUNCTIONS (GameState.cpp)
    void GetPlayerInfo(json& state, ServerWrapper server);
    void GetIndividualPlayerInfo(json& state, PriWrapper pri);
    void GetTeamInfo(json& state, ServerWrapper server);
    void GetGameTimeInfo(json& state, ServerWrapper server);
    void GetBallInfo(json& state, ServerWrapper server);
    void GetCurrentBallSpeed();
    void GetWinnerInfo(json& state, ServerWrapper server);
    void GetArenaInfo(json& state);
    void GetCameraInfo(json& state);
    void GetNameplateInfo();
    void GetLastTouchInfo(CarWrapper car, void* params);
    void GetStatEventInfo(ServerWrapper caller, void* params);

    // DEMO TRACKER
    std::map<std::string, int> DemolitionCountMap{};
    void DemoCounterIncrement(std::string playerId);
    int DemoCounterGetCount(std::string playerId);

    // DEBUGGING HELP
    void DebugRender(CanvasWrapper Canvas);
    void DrawTextVector(CanvasWrapper Canvas, Vector2 StartPosition, const std::vector<DebugText>& TextVector);
};