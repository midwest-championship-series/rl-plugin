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
            if (h.opened()) {
                *loggedIn = false;
                cvarManager->log("[SOS-SocketIO] Closing previous connection");
                h.sync_close();
            }
            if(input.length() <= 11) {
                StartPrompt();
                return;
            }

            // Remove trailing slash
            if(input[input.length()-1] == '/') {
                input = input.substr(0, input.length() - 1);
            }

            cvarManager->log("[SOS-SocketIO] new server: " + input);
            h.clear_con_listeners();
            h.clear_socket_listeners();
            h.set_reconnect_delay(1000);
            h.set_reconnect_delay_max(5000);
            h.set_reconnect_attempts(2);
            h.set_reconnecting_listener([&, input]() {
                cvarManager->log("[SOS-SocketIO] Attempting to reconnect to '" + input + "'...");
                *loggedIn = false;
            });
            h.set_fail_listener([&]() {
                LOG("Socket failed");
                gameWrapper->SetTimeout([&](GameWrapper*) {
                    ModalWrapper failModal = gameWrapper->CreateModal("SOSIO");
                    failModal.SetBody("Failed to connect.");
                    failModal.SetIcon("gfx_shared.Icon_Disconnected");
                    failModal.AddButton("OK", true);
                    failModal.ShowModal();
                    *loggedIn = false;
                }, .0F);
                cvarManager->log("[SOS-SocketIO] Failed to connect. Try reconnecting by resetting sos_server.");
            });
            h.set_open_listener([&, input]() {
                *loggedIn = false;
                gameWrapper->SetTimeout([&, input](GameWrapper* gW) {
                    LOG("Socket connected");

                    h.socket()->emit("login", sio::message::list("PLUGIN"), [&, input](const sio::message::list& list) {
                        string res = list[0]->get_string();
                        string site = input + res;
                        LOG("RESPONSE: ", res);
                        LOG("Total: ", site);

                        // Open default browser
                        ShellExecuteA(0, 0, site.c_str(), 0, 0, SW_SHOW);
                    });

                    h.socket()->on("logged_in", [&](const std::string& name,sio::message::ptr const& message,bool need_ack, sio::message::list& ack_message) {
                        gameWrapper->SetTimeout([&](GameWrapper* gw) {
                            *loggedIn = true;
                            setServer(curServer);
                            ModalWrapper successModal = gw->CreateModal("SOSIO");
                            successModal.SetBody("Logged in!");
                            successModal.AddButton("OK", true);
                            successModal.ShowModal();
                            h.set_reconnect_attempts(3600);
                        }, .0F);
                    });
                }, .3F);
            });
            h.set_close_listener([&](int const& reason) {
                *loggedIn = false;
                LOG("Socket closed");
                cvarManager->log("[SOS-SocketIO] Socket closed.");
            });
            cvarManager->log("[SOS-SocketIO] Attempting to connect to " + input);
            h.connect(input);
            curServer = input;
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
    if(!h.opened()) {
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
    json event;
    event["event"] = eventName;
    event["data"] = jsawn;
    if(eventName == "game:update_state") {
        event["game"] = "ROCKET_LEAGUE";
    }

    if (h.opened() && (force || (SOSUtils::ShouldRun(gameWrapper)))) {
        h.socket()->emit("game event", event.dump());
    }
}

void WebsocketManager::StopClient() {
    if(h.opened()) {
        h.socket()->off_all();
        h.clear_con_listeners();
        h.clear_socket_listeners();
        h.set_reconnect_attempts(0);
        h.set_reconnect_delay(0);
        cvarManager->log("sync_close()");
        clock_t time_req = clock();
        h.sync_close();
        time_req = clock() - time_req;
        cvarManager->log("post close ("+to_string(((float)time_req/CLOCKS_PER_SEC))+"s)");
    }
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