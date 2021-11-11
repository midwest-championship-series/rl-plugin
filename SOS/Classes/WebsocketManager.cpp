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
    token = std::make_shared<std::string>(RELAY_PASSWORD);
    CVarWrapper cserver = cvarManager->registerCvar("sos_server", RELAY_SERVER, "Server to send events to", true);
    cvarManager->registerCvar("sos_token", RELAY_PASSWORD, "Token for authentication").bindTo(token);
#else
    string plServ = getServer();
    string plToken = getToken();
    server = std::make_shared<std::string>(plServ);
    token = std::make_shared<std::string>(plToken);
    CVarWrapper cserver = cvarManager->registerCvar("sos_server", plServ, "Server to send events to", true);
    cvarManager->registerCvar("sos_token", plToken, "Token for authentication").bindTo(token);
#endif
    cserver.bindTo(server);

    cvarManager->setBind("F1", "sos_connect");
    cvarManager->registerNotifier("sos_connect", [&](std::vector<std::string> args) {
        StartPrompt();
    }, "Starts connection process to relay.", PERMISSION_ALL);
    gameWrapper->Execute([&](GameWrapper*) {
        StartPrompt();
    });
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

string WebsocketManager::getToken() {
    string token = "";
    loadLine(SOS_DATADIR + LR"(\sosio\token)", &token);
    return token;
}

void WebsocketManager::setServer(string server) {
    ofstream file(SOS_DATADIR + LR"(\sosio\server)", ios_base::trunc);
    file << server << endl;
    file.close();
}

void WebsocketManager::setToken(string token) {
    ofstream file(SOS_DATADIR + LR"(\sosio\token)", ios_base::trunc);
    file << token << endl;
    file.close();
}

void WebsocketManager::AttemptLogin() {
    TextInputModalWrapper tokenModal = gameWrapper->CreateTextInputModal("SOSIO");
    tokenModal.SetTextInput(getToken(), 32, true, [&](std::string iToken, bool canceled) {

        if (canceled) {
            if (h.opened())
                h.socket()->close();
            return;
        }

        cvarManager->getCvar("sos_token").setValue(iToken);
        cvarManager->log("[SOS-SocketIO] Attempting to log in...");
        LOG("Logging in to SIO");
        h.socket()->emit("login", iToken, [&](const sio::message::list& list) {
            if (list[0]->get_string() == "good") {
                LOG("Logged in");
                gameWrapper->SetTimeout([&](GameWrapper* gw) {
                    ModalWrapper successModal = gw->CreateModal("SOSIO");
                    successModal.SetBody("Connected!");
                    successModal.SetIcon("gfx_shared.Icon_ConnectionStrength4");
                    successModal.AddButton("OK", true);
                    successModal.ShowModal();
                }, .0F);
                cvarManager->log("[SOS-SocketIO] Login successful! Plugin ready!");
                gameWrapper->Execute([&](GameWrapper* gw) {
                    LOG("Setting server");
                    setServer(curServer);
                    string token = cvarManager->getCvar("sos_token").getStringValue();
                    LOG("Setting token in AttemptLogin to " + token);
                    setToken(token);
                    LOG("Done");
                    gw->Toast("SOSIO", "Websocket connected");
                });
            }
            else {
                gameWrapper->SetTimeout([&](GameWrapper*) {
                    AttemptLogin();
                }, .0F);
                LOG("Login failed");
                cvarManager->log("[SOS-SocketIO] Login failed!");
            }
            });
        });
    tokenModal.SetBody("Incorrect! Enter your auth token:");
    tokenModal.ShowModal();
}

void WebsocketManager::StartPrompt() {
    // Server Address -> Connect -> Password -> Save?


    cvarManager->log("Opening modals.");
    TextInputModalWrapper server = gameWrapper->CreateTextInputModal("SOSIO");
    server.SetTextInput(getServer(), 50, false, [&](std::string input, bool canceled) {
        if (!canceled) {
            if (h.opened()) {
                cvarManager->log("[SOS-SocketIO] Closing previous connection");
                h.sync_close();
            }
            cvarManager->log("[SOS-SocketIO] new server: " + input);
            h.clear_con_listeners();
            h.clear_socket_listeners();
            h.set_reconnect_delay(1000);
            h.set_reconnect_delay_max(1000);
            h.set_reconnect_attempts(0);
            h.set_reconnecting_listener([&, input]() {
                cvarManager->log("[SOS-SocketIO] Attempting to reconnect to '" + input + "'...");
            });
            h.set_fail_listener([&]() {
                LOG("Socket failed");
                gameWrapper->SetTimeout([&](GameWrapper*) {
                    ModalWrapper failModal = gameWrapper->CreateModal("SOSIO");
                    failModal.SetBody("Failed to connect.");
                    failModal.SetIcon("gfx_shared.Icon_Disconnected");
                    failModal.AddButton("OK", true);
                    failModal.ShowModal();
                }, .0F);
                cvarManager->log("[SOS-SocketIO] Failed to connect. Try reconnecting by resetting sos_server.");
            });
            h.set_open_listener([&, input]() {
                gameWrapper->SetTimeout([&](GameWrapper* gW) {
                    LOG("Socket connected");

                    // GET TOKEN FROM FILE OR GET FROM PROMPT
                    string token = getToken();
                    if (strlen(token.c_str()) <= 0) {
                        TextInputModalWrapper tokenModal = gW->CreateTextInputModal("SOSIO");
                        tokenModal.SetTextInput("", 32, true, [&](std::string iToken, bool canceled) {
                            if (canceled) {
                                if (h.opened())
                                    h.socket()->close();
                                return;
                            }
                            cvarManager->getCvar("sos_token").setValue(iToken);
                            cvarManager->log("[SOS-SocketIO] Attempting to log in...");
                            LOG("Logging in to SIO");
                            h.socket()->emit("login", iToken, [&](const sio::message::list& list) {
                                if (list[0]->get_string() == "good") {
                                    LOG("Logged in");
                                    gameWrapper->SetTimeout([&](GameWrapper* gw) {
                                        ModalWrapper successModal = gw->CreateModal("SOSIO");
                                        successModal.SetBody("Connected!");
                                        successModal.SetIcon("gfx_shared.Icon_ConnectionStrength4");
                                        successModal.AddButton("OK", true);
                                        successModal.ShowModal();
                                    }, .0F);
                                    gameWrapper->Execute([&](GameWrapper* gw) {
                                        LOG("Setting server");
                                        setServer(curServer);
                                        string token = cvarManager->getCvar("sos_token").getStringValue();
                                        LOG("Setting token in AttemptLogin to " + token);
                                        setToken(token);
                                        LOG("Done");
                                        gw->Toast("SOSIO", "Websocket connected");
                                    });
                                    cvarManager->log("[SOS-SocketIO] Login successful! Plugin ready!");
                                }
                                else {
                                    gameWrapper->SetTimeout([&](GameWrapper*) {
                                        AttemptLogin();
                                    }, .0F);
                                    LOG("Login failed");
                                    cvarManager->log("[SOS-SocketIO] Login failed!");
                                }
                            });
                        });
                        tokenModal.SetBody("Enter your auth token:");
                        tokenModal.ShowModal();
                    }
                    else {
                        LOG("Logging in to SIO with prefilled token");
                        cvarManager->getCvar("sos_token").setValue(token);
                        cvarManager->log("[SOS-SocketIO] Attempting to log in...");
                        h.socket()->emit("login", token, [&](const sio::message::list& list) {
                            if (list[0]->get_string() == "good") {
                                LOG("Logged in");
                                gameWrapper->SetTimeout([&](GameWrapper* gw) {
                                    ModalWrapper successModal = gameWrapper->CreateModal("SOSIO");
                                    successModal.SetBody("Connected!");
                                    successModal.SetIcon("gfx_shared.Icon_ConnectionStrength4");
                                    successModal.AddButton("OK", true);
                                    successModal.ShowModal();
                                    gw->Toast("SOSIO", "Websocket connected");
                                }, .0F);
                                cvarManager->log("[SOS-SocketIO] Login successful! Plugin ready!");
                            }
                            else {
                                LOG("Login failed");
                                cvarManager->log("[SOS-SocketIO] Login failed!");
                                gameWrapper->SetTimeout([&](GameWrapper*) {
                                    AttemptLogin();
                                }, .0F);
                            }
                        });
                    }
                }, .3F);
            });
            h.set_close_listener([&](int const& reason) {
                LOG("Socket closed");
                cvarManager->log("[SOS-SocketIO] Socket closed.");
                gameWrapper->Execute([&](GameWrapper* gw) {
                    gw->Toast("SOSIO", "Websocket disconnected");
                });
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

void WebsocketManager::SendEvent(std::string eventName, const json &jsawn)
{
    json event;
    event["event"] = eventName;
    event["data"] = jsawn;

    if (h.opened() && SOSUtils::ShouldRun(gameWrapper)) {
        h.socket()->emit("game event", event.dump());
    }
}

void WebsocketManager::StopClient() {
    if(h.opened())
        h.close();
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