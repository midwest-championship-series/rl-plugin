#pragma once
#pragma comment( lib, "PluginSDK.lib" )

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "json.hpp"

#ifdef _WIN32

#define CURL_STATICLIB
#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib, "Wldap32.Lib")
#pragma comment(lib, "Crypt32.Lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssl_static.lib")
#pragma comment(lib, "Wldap32.lib")
#pragma comment(lib, "Normaliz.lib")
#pragma comment(lib, "libcurl.lib")
#pragma comment(lib, "curlpp.lib")
#include <windows.h>
#include <Shlobj_core.h>
#include <sstream>
#include <memory>

#include <string>
#include <curl/curl.h>
#include <curl/easy.h>
#include <sstream>
#include <iostream>
#include <fstream>

/*
//#include "curlpp/curlpp.cpp"
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"
#include "curlpp/Exception.hpp"
#include "curlpp/Infos.hpp"

#include <thread>
*/

#endif

using namespace std;
using json = nlohmann::json;

#define LOG(text) \
    log(text, false)

#define LOGC(text) \
    log(text, true)

class Updater : public BakkesMod::Plugin::BakkesModPlugin
{
public:
    wstring baseDir = gameWrapper->GetDataFolderW() + LR"(\sosio)";
    wstring saveDest = baseDir + LR"(\SOSIO.dll)";
    wstring pluginDest = gameWrapper->GetBakkesModPathW() + LR"(\plugins\SOSIO.dll)";
    string downloadURL = "";
    string curVersion = "";
    string newVersion = "";
    string newVersionNum = "";

    void onLoad() override;
    void onUnload() override;

    bool download(const string& url);
    bool updateAvailable();

    void log(string text, bool console = false);
private:
    string getLocalVersion();
    string getUpdateUrl();
    void setUpdateUrl(string url);
};