#include "ReplayManager.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "utils/io.h"

#include <string>

Match backupMatchForReplayName;
string backupPlayerSteamID = "";


#pragma region Utility Functions

string GetPlaylistName(int playlistId) {
	switch (playlistId) {
	case(1):
		return "Casual Duel";
		break;
	case(2):
		return "Casual Doubles";
		break;
	case(3):
		return "Casual Standard";
		break;
	case(4):
		return "Casual Chaos";
		break;
	case(6):
		return "Private";
		break;
	case(10):
		return "Ranked Duel";
		break;
	case(11):
		return "Ranked Doubles";
		break;
	case(12):
		return "Ranked Solo Standard";
		break;
	case(13):
		return "Ranked Standard";
		break;
	case(14):
		return "Mutator Mashup";
		break;
	case(22):
		return "Tournament";
		break;
	case(27):
		return "Ranked Hoops";
		break;
	case(28):
		return "Ranked Rumble";
		break;
	case(29):
		return "Ranked Dropshot";
		break;
	case(30):
		return "Ranked Snowday";
		break;
	default:
		return "";
		break;
	}
}

#pragma endregion

void Log(void* object, string message)
{
	auto plugin = (ReplayManager*)object;
	plugin->cvarManager->log(message);
}

void save(wstring filename, string data) {
	ofstream file(filename, ios_base::trunc);
	file << data << endl;
	file.close();
}

// If the file is valid, loadLine will load the first line into *out.
void loadLine(wstring filename, string* out);

void BallchasingUploadComplete(void* object, bool result, string response)
{
	if (result == true) {
		auto manager = (ReplayManager*)object;
		json res = json::parse(response);
		manager->cvarManager->log(res.at("location").get<std::string>());
		manager->gameWrapper->Execute([&, manager, res](GameWrapper* wrap) {
			manager->Websocket->SendEvent("game:replay_uploaded", res);
		});
	}
}

void BallchasingAuthTestComplete(void* object, bool result)
{
	auto plugin = (ReplayManager*)object;
	string msg = result ? "Auth key correct!" : "Invalid auth key!";
	plugin->cvarManager->log(msg);
}

ReplayManager::ReplayManager(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper, std::shared_ptr<WebsocketManager> Websocket)
    : cvarManager(cvarManager), gameWrapper(gameWrapper), Websocket(Websocket) {

	stringstream userAgentStream;
	userAgentStream << "ReplayManager/1.0.1" << " BakkesModAPI/" << BAKKESMOD_PLUGIN_API_VERSION;
	string userAgent = userAgentStream.str();

	// Setup upload handlers
	ballchasing = new Ballchasing(userAgent, &Log, &BallchasingUploadComplete, &BallchasingAuthTestComplete, this);

	string key = "";
	loadLine(SOS_DATADIR + LR"(\sosio\ballchasing_key)", &key);
	ballchasing->authKey = std::make_shared<std::string>(key);
	cvarManager->registerCvar("sos_ballchasing_key", key, "Token for ballchasing authentication").bindTo(ballchasing->authKey);
	
	ballchasing->visibility = std::make_shared<std::string>("unlisted");
	cvarManager->registerCvar("sos_ballchasing_visibility", "unlisted", "Default visibility to use when uploading replays to ballchasing").bindTo(ballchasing->visibility);

	gameWrapper->HookEventWithCaller<ServerWrapper>(
		"Function GameEvent_Soccar_TA.Active.StartRound",
		bind(
			&ReplayManager::GetPlayerData,
			this,
			placeholders::_1,
			placeholders::_2,
			placeholders::_3
		)
		);

	// Register for Game ending event	
	gameWrapper->HookEventWithCaller<ServerWrapper>(
		"Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
		bind(
			&ReplayManager::OnGameComplete,
			this,
			placeholders::_1,
			placeholders::_2,
			placeholders::_3
		)
		);
}

void ReplayManager::onUnload() {
	save(SOS_DATADIR + LR"(\sosio\ballchasing_key)", *ballchasing->authKey);
	delete ballchasing;
}

Player ConstructPlayer(PriWrapper wrapper)
{
	Player p;
	if (!wrapper.IsNull())
	{
		p.Name = wrapper.GetPlayerName().ToString();
		p.UniqueId = wrapper.GetUniqueIdWrapper().GetUID();
		p.Team = wrapper.GetTeamNum();
		p.Score = wrapper.GetScore();
		p.Goals = wrapper.GetMatchGoals();
		p.Assists = wrapper.GetMatchAssists();
		p.Saves = wrapper.GetMatchSaves();
		p.Shots = wrapper.GetMatchShots();
		p.Demos = wrapper.GetMatchDemolishes();
	}
	return p;
}

void ReplayManager::OnGameComplete(ServerWrapper caller, void* params, std::string eventName)
{
	cvarManager->log("Uploading replay started: " + eventName);

	// Get ReplayDirector
	ReplayDirectorWrapper replayDirector = caller.GetReplayDirector();
	if (replayDirector.IsNull())
	{
		cvarManager->log("Could not upload replay, director is NULL!");
		return;
	}

	// Get Replay wrapper
	ReplaySoccarWrapper soccarReplay = replayDirector.GetReplay();
	if (soccarReplay.memory_address == NULL)
	{
		cvarManager->log("Could not upload replay, replay is NULL!");
		return;
	}
	soccarReplay.StopRecord();

	// If we have a template for the replay name then set the replay name based off that template else use default template
	string replayName = SetReplayName(caller, soccarReplay);


	// Export the replay to a file for upload
	string replayPath = ExportReplay(soccarReplay, replayName);

	// Upload replay
	cvarManager->log("uploading " + replayPath);
	ballchasing->UploadReplay(replayPath); 
	//calculated->UploadReplay(replayPath, "76561198247336622");

	// If we aren't saving the replay remove it after we've uploaded
	cvarManager->log("Removing replay file: " + replayPath);
	remove(replayPath.c_str());
}

std::string ReplayManager::SetReplayName(ServerWrapper& server, ReplaySoccarWrapper& soccarReplay)
{
	string replayName = *replayNameTemplate;
	cvarManager->log("Using replay name template: " + replayName);

	Match match;

	// Get Gamemode game was in
	auto playlist = server.GetPlaylist();
	if (playlist.memory_address != NULL)
	{
		match.GameMode = GetPlaylistName(playlist.GetPlaylistId());
	}
	// Get local primary player
	CarWrapper mycar = gameWrapper->GetLocalCar();
	if (!mycar.IsNull())
	{
		PriWrapper mycarpri = mycar.GetPRI();
		if (!mycarpri.IsNull())
		{
			match.PrimaryPlayer = ConstructPlayer(mycarpri);
		}
	}

	// If upload game was initiated by event Function TAGame.GameEvent_Soccar_TA.Destroyed
	// it is very likely that the primary player data can not be anymore fetched.
	// That's why we saved this data in Function GameEvent_Soccar_TA.Active.StartRound -event
	// and will use it, if needed, to get correct player for the game being uploaded.
	if (match.PrimaryPlayer.Name.length() < 1 && backupMatchForReplayName.PrimaryPlayer.Name.length() > 0)
	{
		cvarManager->log("Using prerecorder username for replay: " + backupMatchForReplayName.PrimaryPlayer.Name);
		match.PrimaryPlayer.Name = backupMatchForReplayName.PrimaryPlayer.Name;
		match.PrimaryPlayer.UniqueId = backupMatchForReplayName.PrimaryPlayer.UniqueId;
		match.PrimaryPlayer.Team = backupMatchForReplayName.PrimaryPlayer.Team;
	}

	// Get all players
	auto players = server.GetLocalPlayers();
	for (int i = 0; i < players.Count(); i++)
	{
		match.Players.push_back(ConstructPlayer(players.Get(i).GetPRI()));
	}

	// Get Team scores
	match.Team0Score = soccarReplay.GetTeam0Score();
	match.Team1Score = soccarReplay.GetTeam1Score();

	// Get current Sequence number
	auto seq = *templateSequence;

	replayName = ApplyNameTemplate(replayName, match, &seq);

	cvarManager->log("ReplayName: " + replayName);
	soccarReplay.SetReplayName(replayName);

	return replayName;
}

void ReplayManager::GetPlayerData(ServerWrapper caller, void* params, string eventName)
{
	// We are in online game and now storing some important player data, if needed after the game
	// When uploading the replay.
	backupPlayerSteamID = to_string(gameWrapper->GetSteamID());

	CarWrapper mycar = gameWrapper->GetLocalCar();
	if (!mycar.IsNull()) {
		PriWrapper mycarpri = mycar.GetPRI();
		if (!mycarpri.IsNull()) {
			backupMatchForReplayName.PrimaryPlayer = ConstructPlayer(mycarpri);
		}
	}

	cvarManager->log("StartRound: Stored userdata for:" + backupMatchForReplayName.PrimaryPlayer.Name);
}

std::string ReplayManager::ExportReplay(ReplaySoccarWrapper& soccarReplay, std::string replayName)
{
	std::string export_path = "./bakkesmod/data/";
	string replayPath = CalculateReplayPath(export_path, replayName);

	// Remove file if it already exists
	if (file_exists(replayPath))
	{
		cvarManager->log("Removing duplicate replay file: " + replayPath);
		remove(replayPath.c_str());
	}

	// Export Replay

	soccarReplay.ExportReplay(replayPath);
	cvarManager->log("Exported replay to: " + replayPath);

	// Check to see if replay exists, if not then export to default path
	if (!file_exists(replayPath))
	{
		cvarManager->log("Export failed to path: " + replayPath + " exporting to default path.");
		replayPath = string(export_path) + "autosaved.replay";

		soccarReplay.ExportReplay(replayPath);
		cvarManager->log("Exported replay to: " + replayPath);
	}

	return replayPath;
}