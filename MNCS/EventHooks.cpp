#include "MNCS.h"
#include "json.hpp"
#include "bakkesmod/wrappers/GameObject/Stats/StatEventWrapper.h"

/*
    Many of these hooks need to have `if (!*cvarEnabled) { return; }` added
    But I'm not sure which
*/

void MNCS::HookAllEvents()
{
    //UPDATE GAMESTATE EVERY TICK
    gameWrapper->HookEvent("Function Engine.GameViewportClient.Tick", std::bind(&MNCS::HookViewportTick, this));

    //FLOAT CLOCK EVENTS (Clock.cpp)
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnGameTimeUpdated", std::bind(&MNCS::UpdateClock, this));
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.StartOvertime", std::bind(&MNCS::PauseClockOnOvertimeStarted, this));
    gameWrapper->HookEvent("Function TAGame.Car_TA.OnHitBall", std::bind(&MNCS::UnpauseClockOnBallTouch, this));
    gameWrapper->HookEvent("Function Engine.WorldInfo.EventPauseChanged", std::bind(&MNCS::OnPauseChanged, this));

    //GAME EVENTS
    gameWrapper->HookEventPost("Function TAGame.Team_TA.PostBeginPlay", std::bind(&MNCS::HookInitTeams, this));
    gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.Destroyed", std::bind(&MNCS::HookMatchDestroyed, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.Countdown.BeginState", std::bind(&MNCS::HookCountdownInit, this));
    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode", std::bind(&MNCS::HookBallExplode, this));
    gameWrapper->HookEventWithCaller<BallWrapper>("Function TAGame.Ball_TA.OnHitGoal", std::bind(&MNCS::HookOnHitGoal, this, _1, _2, _3));
    gameWrapper->HookEvent("Function TAGame.GameInfo_Replay_TA.InitGame", std::bind(&MNCS::HookReplayCreated, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.ReplayPlayback.BeginState", std::bind(&MNCS::HookReplayStart, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.ReplayPlayback.EndState", std::bind(&MNCS::HookReplayEnd, this));
    gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", std::bind(&MNCS::HookMatchEnded, this));
    gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.PodiumSpotlight.BeginState", std::bind(&MNCS::HookPodiumStart, this));


    //STATEVENT FEED
    gameWrapper->HookEventWithCaller<ServerWrapper>("Function TAGame.PRI_TA.ClientNotifyStatTickerMessage", std::bind(&MNCS::HookStatEvent, this, _1, _2));

    //LAST TOUCH INFO
    gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.EventHitBall", std::bind(&MNCS::HookCarBallHit, this, _1, _2, _3));
}

void MNCS::HookViewportTick()
{
    if (!*cvarEnabled) { return; }

    //Clamp number of events per second
    static steady_clock::time_point lastCallTime = steady_clock::now(); // this line only fires first time function is called
    float timeSinceLastCall = duration_cast<duration<float>>(steady_clock::now() - lastCallTime).count();

    //MAIN FUNCTION (GameState.cpp)
    if (timeSinceLastCall >= (*cvarUpdateRate / 1000.f))
    {
        UpdateGameState();
        lastCallTime = steady_clock::now();
    }
    else
    {
        //This spams the log too much
        //LOGC(std::to_string(timeSinceLastCall) + " - " + std::to_string((*cvarUpdateRate / 1000.f)) + " - Too early to send gamestate update");
    }
    
    //Get ball speed every tick (for goal speed)
    GetCurrentBallSpeed();
}

void MNCS::HookInitTeams()
{
    static int NumTimesCalled = 0;

    //"Function TAGame.Team_TA.PostBeginPlay" is called twice rapidly, once for each team
    // Only initialize lobby on the second hook once both teams are ready

    ++NumTimesCalled;
    if(NumTimesCalled >= 2)
    {
        //Set a delay so that everything can be filled in before trying to initialize
        gameWrapper->SetTimeout([this](GameWrapper* gw)
        {
            if(ShouldRun())
            {
                HookMatchCreated();
            }
        }, .05f);
        
        NumTimesCalled = 0;
    }

    //Reset call counter after 2 seconds in case it never got through the >= 2 check
    if(NumTimesCalled != 0)
    {
        gameWrapper->SetTimeout([this](GameWrapper* gw){ NumTimesCalled = 0; }, 2.f);
    }
}

void MNCS::HookMatchCreated()
{
    LOGC(" -------------- MATCH CREATED -------------- ");

    isClockPaused = true;
    matchCreated = true;
    decimalTime = 0;
    SendEvent("game:match_created", "game_match_created");
}

void MNCS::HookMatchDestroyed()
{
    isInReplay = false;
    matchCreated = false;
    firstCountdownHit = false;
    isCurrentlySpectating = false;
    isClockPaused = true;

    SendEvent("game:match_destroyed", "game_match_destroyed");
}

void MNCS::HookCountdownInit()
{
    if (!firstCountdownHit && ShouldRun())
    {
        firstCountdownHit = true;
        SendEvent("game:initialized", "initialized");
    }

    SendEvent("game:pre_countdown_begin", "pre_game_countdown_begin");
    SendEvent("game:post_countdown_begin", "post_game_countdown_begin");
}

void MNCS::HookBallExplode()
{
    if (!*cvarEnabled || !matchCreated) { return; }

    LockBallSpeed();
    PauseClockOnGoal();
    HookReplayWillEnd();
}

void MNCS::HookOnHitGoal(BallWrapper ball, void* params, std::string funcName)
{
    LockBallSpeed();
    goalSpeed = ballCurrentSpeed;

    GoalImpactLocation = GetGoalImpactLocation(ball, params, funcName);
}

void MNCS::HookReplayCreated()
{
    //No state
    isClockPaused = true;
    matchCreated = true;
    decimalTime = 0;
    SendEvent("game:replay_created", "game_replay_created");
}

void MNCS::HookReplayStart()
{
    isInReplay = true;
    SendEvent("game:replay_start", "game_replay_start");
}

void MNCS::HookReplayWillEnd()
{
    //This function is called by the HookBallExplode event
    if (isInReplay)
    {
        LOGC("Sending ReplayWillEnd Event");
        SendEvent("game:replay_will_end", "game_replay_will_end");
    }
}

void MNCS::HookReplayEnd()
{
    isInReplay = false;
    SendEvent("game:replay_end", "game_replay_end");
}

void MNCS::HookMatchEnded()
{
    isInReplay = false;
    matchCreated = false;
    firstCountdownHit = false;
    isCurrentlySpectating = false;
    isClockPaused = true;

    json::JSON winnerData;
    winnerData["winner_team_num"] = NULL;

    ServerWrapper server = GetCurrentGameState();
    if (!server.IsNull())
    {
        TeamWrapper winner = server.GetMatchWinner();
        if (!winner.IsNull())
        {
            winnerData["winner_team_num"] = winner.GetTeamNum();
        }
    }

    SendEvent("game:match_ended", winnerData);
}

void MNCS::HookPodiumStart()
{
    SendEvent("game:podium_start", "game_podium_start");
}

void MNCS::HookCarBallHit(CarWrapper car, void * params, std::string funcName)
{
    //Wait until next tick to get info.
    //Getting the info on the tick the hook was called is usually inaccurate.
    //(i.e. on kickoff the ball speed would be 0 even though the car has touched it)
    gameWrapper->SetTimeout(std::bind(&MNCS::GetLastTouchInfo, this, car), 0.01f);
}

void MNCS::HookStatEvent(ServerWrapper caller, void* args)
{
    auto tArgs = (DummyStatEventContainer*)args;

    auto statEvent = StatEventWrapper(tArgs->StatEvent);
    auto label = statEvent.GetLabel();
    auto eventStr = label.ToString();

    //Victim info
    auto victim = PriWrapper(tArgs->Victim);
    std::string victimName, victimID;
    GetNameAndID(victim, victimName, victimID);
    
    //Receiver info
    auto receiver = PriWrapper(tArgs->Receiver);
    std::string receiverName, receiverID;
    GetNameAndID(receiver, receiverName, receiverID); 

    //General statfeed event
    json::JSON statfeed;
    statfeed["type"] = eventStr;
    statfeed["main_target"]["name"] = receiverName;
    statfeed["main_target"]["id"] = receiverID;
    statfeed["secondary_target"]["name"] = victimName;
    statfeed["secondary_target"]["id"] = victimID;
    SendEvent("game:statfeed_event", statfeed);

    //Goal statfeed event
    if (eventStr == "Goal")
    {
        json::JSON goalScoreData;
        goalScoreData["goalspeed"] = goalSpeed;
        goalScoreData["impact_location"]["X"] = GoalImpactLocation.X;
        goalScoreData["impact_location"]["Y"] = GoalImpactLocation.Y;
        goalScoreData["scorer"]["name"] = receiverName;
        goalScoreData["scorer"]["id"] = receiverID;
        goalScoreData["scorer"]["teamnum"] = receiver.IsNull() ? -1 : receiver.GetTeamNum();
        goalScoreData["ball_last_touch"]["player"] = lastTouch.playerID;
        goalScoreData["ball_last_touch"]["speed"] = lastTouch.speed;
        SendEvent("game:goal_scored", goalScoreData);
    }
}
