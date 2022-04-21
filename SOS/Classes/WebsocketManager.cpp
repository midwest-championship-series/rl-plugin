#include "WebsocketManager.h"
#include "utils/parser.h"
#include "../Plugin/SOSUtils.h"
#include <ctime>

using nlohmann::json;

// PUBLIC FUNCTIONS //
WebsocketManager::WebsocketManager(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper)
    : cvarManager(cvarManager), gameWrapper(gameWrapper) {
#ifdef PREFILL
    server = std::make_shared<std::string>(RELAY_SERVER);
    CVarWrapper cserver = cvarManager->registerCvar("sos_server", RELAY_SERVER, "Server to send events to", true);
#else
    string plServ = getServer();
    server = std::make_shared<std::string>(plServ);
    CVarWrapper cserver = cvarManager->registerCvar("sos_server", plServ, "Server to send events to", true);
#endif
    cserver.bindTo(server);
    loggedIn = std::make_shared<bool>(false);

    cvarManager->setBind("F1", "sos_connect");
    cvarManager->registerNotifier("sos_connect", [&](std::vector<std::string> args) {
        StartPrompt();
    }, "Starts connection process to relay.", PERMISSION_ALL);
    gameWrapper->Execute([&](GameWrapper*) {
        StartPrompt();
    });
    gameWrapper->RegisterDrawable(std::bind(&WebsocketManager::Render, this, std::placeholders::_1));

    lib = std::make_shared<BCLib>(/*BCLib("ROCKET_LEAGUE", "RL_Bakkesmod", [&, gameWrapper](std::string msg) {
        // ERROR HERE
        gameWrapper->Execute([&, msg](GameWrapper*) {
            log(msg, true);
        });
    })*/);

    lib->StartCBServer();
}

// If the file is valid, loadLine will load the first line into *out.
void loadLine(wstring filename, string* out) {
    ifstream in(filename);
    if (in.is_open()) {
        string line;
        getline(in, line);
        if (line.length() > 0) {
            *out = line;
        }
        in.close();
    }
}

string WebsocketManager::getServer() {
    string server = "";
    loadLine(SOS_DATADIR + LR"(\sosio\server)", &server);
    return server;
}

void WebsocketManager::setServer(string server) {
    ofstream file(SOS_DATADIR + LR"(\sosio\server)", ios_base::trunc);
    file << server << endl;
    file.close();
}

void WebsocketManager::StartPrompt() {
    // Server Address -> Connect -> Save?

    cvarManager->log("Opening modals.");
    TextInputModalWrapper server = gameWrapper->CreateTextInputModal("SOSIO");
    server.SetTextInput(getServer(), 50, false, [&](std::string input, bool canceled) {
        if (!canceled) {
            lib->Disconnect();

            if(input.length() <= 11) {
                StartPrompt();
                return;
            }

            // Remove trailing slash
            if(input[input.length()-1] == '/') {
                input = input.substr(0, input.length() - 1);
            }

            cvarManager->log("[SOS-SocketIO] new server: " + input);
            lib->Connect(input);
        }
    });
#ifdef USE_TLS
    server.SetBody("Enter the relay server address. [HTTPS ONLY]");
#else
    server.SetBody("Enter the relay server address. [HTTP ONLY]");
#endif
    server.ShowModal();
}


void WebsocketManager::Render(CanvasWrapper cw) {
    cw.SetPosition(Vector2F{ 30.0, 30.0 });
    if(true) {//!lib->Connected) {
        LinearColor colors;
        colors.R = 255;
        colors.G = 0;
        colors.B = 0;
        colors.A = 255;
        cw.SetColor(colors);

        cw.DrawString("Plugin not connected.", 2.0, 2.0, false);
    } else if(!*loggedIn) {
        LinearColor colors;
        colors.R = 255;
        colors.G = 0;
        colors.B = 0;
        colors.A = 255;
        cw.SetColor(colors);

        cw.DrawString("Not logged in.", 2.0, 2.0, false);
    } else if(!SOSUtils::ShouldRun(gameWrapper)) {
        LinearColor colors;
        colors.R = 0;
        colors.G = 255;
        colors.B = 0;
        colors.A = 255;
        cw.SetColor(colors);

        cw.DrawString("Plugin connected!", 2.0, 2.0, false);
    }
}

void WebsocketManager::SendEvent(std::string eventName, const json &jsawn, bool force)
{
    lib->SendEvent(eventName, jsawn);
}

void WebsocketManager::StopClient() {
    lib->Stop();
}

void WebsocketManager::log(string text, bool console) {
    ofstream file(SOS_DATADIR + LR"(\sosio\log.txt)", ios_base::app);

    time_t now = time(0);
    tm* ltm = localtime(&now);
    string datestring = to_string(1 + ltm->tm_mon) + "/" + to_string(ltm->tm_mday) + "/" + to_string(1900 + ltm->tm_year) + " " + to_string(ltm->tm_hour) + ":" + to_string(ltm->tm_min) + ":" + to_string(ltm->tm_sec);

    file << "["+ datestring + "] [PLUGIN] " + text << endl;
    file.close();
    if (console) {
        cvarManager->log(text);
    }
}