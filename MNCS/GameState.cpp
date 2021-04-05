#include "MNCS.h"
#include "json.hpp"
#include "utils/parser.h"

/*
    GetGameTimeInfo needs to mesh better with the functions in Clock.cpp
*/

ServerWrapper MNCS::GetCurrentGameState()
{
    if(gameWrapper->IsInReplay())
		return gameWrapper->GetGameEventAsReplay().memory_address;
	else if(gameWrapper->IsInOnlineGame())
		return gameWrapper->GetOnlineGame();
	else
		return gameWrapper->GetGameEventAsServer();
}

void MNCS::UpdateGameState()
{
    json::JSON state;
    state["event"] = "gamestate";
    state["players"] = json::Object();
    state["game"] = json::Object();
    state["hasGame"] = false;

    if (GetGameStateInfo(state))
    {
        SendEvent("game:update_state", state);
    }
    else
    {
        LOGC("No Game");
    }
}

bool MNCS::GetGameStateInfo(json::JSON& state)
{    
    if(!ShouldRun()) { return false; }
    ServerWrapper server = GetCurrentGameState();

    //Get info
    ArrayWrapper<PriWrapper> PRIs = server.GetPRIs();
    for (int i = 0; i < PRIs.Count(); ++i)
    {
        PriWrapper pri = PRIs.Get(i);
        if (pri.IsNull()) { continue; }
        if (pri.IsSpectator() || pri.GetTeamNum() == 255) { continue; }
        GetPlayerInfo(state, pri);
    }
    GetTeamInfo(state, server);
    GetGameTimeInfo(state, server);
    GetBallInfo(state, server);
    GetWinnerInfo(state, server);
    GetArenaInfo(state);
    GetCameraInfo(state);

    state["hasGame"] = true;

    return true;
}

void MNCS::GetNameAndID(PriWrapper PRI, std::string &name, std::string &ID)
{
    //Use this whenever you need a player's name and ID in a JSON object
    if (PRI.IsNull())
    {
        name = "";
        ID = "";
    }
    else 
    {
        name = PRI.GetPlayerName().IsNull() ? "" : PRI.GetPlayerName().ToString();
        ID = name + '_' + std::to_string(PRI.GetSpectatorShortcut());
    }
}

void MNCS::GetPlayerInfo(json::JSON& state, PriWrapper pri)
{
    std::string name, id;
    GetNameAndID(pri, name, id);

    state["players"][id] = json::Object();

    state["players"][id]["name"] = name;
    state["players"][id]["id"] = id;
    state["players"][id]["primaryID"] = std::to_string(pri.GetUniqueIdWrapper().GetUID()); // TODO: Needs to be replaced once UniqueIDWrapper gets better sorted out
    state["players"][id]["shortcut"] = pri.GetSpectatorShortcut();
    state["players"][id]["team"] = pri.GetTeamNum();
    state["players"][id]["score"] = pri.GetMatchScore();
    state["players"][id]["goals"] = pri.GetMatchGoals();
    state["players"][id]["shots"] = pri.GetMatchShots();
    state["players"][id]["assists"] = pri.GetMatchAssists();
    state["players"][id]["saves"] = pri.GetMatchSaves();
    state["players"][id]["touches"] = pri.GetBallTouches();
    state["players"][id]["carTouches"] = pri.GetCarTouches();
    state["players"][id]["demos"] = pri.GetMatchDemolishes();

    CarWrapper car = pri.GetCar();

    //Car is null, return
    if (car.IsNull())
    {
        state["players"][id]["hasCar"] = false;
        state["players"][id]["speed"] = 0;
        state["players"][id]["boost"] = 0;
        state["players"][id]["isSonic"] = false;

        state["players"][id]["isDead"] = false;
        state["players"][id]["attacker"] = "";

		return;
	}

	Vector carLocation = car.GetLocation();
	state["players"][id]["x"] = carLocation.X;
	state["players"][id]["y"] = carLocation.Y;
	state["players"][id]["z"] = carLocation.Z;

	Rotator carRotation = car.GetRotation();
	state["players"][id]["roll"] = carRotation.Roll;
	state["players"][id]["pitch"] = carRotation.Pitch;
	state["players"][id]["yaw"] = carRotation.Yaw;

	state["players"][id]["onWall"] = car.IsOnWall();
	state["players"][id]["onGround"] = car.IsOnGround();

	// Check if player is powersliding
	ControllerInput controller = car.GetInput();
	state["players"][id]["isPowersliding"] = controller.Handbrake && car.IsOnGround();

    if (car.GetbHidden())
    {
        state["players"][id]["isDead"] = true;
        state["players"][id]["attacker"] = "";

        PriWrapper att = car.GetAttackerPRI(); // Attacker is only set on local player???
        if (!att.IsNull())
        {
            std::string attName, attID;
            GetNameAndID(pri, attName, attID);
            state["players"][id]["attacker"] = attID;
        }
    }
    else
    {
        state["players"][id]["isDead"] = false;
        state["players"][id]["attacker"] = "";
    }

    float boost = car.GetBoostComponent().IsNull() ? 0 : car.GetBoostComponent().GetPercentBoostFull();

    state["players"][id]["hasCar"] = true;
    state["players"][id]["speed"] = static_cast<int>(ToKPH(car.GetVelocity().magnitude()) + .5f);
    state["players"][id]["boost"] = static_cast<int>(boost * 100);
    state["players"][id]["isSonic"] = car.GetbSuperSonic() ? true : false;
}

void MNCS::GetTeamInfo(json::JSON& state, ServerWrapper server)
{
    //Not enough teams
    if (server.GetTeams().Count() != 2)
    {
        state["game"]["teams"][0]["name"] = "BLUE";
        state["game"]["teams"][0]["score"] = 0;
        state["game"]["teams"][1]["name"] = "ORANGE";
        state["game"]["teams"][1]["score"] = 0;
        return;
    }

    TeamWrapper team0 = server.GetTeams().Get(0);
    if (!team0.IsNull())
    {
        state["game"]["teams"][0]["name"] = team0.GetCustomTeamName().IsNull() ? "BLUE" : team0.GetCustomTeamName().ToString();
        state["game"]["teams"][0]["score"] = team0.GetScore();
    }
    else
    {
        state["game"]["teams"][0]["name"] = "BLUE";
        state["game"]["teams"][0]["score"] = 0;
    }

    TeamWrapper team1 = server.GetTeams().Get(1);
    if (!team1.IsNull())
    {
        state["game"]["teams"][1]["name"] = team1.GetCustomTeamName().IsNull() ? "ORANGE" : team1.GetCustomTeamName().ToString();
        state["game"]["teams"][1]["score"] = team1.GetScore();
    }
    else
    {
        state["game"]["teams"][1]["name"] = "ORANGE";
        state["game"]["teams"][1]["score"] = 0;
    }
}

void MNCS::GetGameTimeInfo(json::JSON& state, ServerWrapper server)
{
    //Get the time difference between now and the last time the clock was updated (in UpdateClock() hook)
    //Use a static float so that the diff remains the same value if the clock is paused
    //localCachedDecimal is a terrible hack for making sure float time stays consistent when pausing/unpausing
    float localCachedDecimal = 0;
    if (!isClockPaused)
    {
        localCachedDecimal = cachedDecimalTimeOnPause;
        decimalTime = duration_cast<duration<float>>(steady_clock::now() - timeSnapshot).count();
    }

    //Add or subtract the time difference to/from the current time on the clock. +/- depends on overtime status
    float floatTime = 0;
    if (!server.GetbOverTime())
    {
        floatTime = (float)server.GetSecondsRemaining() - decimalTime - localCachedDecimal;
    }
    else
    {
        floatTime = (float)server.GetSecondsRemaining() + decimalTime + localCachedDecimal;
    }

    //Zero out time after game ends because UpdateClock() isn't called again
    if (floatTime < 0 || waitingForOvertimeToStart) floatTime = 0;

    state["game"]["time"] = !firstCountdownHit ? 300.f : (bool)server.GetbOverTime() ? server.GetSecondsRemaining() : floatTime;
    state["game"]["isOT"] = (bool)server.GetbOverTime();

    if (gameWrapper->IsInReplay())
    {
        state["game"]["frame"] = gameWrapper->GetGameEventAsReplay().GetCurrentReplayFrame();
        state["game"]["elapsed"] = gameWrapper->GetGameEventAsReplay().GetReplayTimeElapsed();
    }

    LOGC(std::to_string((float)server.GetSecondsRemaining() - decimalTime));
}

void MNCS::GetBallInfo(json::JSON& state, ServerWrapper server)
{
    BallWrapper ball = server.GetBall();

    //Ball is null
    if (ball.IsNull())
    {
        state["game"]["ballSpeed"] = 0;
        state["game"]["ballTeam"] = 255;
        state["game"]["isReplay"] = false;
        return;
    }

	//Get ball info
	state["game"]["ballSpeed"] = static_cast<int>(ballCurrentSpeed);
	state["game"]["ballTeam"] = ball.GetHitTeamNum();
	state["game"]["isReplay"] = ball.GetbReplayActor() ? true : false;

	//Get Ball Location
	Vector ballLocation = ball.GetLocation();
	state["game"]["ballX"] = ballLocation.X;
	state["game"]["ballY"] = ballLocation.Y;
	state["game"]["ballZ"] = ballLocation.Z;
}

void MNCS::GetCurrentBallSpeed()
{
    //This function is called by HookViewportClientTick event
    if (bLockBallSpeed) { return; }
    if (!ShouldRun()) { return; }
    ServerWrapper server = GetCurrentGameState();
    if (server.IsNull()) { return; }
    BallWrapper ball = server.GetBall();
    if (ball.IsNull()) { return; }

    ballCurrentSpeed = ToKPH(ball.GetVelocity().magnitude()) + .5f;
}

void MNCS::GetWinnerInfo(json::JSON& state, ServerWrapper server)
{
    TeamWrapper winner = server.GetGameWinner();

    //Winning team is null
    if (winner.IsNull())
    {
        state["game"]["hasWinner"] = false;
        state["game"]["winner"] = "";
        return;
    }

    //Get the winning team
    state["game"]["hasWinner"] = true;
    state["game"]["winner"] = winner.GetCustomTeamName().IsNull() ? "" : winner.GetCustomTeamName().ToString();
}

void MNCS::GetArenaInfo(json::JSON& state)
{
    state["game"]["arena"] = gameWrapper->GetCurrentMap();
}

void MNCS::GetCameraInfo(json::JSON& state)
{
    CameraWrapper cam = gameWrapper->GetCamera();

    //Camera is null
    if (cam.IsNull())
    {
        state["game"]["hasTarget"] = false;
        state["game"]["target"] = "";
        return;
    }

    PriWrapper specPri = PriWrapper(reinterpret_cast<std::uintptr_t>(cam.GetViewTarget().PRI));

    //Target PRI is null
    if (specPri.IsNull())
    {
        state["game"]["hasTarget"] = false;
        state["game"]["target"] = "";
        return;
    }

    //Target PRI is the local player
    if (specPri.IsLocalPlayerPRI())
    {
        state["game"]["hasTarget"] = false;
        state["game"]["target"] = "";
        return;
    }

    //Get the target's name
    std::string targetName, targetID;
    GetNameAndID(specPri, targetName, targetID);
    state["game"]["hasTarget"] = true;
    state["game"]["target"] = targetID;
}

void MNCS::GetLastTouchInfo(CarWrapper car)
{
    //This function is called by the HookCarBallHit event
    if (!ShouldRun()) { return; }
    if (car.IsNull()) { return; }
    PriWrapper PRI = car.GetPRI();
    if (PRI.IsNull()) { return; }
    ServerWrapper server = GetCurrentGameState();
    if (server.IsNull()) { return; }
    BallWrapper ball = server.GetBall();
    if (ball.IsNull()) { return; }

    std::string playerName, playerID;
    GetNameAndID(PRI, playerName, playerID);

    lastTouch.speed = ToKPH(ball.GetVelocity().magnitude()) + .5f;
    lastTouch.playerID = playerID;
}
