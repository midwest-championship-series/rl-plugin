#include "MNCS.h"
#include "json.hpp"
#include "utils/parser.h"
#include "socketio/sio_message.h"

// SEND EVENTS
void MNCS::SendEvent(std::string eventName, const json::JSON &jsawn)
{
    json::JSON event;
    event["event"] = eventName;
    event["data"] = jsawn;

    if (h.opened()) {
        h.socket()->emit("game event", event.dump());
    }
}

// WEBSOCKET CODE
void MNCS::RunClient()
{
    cvarManager->log("[MNCS] Starting Client ("+*cvarServer+")");
    h.set_reconnect_delay(2000);
    h.set_reconnect_delay_max(3000);
    h.set_reconnecting_listener([&]() {
        cvarManager->log("Attempting to reconnect to '"+*cvarServer+"'...");
    });
    h.set_open_listener([&]() {
        cvarManager->log("Connected to " + *cvarServer + "! Attempting to log in...");
        h.socket()->emit("login", *cvarToken, [&](const sio::message::list& list) {
            if (list[0]->get_map()["status"]->get_string() == "good")
                cvarManager->log("Login successful!");
            else
                cvarManager->log("Login failed!");
        });
    });
    h.connect(*cvarServer);
}

void MNCS::StopClient()
{
    h.close();
}
