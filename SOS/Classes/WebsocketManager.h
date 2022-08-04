#pragma once

// Project Definitions

#define USE_TLS

// Dependencies
//#define CURLPP_STATICLIB
//#define CURL_STATICLIB
#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#define WIN32_LEAN_AND_MEAN

#ifdef _WIN32
//#pragma comment(lib, "Wldap32.Lib")
//#pragma comment(lib, "crypt32.Lib")
//#pragma comment(lib, "ws2_32.lib")
//#pragma comment(lib, "Wldap32.lib")
//#pragma comment(lib, "Normaliz.lib")
//#pragma comment(lib, "libcurl.lib")
//#pragma comment(lib, "curlpp.lib")
#include <windows.h>
#include <shellapi.h>
#include <Shlobj_core.h>
#include <sstream>
#include <memory>

//#include "curlpp/curlpp.cpp"
//#include "curlpp/cURLpp.hpp"
//#include "curlpp/Easy.hpp"
//#include "curlpp/Options.hpp"
//#include "curlpp/Exception.hpp"
//#include "curlpp/Infos.hpp"

#include <thread>
#endif

#include "Plugin/MacrosStructsEnums.h"
#include "json.hpp"
#include "Plugin/SOS.h"
#include "socketio/sio_client.h"


#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"

#include <set>
#include <iostream>
#include <fstream>


using nlohmann::json;
class CVarManagerWrapper;
using websocketpp::connection_hdl;
using namespace std;

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

#define LOG(text) \
    log(text, false)

#define LOGC(text) \
    log(text, true)

class WebsocketManager
{

    using PluginServer = websocketpp::server<websocketpp::config::asio>;
    using ConnectionSet = std::set<connection_hdl, std::owner_less<connection_hdl>>;

public:
    WebsocketManager(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper);

    void Disconnect();
    void Connect(const std::string& serv) { Connect(serv, ""); }
    void Connect(const std::string& serv, const std::string& token);

    void StartServer();
    void StopServer();
    void SetbUseBase64(bool bNewValue) { bUseBase64 = bNewValue; }
    void Destroy();

    
    void SendEvent(std::string eventName, const json& jsawn);
    void StartPrompt();

    bool unloading = false;

    void log(std::string text, bool console = false);
private:
    WebsocketManager() = delete; // No default constructor

    std::string curServer;


    int ListenPort = 8477;
    bool bUseBase64 = false;

    void OnHttpRequest(connection_hdl hdl);
    void SendWebSocketPayload(const json& jsawn);
    void OnWsMsg(connection_hdl hdl, PluginServer::message_ptr msg);
    void OnWsOpen(connection_hdl hdl);
    void OnWsClose(connection_hdl hdl) { ws_connections->erase(hdl); }

    PluginServer* ws_server = nullptr;
    ConnectionSet* ws_connections = nullptr;


    std::shared_ptr<bool> loggedIn;
    std::shared_ptr<std::string> server;
    std::shared_ptr<std::string> token;
    std::shared_ptr<GameWrapper> gameWrapper;
    std::shared_ptr<CVarManagerWrapper> cvarManager;
    sio::client h;

    json::string_t DumpMessage(json jon);


    std::string getServer();
    std::string getToken();
    void setServer(std::string server);
    void setToken(std::string token);
    void Render(CanvasWrapper canvas);

    void AttemptLogin();

    std::vector<std::string> split(std::string s, std::string delimiter) {
        size_t pos_start = 0, pos_end, delim_len = delimiter.length();
        std::string token;
        std::vector<std::string> res;

        while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
            token = s.substr(pos_start, pos_end - pos_start);
            pos_start = pos_end + delim_len;
            res.push_back(token);
        }

        res.push_back(s.substr(pos_start));
        return res;
    }
};
