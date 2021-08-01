#pragma once

#include <set>
#include "Plugin/MacrosStructsEnums.h"
#include "json.hpp"
#include "Plugin/SOS.h"
#include "socketio/sio_client.h"


using nlohmann::json;
class CVarManagerWrapper;
class SOS;

#define USE_TLS

#ifdef USE_TLS
#pragma comment(lib, "sioclient_tls.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "libcrypto64MT.lib")
#pragma comment(lib, "libssl64MT.lib")
#else
#pragma comment(lib, "sioclient.lib")
#endif

class WebsocketManager
{
public:
    WebsocketManager(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper);

    void StartClient();
    void StopClient();
    
    void SendEvent(std::string eventName, const json& jsawn);

private:
    WebsocketManager() = delete; // No default constructor

    std::shared_ptr<std::string> cvarServer;
    std::shared_ptr<std::string> cvarToken;
    std::shared_ptr<CVarManagerWrapper> cvarManager;
    std::shared_ptr<GameWrapper> gameWrapper;
    sio::client h;
};
