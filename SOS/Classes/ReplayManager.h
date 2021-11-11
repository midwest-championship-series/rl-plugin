#pragma once
#include "Plugin/SOS.h"

#include "bakkesmod/wrappers/GameEvent/ReplayWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ReplayDirectorWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ReplaySoccarWrapper.h"
#include "bakkesmod/wrappers/wrapperstructs.h"

#include "uploader/Utils.h"
#include "uploader/Ballchasing.h"
#include "uploader/Calculated.h"
#include "uploader/Match.h"
#include "uploader/Player.h"
#include "uploader/Replay.h"

#include "json.hpp"

using nlohmann::json;
class CVarManagerWrapper;
class SOS;

using namespace std;

class ReplayManager
{
public:

    ReplayManager(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper, std::shared_ptr<WebsocketManager> Websocket);
    void OnGameComplete(ServerWrapper caller, void* params, std::string eventName);
    void onUnload();

    std::shared_ptr<CVarManagerWrapper> cvarManager;
    std::shared_ptr<GameWrapper> gameWrapper;
    std::shared_ptr<WebsocketManager> Websocket;
private:
    ReplayManager() = delete; // No default constructor

    void GetPlayerData(ServerWrapper caller, void* params, string eventName);
    string SetReplayName(ServerWrapper& server, ReplaySoccarWrapper& soccarReplay);
    std::string ExportReplay(ReplaySoccarWrapper& soccarReplay, std::string replayName);

    shared_ptr<string> replayNameTemplate = make_shared<string>("{YEAR}-{MONTH}-{DAY}.{HOUR}.{MIN} {PLAYER} {MODE} {WINLOSS}");
    shared_ptr<int> templateSequence = make_shared<int>(0);
    Ballchasing* ballchasing;
    Calculated* calculated;
};
