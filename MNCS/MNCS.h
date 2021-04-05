#pragma once
#pragma comment( lib, "pluginsdk.lib" )

#include <set>
#include <iostream>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "Structs.h"
#include "socketio/sio_client.h"

// USINGS
using namespace std::chrono;
using namespace std::placeholders;

// FORWARD DECLARATIONS
namespace json
{
    class JSON;
};

#ifdef USE_NAMEPLATES
namespace RT
{
    class Frustum;
}
#endif

// LOGGING PREPROCESSOR MACROS (change SHOULDLOG 0 to 1 if you want debug logging)
#define SHOULDLOG 0
#if SHOULDLOG
    #define LOGC(x) cvarManager->log(x)
#else
    #define LOGC(x)
#endif

#define USE_TLS

#ifdef USE_TLS
#pragma comment(lib, "sioclient_tls.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "libcrypto64MT.lib")
#pragma comment(lib, "libssl64MT.lib")
#else
#pragma comment(lib, "sioclient.lib")
#endif

class MNCS : public BakkesMod::Plugin::BakkesModPlugin
{
public:
    void onLoad() override;
    void onUnload() override;
    void OnEnabledChanged();

private:
    // CVARS
    std::shared_ptr<bool>  cvarEnabled;
    std::shared_ptr<bool>  cvarUseBase64;
    std::shared_ptr<std::string> cvarServer;
    std::shared_ptr<std::string> cvarToken;
    std::shared_ptr<float> cvarUpdateRate;

    // FLOAT TIME VARIABLES
    steady_clock::time_point timeSnapshot;
    float decimalTime = 0;
    float cachedDecimalTimeOnPause = 0;
    bool isClockPaused = false;
    bool newRoundActive = false;
    bool waitingForOvertimeToStart = false;

    // ORIGINAL SOS VARIABLES
    bool firstCountdownHit = false;
    bool matchCreated = false;
    bool isCurrentlySpectating = false;
    bool isInReplay = false;

    // BALL SPEED / GOAL SPEED VARIABLES
    bool bLockBallSpeed = false;
    float ballCurrentSpeed = 0;
    float goalSpeed = 0;
    LastTouchInfo lastTouch;
    Vector2F GoalImpactLocation = {0,0}; // top-left (0,0) bottom right (1,1)

    // RESTRICTION FUNCTION
    bool ShouldRun();
    bool IsSafeMode(int CurrentPlaylist, const std::vector<int>& SafePlaylists);

    // MAIN FUNCTION (GameState.cpp)
    ServerWrapper GetCurrentGameState();
    void UpdateGameState();
    bool GetGameStateInfo(class json::JSON& state);

    // HOOKS (EventHooks.cpp)
    void HookAllEvents();
    void HookViewportTick();
    void HookBallExplode();
    void HookOnHitGoal(BallWrapper ball, void* params, std::string funcName);
    void HookInitTeams();
    void HookMatchCreated();
    void HookMatchDestroyed();
    void HookMatchEnded();
    void HookCountdownInit();
    void HookPodiumStart();
    void HookReplayCreated();
    void HookReplayStart();
    void HookReplayEnd();
    void HookReplayWillEnd();
    void HookStatEvent(ServerWrapper caller, void* args);
    void HookCarBallHit(CarWrapper car, void* params, std::string funcName);

    // DATA GATHERING FUNCTIONS (GameState.cpp)
    void GetNameAndID(PriWrapper PRI, std::string &name, std::string &ID);
    void GetPlayerInfo(class json::JSON& state, PriWrapper pri);
    void GetTeamInfo(class json::JSON& state, ServerWrapper server);
    void GetGameTimeInfo(class json::JSON& state, ServerWrapper server);
    void GetBallInfo(class json::JSON& state, ServerWrapper server);
    void GetWinnerInfo(class json::JSON& state, ServerWrapper server);
    void GetArenaInfo(class json::JSON& state);
    void GetCameraInfo(class json::JSON& state);
    void GetLastTouchInfo(CarWrapper car);
    void GetCurrentBallSpeed();
    
    // NAMEPLATE FUNCTIONS (Nameplates.cpp)
    #ifdef USE_NAMEPLATES
    //FOV: Anything above 90 degrees remains at 1.0
	//DISTANCE: Anything above 10,000 cm remains at 9.0
	const float FOVMin = 0.1f; //0 degrees
	const float FOVMax = 1.0f; //90 degrees
	const float distMin = 0.5f; //0 centimeters
	const float distMax = 9.0f; //10,000 centimeters
    void GetNameplateInfo(CanvasWrapper canvas);
    void GetIndividualNameplate(CanvasWrapper canvas, class RT::Frustum &frustum, class json::JSON& nameplatesState, CarWrapper car);
    float GetBallVerticalRadius(CanvasWrapper canvas, BallWrapper ball, CameraWrapper camera, class RT::Frustum &frustum);
    #endif

    // CLOCK FUNCTIONS (Clock.cpp)
    void UpdateClock();
    void PauseClockOnGoal();
    void UnpauseClockOnBallTouch();
    void PauseClockOnOvertimeStarted();
    void OnPauseChanged();

    // WEBSOCKET FUNCTIONS
    sio::client h;
    void RunClient();
    void StopClient();
    void SendEvent(std::string eventName, const class json::JSON& jsawn);

    // UTILITY FUNCTIONS
    float ToKPH(float RawSpeed);
    void LockBallSpeed();
    Vector2F GetGoalImpactLocation(BallWrapper ball, void* params, std::string funcName);
};