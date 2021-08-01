#include "WebsocketManager.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "json.hpp"
#include "utils/parser.h"

using nlohmann::json;

// PUBLIC FUNCTIONS //
WebsocketManager::WebsocketManager(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper)
    : cvarManager(cvarManager), gameWrapper(gameWrapper) {

    cvarServer = std::make_shared<std::string>("https://rl-relay.herokuapp.com");
    cvarToken = std::make_shared<std::string>("wTJ2bAQW9R6M89NTSaqUsAvQ");

    CVarWrapper server = cvarManager->registerCvar("sos_server", "https://rl-relay.herokuapp.com", "Server to send events to", true);
    server.bindTo(cvarServer);
    server.addOnValueChanged([cvarManager, gameWrapper, this](std::string s, CVarWrapper c) {
        // Stop websocket client then connect to new server
        if (h.opened()) {
            cvarManager->log("[SOS-SocketIO] Closing previous connection");
            h.sync_close();
        }
        cvarManager->log("[SOS-SocketIO] new server: " + c.getStringValue());
        h.clear_con_listeners();
        h.clear_socket_listeners();
        h.set_reconnect_delay(0);
        h.set_reconnect_delay_max(1000);
        h.set_reconnect_attempts(0);
        h.set_reconnecting_listener([cvarManager, &c]() {
            cvarManager->log("[SOS-SocketIO] Attempting to reconnect to '" + c.getStringValue() + "'...");
        });
        h.set_fail_listener([cvarManager]() {
            cvarManager->log("[SOS-SocketIO] Failed to connect. Try reconnecting by resetting sos_server.");
        });
        h.set_open_listener([gameWrapper, cvarManager, this]() {
            gameWrapper->SetTimeout([gameWrapper, cvarManager, this](GameWrapper*) {
                h.socket()->emit("mncs_register");
                cvarManager->log("[SOS-SocketIO] Attempting to log in...");
                h.socket()->emit("login", cvarManager->getCvar("sos_token").getStringValue(), [cvarManager](const sio::message::list& list) {
                    if (list[0]->get_string() == "good")
                        cvarManager->log("[SOS-SocketIO] Login successful! Plugin ready!");
                    else
                        cvarManager->log("[SOS-SocketIO] Login failed! Try reconnecting by resetting sos_server.");
                });
            }, .3F);
        });
        cvarManager->log("[SOS-SocketIO] Attempting to connect to " + c.getStringValue());
        h.connect(c.getStringValue());
    });

    cvarManager->registerCvar("sos_token", "wTJ2bAQW9R6M89NTSaqUsAvQ", "Token for authentication").bindTo(cvarToken);
}

void WebsocketManager::SendEvent(std::string eventName, const json &jsawn)
{
    json event;
    event["event"] = eventName;
    event["data"] = jsawn;

    if (h.opened()) {
        h.socket()->emit("game event", event.dump());
    }
}

void WebsocketManager::StartClient()
{
    cvarManager->log("[SOS-SocketIO] Starting Client (" + *cvarServer + ")");
    h.set_reconnect_delay(0);
    h.set_reconnect_delay_max(1000);
    h.set_reconnect_attempts(3);
    h.set_reconnecting_listener([&]() {
        cvarManager->log("[SOS-SocketIO] Attempting to reconnect to '" + *cvarServer + "'...");
    });
    h.set_fail_listener([&]() {
        cvarManager->log("[SOS-SocketIO] Failed to connect to '" + *cvarServer + "'. Try reconnecting by resetting sos_server.");
    });
    h.set_open_listener([&]() {
        gameWrapper->SetTimeout([&](GameWrapper*) {
            h.socket()->emit("mncs_register");
            cvarManager->log("[SOS-SocketIO] Attempting to log in...");
            h.socket()->emit("login", *cvarToken, [&](const sio::message::list& list) {
                if (list[0]->get_string() == "good")
                    cvarManager->log("[SOS-SocketIO] Login successful! Plugin ready!");
                else
                    cvarManager->log("[SOS-SocketIO] Login failed! Try reconnecting by resetting sos_server.");
            });
        }, .3F);
    });
    h.connect(*cvarServer);
}

void WebsocketManager::StopClient() {
    h.close();
}