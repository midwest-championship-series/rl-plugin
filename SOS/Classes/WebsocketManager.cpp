#include "WebsocketManager.h"
#include "utils/parser.h"
#include "../Plugin/SOSUtils.h"

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
    server = std::make_shared<std::string>("");
    token = std::make_shared<std::string>("");
    CVarWrapper cserver = cvarManager->registerCvar("sos_server", "", "Server to send events to", true);
    cvarManager->registerCvar("sos_token", "", "Token for authentication").bindTo(token);
#endif

    cserver.bindTo(server);
    cserver.addOnValueChanged([cvarManager, gameWrapper, this](std::string s, CVarWrapper c) {
        // Stop websocket client then connect to new server
        if (h.opened()) {
            cvarManager->log("[SOS-SocketIO] Closing previous connection");
            h.sync_close();
        }
        cvarManager->log("[SOS-SocketIO] new server: " + c.getStringValue());
        h.clear_con_listeners();
        h.clear_socket_listeners();
        h.set_reconnect_delay(1000);
        h.set_reconnect_delay_max(1000);
        h.set_reconnect_attempts(0);
        h.set_reconnecting_listener([cvarManager, &c]() {
            cvarManager->log("[SOS-SocketIO] Attempting to reconnect to '" + c.getStringValue() + "'...");
        });
        h.set_fail_listener([cvarManager]() {
            cvarManager->log("[SOS-SocketIO] Failed to connect. Try reconnecting by resetting sos_server.");
        });
        h.set_open_listener([&c, gameWrapper, cvarManager, this]() {
            gameWrapper->SetTimeout([gameWrapper, cvarManager, this](GameWrapper*) {
                h.socket()->emit("mncs_register");
                cvarManager->log("[SOS-SocketIO] Attempting to log in...");
                h.socket()->emit("login", cvarManager->getCvar("sos_token").getStringValue(), [&](const sio::message::list& list) {
                    if (list[0]->get_string() == "good") {
                        cvarManager->log("[SOS-SocketIO] Login successful! Plugin ready!");
                        CheckKeepalive();
                    }
                    else
                        cvarManager->log("[SOS-SocketIO] Login failed! Try reconnecting by resetting sos_server.");
                });
            }, .3F);
        });
        h.set_close_listener([&](int const& reason) {
            cvarManager->log("[SOS-SocketIO] Socket closed.");
        });
        cvarManager->log("[SOS-SocketIO] Attempting to connect to " + c.getStringValue());
        h.connect(c.getStringValue());
        curServer = c.getStringValue();
    });

    cvarManager->setBind("F1", "sos_connect");
    cvarManager->registerNotifier("sos_connect", [&](std::vector<std::string> args) {
        StartPrompt();
    }, "Starts connection process to relay.", PERMISSION_ALL);
}

void WebsocketManager::StartPrompt() {
    cvarManager->log("Opening modals.");
    TextInputModalWrapper server = gameWrapper->CreateTextInputModal("BROADCAST");
    server.SetTextInput("", 50, false, [&](std::string input, bool canceled) {
        if (!canceled) {
            ModalWrapper connectingModal = gameWrapper->CreateModal("BROADCAST");
            connectingModal.SetBody("Connecting...");
            connectingModal.SetIcon("Icon_Spinner");
            connectingModal.ShowModal();
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
            h.set_fail_listener([&, connectingModal]() {
                ModalWrapper failModal = gameWrapper->CreateModal("BROADCAST");
                failModal.SetBody("Failed to connect.");
                failModal.SetIcon("Icon_Disconnected");
                failModal.AddButton("OK", true);
                failModal.ShowModal();
                cvarManager->log("[SOS-SocketIO] Failed to connect. Try reconnecting by resetting sos_server.");
            });
            h.set_open_listener([&, input]() {
                gameWrapper->SetTimeout([&](GameWrapper*) {
                    h.socket()->emit("mncs_register");
                    cvarManager->log("[SOS-SocketIO] Attempting to log in...");
                    h.socket()->emit("login", cvarManager->getCvar("sos_token").getStringValue(), [&](const sio::message::list& list) {
                        if (list[0]->get_string() == "good") {
                            ModalWrapper successModal = gameWrapper->CreateModal("BROADCAST");
                            successModal.SetBody("Connected!");
                            successModal.SetIcon("Icon_ConnectionStrength4");
                            successModal.AddButton("OK", true);
                            successModal.ShowModal();
                            cvarManager->log("[SOS-SocketIO] Login successful! Plugin ready!");
                            CheckKeepalive();
                        }
                        else
                            cvarManager->log("[SOS-SocketIO] Login failed! Try reconnecting by resetting sos_server.");
                        });
                    }, .3F);
                });
            h.set_close_listener([&](int const& reason) {
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

void WebsocketManager::SendEvent(std::string eventName, const json &jsawn)
{
    json event;
    event["event"] = eventName;
    event["data"] = jsawn;

    if (h.opened() && SOSUtils::ShouldRun(gameWrapper)) {
        h.socket()->emit("game event", event.dump());
    }
}

void WebsocketManager::CheckKeepalive() {
    gameWrapper->SetTimeout([&](GameWrapper*) {
        if (h.opened()) {
            WebsocketManager::SendKeepalive(curServer);
            WebsocketManager::CheckKeepalive();
        }
    }, 1.0F);
}

void WebsocketManager::StartClient()
{
    cvarManager->log("[SOS-SocketIO] Starting Client (" + *server + ")");
    h.set_reconnect_delay(1000);
    h.set_reconnect_delay_max(1000);
    h.set_reconnect_attempts(3);
    h.set_reconnecting_listener([&]() {
        cvarManager->log("[SOS-SocketIO] Attempting to reconnect to '" + *server + "'...");
    });
    h.set_fail_listener([&]() {
        cvarManager->log("[SOS-SocketIO] Failed to connect to '" + *server + "'. Try reconnecting by resetting sos_server.");
    });
    h.set_open_listener([&]() {
        gameWrapper->SetTimeout([&](GameWrapper*) {
            h.socket()->emit("register");
            cvarManager->log("[SOS-SocketIO] Attempting to log in...");
            h.socket()->emit("login", *token, [&](const sio::message::list& list) {
                if (list[0]->get_string() == "good") {
                    cvarManager->log("[SOS-SocketIO] Login successful! Plugin ready!");
                    CheckKeepalive();
                }
                else
                    cvarManager->log("[SOS-SocketIO] Login failed! Try reconnecting by resetting sos_server.");
            });
        }, .3F);
    });
    h.set_close_listener([&](int const& reason) {
        // 0: normal, 1: drop
        cvarManager->log("[SOS-SocketIO] Socket closed.");
    });
    h.connect(*server);
    curServer = *server;
}

void WebsocketManager::StopClient() {
    h.close();
}

void Get(std::string server) {
    try
    {
        curlpp::Cleanup cleaner;
        curlpp::Easy request;

        request.setOpt(new curlpp::options::Verbose(true));
        std::list<std::string> header;
        header.push_back("Expect: ");
        request.setOpt(new curlpp::options::HttpHeader(header));
        request.setOpt(new curlpp::options::Url(server));
        request.setOpt(new curlpp::options::SslVerifyPeer(false));

        request.perform();

        long res = curlpp::infos::ResponseCode::get(request); 
    }
    catch (curlpp::LogicError& e) { }
    catch (curlpp::RuntimeError& e) { }
    catch (...) { }
}

void WebsocketManager::SendKeepalive(std::string server) {
    std::thread http([server]() {
        Get(server);
    });
    http.detach();
}