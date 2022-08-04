#include "WebsocketManager.h"
#include "utils/parser.h"
#include "../Plugin/SOSUtils.h"
#include <ctime>

using nlohmann::json;

// PUBLIC FUNCTIONS //
WebsocketManager::WebsocketManager(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper)
    : cvarManager(cvarManager), gameWrapper(gameWrapper), ListenPort(8477) {

#ifdef PREFILL
    server = std::make_shared<std::string>(RELAY_SERVER);
    token = std::make_shared<std::string>(RELAY_PASSWORD);
    CVarWrapper cserver = cvarManager->registerCvar("sos_server", RELAY_SERVER, "Server to send events to", true);
    cvarManager->registerCvar("sos_token", RELAY_PASSWORD, "Token for authentication").bindTo(token);
#else
    string plServ = getServer();
    server = std::make_shared<std::string>(plServ);
    loggedIn = std::make_shared<bool>(false);
    CVarWrapper cserver = cvarManager->registerCvar("sos_server", plServ, "Server to send events to", true);
#endif
    cserver.bindTo(server);

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
    loadLine(SOS_DATADIR + LR"(\rocketcast\server)", &server);
    return server;
}

void WebsocketManager::setServer(string server) {
    ofstream file(SOS_DATADIR + LR"(\rocketcast\server)", ios_base::trunc);
    file << server << endl;
    file.close();
}

void WebsocketManager::StartServer()
{
    using namespace std::placeholders;

    LOGC("Starting WebSocket server");

    ws_connections = new ConnectionSet();
    ws_server = new PluginServer();

    LOG("Starting asio");
    ws_server->init_asio();
    ws_server->set_open_handler(websocketpp::lib::bind(&WebsocketManager::OnWsOpen, this, _1));
    ws_server->set_close_handler(websocketpp::lib::bind(&WebsocketManager::OnWsClose, this, _1));
    ws_server->set_message_handler(websocketpp::lib::bind(&WebsocketManager::OnWsMsg, this, _1, _2));
    ws_server->set_http_handler(websocketpp::lib::bind(&WebsocketManager::OnHttpRequest, this, _1));

    LOGC("Starting listen on port " + std::to_string(ListenPort));
    ws_server->listen(ListenPort);

    LOG("Starting accepting connections");
    ws_server->start_accept();
    ws_server->run();
}
void WebsocketManager::StopServer()
{
    LOGC("Stopping websocket server");

    if (ws_server)
    {
        ws_server->stop();
        ws_server->stop_listening();
        delete ws_server;
        ws_server = nullptr;
    }

    if (ws_connections)
    {
        ws_connections->clear();
        delete ws_connections;
        ws_connections = nullptr;
    }
}


// PRIVATE FUNCTIONS //
void WebsocketManager::OnWsMsg(connection_hdl hdl, PluginServer::message_ptr msg)
{
    auto out_message = msg->get_payload();

    if (out_message == "stop") {
        LOG("STOP [source: websocket]");
        ws_server->send(hdl, "STOPPING", websocketpp::frame::opcode::text);
        cvarManager->executeCommand("plugin unload rocketcast");
    } else if (out_message == "disconnect") {
        LOG("DISCONNECT [source: websocket]");
        ws_server->send(hdl, "DISCONNECT", websocketpp::frame::opcode::text);
        Disconnect();
    } else if (out_message.find("connect", 0) == 0) {
        std::vector<std::string> args = split(out_message, " ");
        if (args.size() >= 2) {
            Disconnect();
            Connect(args[1], args.size() >= 3 ? args[2] : "");
            ws_server->send(hdl, "CONNECT " + args[1], websocketpp::frame::opcode::text);
        }
        else {
            ws_server->send(hdl, "CONNECT_ERROR NOT_ENOUGH_ARGS", websocketpp::frame::opcode::text);
        }
    }
}

void WebsocketManager::OnHttpRequest(websocketpp::connection_hdl hdl)
{
    PluginServer::connection_ptr connection = ws_server->get_con_from_hdl(hdl);
    connection->append_header("Content-Type", "application/json");
    connection->append_header("Server", "SOS/" + std::string(SOS_VERSION));

    json data;
    data["message"] = "HTTP not supported at this time.";

    connection->set_body(DumpMessage(data));
    connection->set_status(websocketpp::http::status_code::ok);
}

void WebsocketManager::SendWebSocketPayload(const json& jsawn)
{
    // broadcast to all connections
    try
    {
        for (const connection_hdl& it : *ws_connections)
        {
            ws_server->send(it, DumpMessage(jsawn), websocketpp::frame::opcode::text);
        }
    }
    catch (std::exception e)
    {
        LOGC("An error occured sending websocket event: " + std::string(e.what()));
    }
}

void WebsocketManager::OnWsOpen(websocketpp::connection_hdl hdl) {
    ws_connections->insert(hdl);

    json data;
    data["event"] = "sos:version";
    data["data"] = std::string(SOS_VERSION);
    ws_server->send(hdl, DumpMessage(data), websocketpp::frame::opcode::text);
}


void WebsocketManager::StartPrompt() {
    // Server Address -> Connect -> Password -> Save?
    TextInputModalWrapper server = gameWrapper->CreateTextInputModal("ROCKETCAST");
    server.SetTextInput(getServer(), 50, false, [&](std::string input, bool canceled) {
        if (!canceled) {
            Disconnect();

            if(input.length() <= 11) {
                StartPrompt();
                return;
            }

            // Remove trailing slash
            if(input[input.length()-1] == '/') {
                input = input.substr(0, input.length() - 1);
            }

            LOGC("[SOS-SocketIO] new server: " + input);
            Connect(input);
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

void WebsocketManager::Destroy() {
    gameWrapper->UnregisterDrawables();
    StopServer(); 
    Disconnect();
}

// Connects to the specified server with a given token. This will bypass webpage login.
void WebsocketManager::Connect(const std::string& serv, const std::string& token) {
    h.clear_con_listeners();
    h.clear_socket_listeners();
    h.set_reconnect_delay(1000);
    h.set_reconnect_delay_max(5000);
    h.set_reconnect_attempts(2);
    h.set_reconnecting_listener([&, serv]() {
        LOGC("Attempting to reconnect to '" + serv + "'...");
        *loggedIn = false;
        });
    h.set_fail_listener([&]() {
        LOGC("Socket failed.");
        *loggedIn = false;
    });
    h.set_open_listener([&, serv, token]() {
        *loggedIn = false;
        LOGC("Socket connected");

        sio::message::list args = sio::message::list("PLUGIN");

        // Use computer name as name for relay
        char* comp_name = getenv("COMPUTERNAME");
        LOG("Computer name: " + std::string(comp_name));
        args.push(std::string(comp_name == 0 ? "RL_Bakkesmod" : comp_name));

        h.socket()->on("logged_in", [&](const std::string& name, sio::message::ptr const& message, bool need_ack, sio::message::list& ack_message) {
            *loggedIn = true;
            h.set_reconnect_attempts(3600);
        });

        h.socket()->emit("login", args);
    });
    h.set_close_listener([&](int const& reason) {
        *loggedIn = false;
        LOGC("Socket closed");
    });
    *server = serv;
    LOGC("Attempting to connect to '" + serv + "'...");
    h.connect(serv);
}

// Disconnects from the current server.
void WebsocketManager::Disconnect() {
    LOGC("Disconnecting from server.");
    h.socket()->off_all();
    h.clear_con_listeners();
    h.clear_socket_listeners();
    h.set_reconnect_attempts(0);
    h.set_reconnect_delay(0);
    clock_t time_req = clock();
    h.sync_close();
    time_req = clock() - time_req;
    LOGC("Disconnected. (" + std::to_string(((float)time_req / CLOCKS_PER_SEC)) + "s)");
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

void WebsocketManager::log(string text, bool console) {
    if(!unloading) {
        ofstream file(SOS_DATADIR + LR"(\rocketcast\log.txt)", ios_base::app);

        time_t now = time(0);
        tm* ltm = localtime(&now);
        string datestring = to_string(1 + ltm->tm_mon) + "/" + to_string(ltm->tm_mday) + "/" + to_string(1900 + ltm->tm_year) + " " + to_string(ltm->tm_hour) + ":" + to_string(ltm->tm_min) + ":" + to_string(ltm->tm_sec);

        file << "["+ datestring + "] [PLUGIN] " + text << endl;
        file.close();
        if (console) {
            cvarManager->log(text);
        }
    } else {
        cvarManager->log(text);
    }
}

json::string_t WebsocketManager::DumpMessage(json jsonData)
{
    json::string_t Output = jsonData.dump(-1, ' ', false, nlohmann::detail::error_handler_t::replace);
    //cvarManager->log(Output);
    return Output;
}
