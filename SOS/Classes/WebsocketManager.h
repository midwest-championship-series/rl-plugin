#pragma once

// Project Definitions

#define MNCS
//#define UMN
#define PREFILL
//#define USE_TLS

// Dependencies
#define CURLPP_STATICLIB
#define CURL_STATICLIB

#ifdef _WIN32
#pragma comment(lib, "Wldap32.Lib")
#pragma comment(lib, "crypt32.Lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Wldap32.lib")
#pragma comment(lib, "Normaliz.lib")
#pragma comment(lib, "libcurl.lib")
#pragma comment(lib, "curlpp.lib")
#include <windows.h>
#include <Shlobj_core.h>
#include <sstream>
#include <memory>

//#include "curlpp/curlpp.cpp"
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"
#include "curlpp/Exception.hpp"
#include "curlpp/Infos.hpp"

#include <thread>
#endif

#include <set>
#include "Plugin/MacrosStructsEnums.h"
#include "json.hpp"
#include "Plugin/SOS.h"
#include "socketio/sio_client.h"

#include <iostream>
#include <fstream>


using nlohmann::json;
class CVarManagerWrapper;
class SOS;

#ifdef USE_TLS
#pragma comment(lib, "sioclient_tls.lib")
#pragma comment(lib, "libcrypto64MT.lib")
#pragma comment(lib, "libssl64MT.lib")
#else
#pragma comment(lib, "sioclient.lib")
#endif

#ifdef PREFILL
#include "Secrets.h"
#endif

class WebsocketManager
{
public:
    WebsocketManager(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper);

    void StartClient();
    void StopClient();
    void SendKeepalive(std::string server);
    
    void SendEvent(std::string eventName, const json& jsawn);
    void StartPrompt();

private:
    WebsocketManager() = delete; // No default constructor

    std::string curServer;

    void CheckKeepalive();

    std::shared_ptr<std::string> server;
    std::shared_ptr<std::string> token;
    std::shared_ptr<GameWrapper> gameWrapper;
    std::shared_ptr<CVarManagerWrapper> cvarManager;
    sio::client h;
};
