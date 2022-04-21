#pragma once

#include "Plugin/MacrosStructsEnums.h"
#include "nlohmann/json.hpp"
#include "Plugin/SOS.h"
#include "bclib++.hpp"

#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <set>

using nlohmann::json;
using namespace std;
class CVarManagerWrapper;
class SOS;

#ifdef USE_TLS
#pragma comment(lib, "sioclient_tls.lib")
#else
#pragma comment(lib, "sioclient.lib")
#endif

#ifdef PREFILL
#include "Secrets.h"
#endif

#define LOG(text) \
    log(text, false)

#define LOGC(text) \
    log(text, true)

class WebsocketManager
{
public:
    WebsocketManager(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper);

    void StopClient();
    
    void SendEvent(std::string eventName, const json& jsawn, bool force = false);
    void StartPrompt();

    void log(std::string text, bool console = false);
private:
    WebsocketManager() = delete; // No default constructor

    std::string curServer;

    std::shared_ptr<bool> loggedIn;
    std::shared_ptr<std::string> server;
    std::shared_ptr<GameWrapper> gameWrapper;
    std::shared_ptr<CVarManagerWrapper> cvarManager;
    std::shared_ptr<BCLib> lib;

    std::string getServer();
    void setServer(std::string server);    
    void Render(CanvasWrapper canvas);
};
