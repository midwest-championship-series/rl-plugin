#include "MNCS.h"

/*
    This plugin implements SOS (https://gitlab.com/bakkesplugins/sos/sos-plugin)

    Modified by Ben Nylund (https://github.com/bnylund)
*/


BAKKESMOD_PLUGIN(MNCS, "MNCS Overlay System", "1.0.0", PLUGINTYPE_THREADED)

void MNCS::onLoad()
{
#ifdef USE_TLS
    cvarManager->log("Loading MNCS Plugin --- TLS ENABLED");
#else
    cvarManager->log("MNCS Plugin --- TLS DISABLED");
#endif
    //Cvars
    cvarEnabled = std::make_shared<bool>(false);
    cvarUseBase64 = std::make_shared<bool>(false);
    cvarServer = std::make_shared<std::string>("http://localhost:5555");
    cvarToken = std::make_shared<std::string>("testtoken");
    cvarUpdateRate = std::make_shared<float>(50.0f);

    CVarWrapper registeredEnabledCvar = cvarManager->registerCvar("mncs_enabled", "1", "Enable MNCS plugin", true, true, 0, true, 1);
    registeredEnabledCvar.bindTo(cvarEnabled);
    registeredEnabledCvar.addOnValueChanged(std::bind(&MNCS::OnEnabledChanged, this));

    cvarManager->registerCvar("mncs_token", "testtoken", "Token for authentication").bindTo(cvarToken);
    
    cvarManager->registerCvar("mncs_use_base64", "0", "Use base64 encoding to send websocket info (useful for non ASCII characters)", true, true, 0, true, 1).bindTo(cvarUseBase64);
    CVarWrapper server = cvarManager->registerCvar("mncs_server", "http://localhost:5555", "Server to send events to", true);
    server.bindTo(cvarServer);
    server.addOnValueChanged([this](std::string s, CVarWrapper c) {
        // Stop websocket client then connect to new server
    });

    cvarManager->registerCvar("mncs_state_flush_rate", "50", "Rate at which to send events to websocket (milliseconds)", true, true, 5.0f, true, 2000.0f).bindTo(cvarUpdateRate);

    cvarManager->registerNotifier("mncs_reset_internal_state", [this](std::vector<std::string> params) { HookMatchEnded(); }, "Reset internal state", PERMISSION_ALL);

    cvarManager->registerNotifier("mncs_authenticate", [this](std::vector<std::string> params) {
        if (h.opened()) {
            cvarManager->log("Sent login...");
            h.socket()->emit("login", *cvarToken, [&](const sio::message::list& list) {
                if (list[0]->get_map()["status"]->get_string() == "good")
                    cvarManager->log("Login successful!");
                else
                    cvarManager->log("Login failed!");
                });
        }
        else {
            cvarManager->log("Websocket not open!");
        }
    }, "Authenticate websocket", PERMISSION_ALL);

    cvarManager->registerNotifier("mncs_connect", [this](std::vector<std::string> args) {
        if (args.size() >= 2) {
            if (h.opened()) {
                cvarManager->log("Closing previous connection");
                h.close();
            }
            cvarManager->log("Attempting to connect to " + args[1]);
            h.set_reconnect_delay(2000);
            h.set_reconnect_delay_max(3000);
            h.set_reconnecting_listener([&]() {
                cvarManager->log("Attempting to reconnect to '" + args[1] + "'...");
            });
            h.set_open_listener([&]() {
                cvarManager->log("Connected to " + args[1] + "! Attempting to log in...");
                h.socket()->emit("login", *cvarToken, [&](const sio::message::list& list) {
                    if (list[0]->get_map()["status"]->get_string() == "good")
                        cvarManager->log("Login successful!");
                    else
                        cvarManager->log("Login failed!");
                    });
            });
            cvarManager->getCvar("mncs_server").setValue(args[1]);
            h.connect(args[1]);
        }
    }, "Connect to server", PERMISSION_ALL);

    HookAllEvents();

    gameWrapper->SetTimeout([this](GameWrapper* gw)
    {
        if(ShouldRun())
        {
            HookMatchCreated();
        }
    }, 1.f);

    #ifdef USE_NAMEPLATES
    gameWrapper->RegisterDrawable(std::bind(&MNCS::GetNameplateInfo, this, _1));
    #endif

    RunClient();
}

void MNCS::onUnload()
{
    StopClient();

    int* i = 0;
    delete i;
    *i = 2;
}

void MNCS::OnEnabledChanged()
{
    //If mod has been disabled, stop any potentially remaining clock calculations
    if (!*cvarEnabled)
    {
        isClockPaused = true;
        newRoundActive = false;
    }
}

bool MNCS::ShouldRun()
{
    //Check if player is spectating
    if (!gameWrapper->GetLocalCar().IsNull())
    {
        LOGC("GetLocalCar().IsNull(): (need true) false");
        return false;
    }

    //Check if server exists
    ServerWrapper server = GetCurrentGameState();
    if (server.IsNull())
    {
        LOGC("server.IsNull(): (need false) true");
        return false;
    }

    //Check if server playlist exists
    if (server.GetPlaylist().memory_address == NULL)
    {
        LOGC("server.GetPlaylist().memory_address == NULL: (need false) true");
        return false;
    }

    //Check if server playlist is valid
    // 6:  Private Match
    // 22: Custom Tournaments
    // 24: LAN Match
    static const std::vector<int> SafePlaylists = {6, 22, 24};
    int playlistID = server.GetPlaylist().GetPlaylistId();
    if (!IsSafeMode(playlistID, SafePlaylists))
    {
        std::string NotSafeMessage;
        
        #if SHOULDLOG //Don't constantly compile the message unless it's going to be printed
        NotSafeMessage += "server.GetPlaylist().GetPlaylistId(): (need ";
        
        //Add list of safe modes to string
        for(const auto& Mode : SafeModes)
        {
            NotSafeMessage += std::to_string(Mode) + ", ";
        }

        //Remove last ", "
        NotSafeMessage.pop_back();
        NotSafeMessage.pop_back();

        NotSafeMessage += ") " + std::to_string(playlistID);
        #endif

        LOGC(NotSafeMessage);
        return false;
    }

    return true;
}

bool MNCS::IsSafeMode(int CurrentPlaylist, const std::vector<int>& SafePlaylists)
{
    for(const auto& SafePlaylist : SafePlaylists)
    {
        if (CurrentPlaylist == SafePlaylist)
        {
            return true;
        }
    }

    return false;
}
