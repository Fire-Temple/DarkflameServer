#include <iostream>
#include <string>
#include <ctime>
#include <chrono>
#include <thread>

#include "MD5.h"

//DLU Includes:
#include "dCommonVars.h"
#include "dServer.h"
#include "Logger.h"
#include "Database.h"
#include "dConfig.h"
#include "dpWorld.h"
#include "dZoneManager.h"
#include "Metrics.hpp"
#include "PerformanceManager.h"
#include "Diagnostics.h"
#include "BinaryPathFinder.h"
#include "dPlatforms.h"

//RakNet includes:
#include "RakNetDefines.h"
#include "RakNetworkFactory.h"
#include "RakString.h"

//World includes:
#include <csignal>

#include "AuthPackets.h"
#include "BitStreamUtils.h"
#include "WorldPackets.h"
#include "UserManager.h"
#include "CDClientManager.h"
#include "CDClientDatabase.h"
#include "GeneralUtils.h"
#include "ObjectIDManager.h"
#include "ZoneInstanceManager.h"
#include "dChatFilter.h"
#include "ClientPackets.h"
#include "CharacterComponent.h"
#include "ePlayerFlag.h"

#include "EntityManager.h"
#include "EntityInfo.h"
#include "User.h"
#include "Loot.h"
#include "Entity.h"
#include "Character.h"
#include "ChatPackets.h"
#include "GameMessageHandler.h"
#include "GameMessages.h"
#include "Mail.h"
#include "TeamManager.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"
#include "Game.h"
#include "MasterPackets.h"
#include "PropertyManagementComponent.h"
#include "AssetManager.h"
#include "LevelProgressionComponent.h"
#include "eBlueprintSaveResponseType.h"
#include "Amf3.h"
#include "NiPoint3.h"
#include "eServerDisconnectIdentifiers.h"
#include "eObjectBits.h"
#include "eConnectionType.h"
#include "MessageType/Server.h"
#include "MessageType/Chat.h"
#include "MessageType/World.h"
#include "MessageType/Master.h"
#include "MessageType/Game.h"
#include "ZCompression.h"
#include "EntityManager.h"
#include "CheatDetection.h"
#include "eGameMasterLevel.h"
#include "StringifiedEnum.h"
#include "Server.h"
#include "PositionUpdate.h"
#include "PlayerManager.h"
#include "eLoginResponse.h"
#include "MissionComponent.h"
#include "SlashCommandHandler.h"
#include "InventoryComponent.h"

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dChatFilter* chatFilter = nullptr;
	dConfig* config = nullptr;
	AssetManager* assetManager = nullptr;
	RakPeerInterface* chatServer = nullptr;
	std::mt19937 randomEngine;
	SystemAddress chatSysAddr;
	Game::signal_t lastSignal = 0;
	EntityManager* entityManager = nullptr;
	dZoneManager* zoneManager = nullptr;
	std::string projectVersion = PROJECT_VERSION;
} // namespace Game

namespace {
	struct TempSessionInfo {
		SystemAddress sysAddr;
		std::string hash;
	};

	std::map<std::string, TempSessionInfo> g_PendingUsers;
	uint32_t g_InstanceID = 0;
	uint32_t g_CloneID = 0;
	std::string g_DatabaseChecksum = "";

	bool g_ChatDisabled = false;
	bool g_ChatConnected = false;
	bool g_WorldShutdownSequenceComplete = false;
}; // namespace anonymous

void WorldShutdownSequence();
void WorldShutdownProcess(uint32_t zoneId);
void FinalizeShutdown();
void SendShutdownMessageToMaster();

void HandlePacketChat(Packet* packet);
void HandleMasterPacket(Packet* packet);
void HandlePacket(Packet* packet);

int main(int argc, char** argv) {
	Diagnostics::SetProcessName("World");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	// Triggers the shutdown sequence at application exit
	std::atexit(WorldShutdownSequence);

	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);

	uint32_t zoneID = 1000;
	uint32_t cloneID = 0;
	uint32_t maxClients = 8;
	uint32_t ourPort = 2007;

	//Check our arguments:
	for (int32_t i = 0; (i + 1) < argc; i++) {
		std::string argument(argv[i]);
		const auto valOptional = GeneralUtils::TryParse<uint32_t>(argv[i + 1]);
		if (!valOptional) {
			continue;
		}

		if (argument == "-zone") zoneID = valOptional.value_or(1000);
		else if (argument == "-instance") g_InstanceID = valOptional.value_or(0);
		else if (argument == "-clone") cloneID = valOptional.value_or(0);
		else if (argument == "-maxclients") maxClients = valOptional.value_or(8);
		else if (argument == "-port") ourPort = valOptional.value_or(2007);
	}

	Game::config = new dConfig("worldconfig.ini");

	//Create all the objects we need to run our service:
	Server::SetupLogger("WorldServer_" + std::to_string(zoneID) + "_" + std::to_string(g_InstanceID));
	if (!Game::logger) return EXIT_FAILURE;

	LOG("Starting World server...");
	LOG("Version: %s", Game::projectVersion.c_str());
	LOG("Compiled on: %s", __TIMESTAMP__);

	g_ChatDisabled = Game::config->GetValue("disable_chat") == "1";

	try {
		std::string clientPathStr = Game::config->GetValue("client_location");
		if (clientPathStr.empty()) clientPathStr = "./res";
		std::filesystem::path clientPath = std::filesystem::path(clientPathStr);
		if (clientPath.is_relative()) {
			clientPath = BinaryPathFinder::GetBinaryDir() / clientPath;
		}
		Game::assetManager = new AssetManager(clientPath);
	} catch (const std::exception& ex) {
		LOG("Got an error while setting up assets: %s", ex.what());

		return EXIT_FAILURE;
	}

	// Connect to CDClient
	try {
		CDClientDatabase::Connect((BinaryPathFinder::GetBinaryDir() / "resServer" / "CDServer.sqlite").string());
	} catch (const CppSQLite3Exception& e) {
		LOG("Unable to connect to CDServer SQLite Database");
		LOG("Error: %s", e.errorMessage());
		LOG("Error Code: %i", e.errorCode());
		return EXIT_FAILURE;
	} catch (const std::exception& e) {
		LOG("Caught generic exception %s when connecting to CDClient", e.what());
		return EXIT_FAILURE;
	}

	CDClientManager::LoadValuesFromDatabase();

	Diagnostics::SetProduceMemoryDump(Game::config->GetValue("generate_dump") == "1");

	if (!Game::config->GetValue("dump_folder").empty()) {
		Diagnostics::SetOutDirectory(Game::config->GetValue("dump_folder"));
	}

	//Connect to the MySQL Database:
	try {
		Database::Connect();
	} catch (const std::exception& ex) {
		LOG("Got an error while connecting to the database: %s", ex.what());
		return EXIT_FAILURE;
	}

	//Find out the master's IP:
	std::string masterIP = "localhost";
	uint32_t masterPort = 1000;
	std::string masterPassword;
	const auto masterInfo = Database::Get()->GetMasterInfo();

	if (masterInfo) {
		masterIP = masterInfo->ip;
		masterPort = masterInfo->port;
		masterPassword = masterInfo->password;
	}

	UserManager::Instance()->Initialize();

	const bool dontGenerateDCF = GeneralUtils::TryParse<bool>(Game::config->GetValue("dont_generate_dcf")).value_or(false);
	Game::chatFilter = new dChatFilter(Game::assetManager->GetResPath().string() + "/chatplus_en_us", dontGenerateDCF);

	Game::server = new dServer(masterIP,
		ourPort,
		g_InstanceID,
		maxClients,
		false /* Is internal */,
		true /* Use encryption */,
		Game::logger,
		masterIP,
		masterPort,
		ServerType::World,
		Game::config,
		&Game::lastSignal,
		masterPassword,
		zoneID);

	//Connect to the chat server:
	uint32_t chatPort = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("chat_server_port")).value_or(1501);

	auto chatSock = SocketDescriptor(static_cast<uint16_t>(ourPort + 2), NULL);
	Game::chatServer = RakNetworkFactory::GetRakPeerInterface();
	Game::chatServer->Startup(1, 30, &chatSock, 1);
	Game::chatServer->Connect(masterIP.c_str(), chatPort, NET_PASSWORD_EXTERNAL, strnlen(NET_PASSWORD_EXTERNAL, sizeof(NET_PASSWORD_EXTERNAL)));

	//Set up other things:
	Game::randomEngine = std::mt19937(time(0));

	//Run it until server gets a kill message from Master:
	auto lastTime = std::chrono::high_resolution_clock::now();
	auto t = std::chrono::high_resolution_clock::now();

	Packet* packet = nullptr;
	uint32_t framesSinceLastFlush = 0;
	uint32_t framesSinceMasterDisconnect = 0;
	uint32_t framesSinceChatDisconnect = 0;
	uint32_t framesSinceLastUsersSave = 0;
	uint32_t framesSinceLastSQLPing = 0;
	uint32_t framesSinceLastUser = 0;

	const float maxPacketProcessingTime = 1.5f; //0.015f;
	const uint32_t maxPacketsToProcess = 1024;

	bool ready = false;
	uint32_t framesSinceMasterStatus = 0;
	uint32_t framesSinceShutdownSequence = 0;
	uint32_t currentFramerate = highFramerate;

	uint32_t ghostingStepCount = 0;
	auto ghostingLastTime = std::chrono::high_resolution_clock::now();

	PerformanceManager::SelectProfile(zoneID);

	Game::entityManager = new EntityManager();
	Game::zoneManager = new dZoneManager();
	//Load our level:
	if (zoneID != 0) {
		dpWorld::Initialize(zoneID);
		Game::zoneManager->Initialize(LWOZONEID(zoneID, g_InstanceID, cloneID));
		g_CloneID = cloneID;
	} else {
		Game::entityManager->Initialize();
	}

	// pre calculate the FDB checksum
	if (Game::config->GetValue("check_fdb") == "1") {
		auto cdclient = Game::assetManager->GetFile("cdclient.fdb");
		if (cdclient) {

			const int32_t bufferSize = 1024;
			MD5 md5;

			char fileStreamBuffer[bufferSize] = {};

			while (!cdclient.eof()) {
				memset(fileStreamBuffer, 0, bufferSize);
				cdclient.read(fileStreamBuffer, bufferSize);
				md5.update(fileStreamBuffer, cdclient.gcount());
			}

			const char* nullTerminateBuffer = "\0";
			md5.update(nullTerminateBuffer, 1); // null terminate the data
			md5.finalize();
			g_DatabaseChecksum = md5.hexdigest();

			LOG("FDB Checksum calculated as: %s", g_DatabaseChecksum.c_str());
		}
		if (g_DatabaseChecksum.empty()) {
			LOG("check_fdb is on but no fdb file found.");
			return EXIT_FAILURE;
		}
	}

	uint32_t currentFrameDelta = highFrameDelta;
	// These values are adjust them selves to the current framerate should it update.
	uint32_t logFlushTime = 15 * currentFramerate; // 15 seconds in frames
	uint32_t shutdownTimeout = 10 * 60 * currentFramerate; // 10 minutes in frames
	uint32_t noMasterConnectionTimeout = 5 * currentFramerate; // 5 seconds in frames
	uint32_t chatReconnectionTime = 30 * currentFramerate; // 30 seconds in frames
	uint32_t saveTime = 10 * 60 * currentFramerate; // 10 minutes in frames
	uint32_t sqlPingTime = 10 * 60 * currentFramerate; // 10 minutes in frames
	uint32_t emptyShutdownTime = (cloneID == 0 ? 30 : 5) * 60 * currentFramerate; // 30 minutes for main worlds, 5 for all others.

	// Register slash commands if not in zone 0
	if (zoneID != 0) SlashCommandHandler::Startup();

	Game::logger->Flush(); // once immediately before the main loop
	while (true) {
		Metrics::StartMeasurement(MetricVariable::Frame);
		Metrics::StartMeasurement(MetricVariable::GameLoop);

		std::clock_t metricCPUTimeStart = std::clock();

		const auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
		lastTime = currentTime;

		const auto occupied = UserManager::Instance()->GetUserCount() != 0;

		uint32_t newFrameDelta = currentFrameDelta;
		if (!ready) {
			newFrameDelta = highFrameDelta;
		} else {
			newFrameDelta = PerformanceManager::GetServerFrameDelta();
		}

		// Update to the new framerate and scale all timings to said new framerate
		if (newFrameDelta != currentFrameDelta) {
			float_t ratioBeforeToAfter = static_cast<float>(currentFrameDelta) / static_cast<float>(newFrameDelta);
			currentFrameDelta = newFrameDelta;
			currentFramerate = MS_TO_FRAMES(newFrameDelta);
			LOG_DEBUG("Framerate for zone/instance/clone %i/%i/%i is now %i", zoneID, g_InstanceID, cloneID, currentFramerate);
			logFlushTime = 15 * currentFramerate; // 15 seconds in frames
			framesSinceLastFlush *= ratioBeforeToAfter;
			shutdownTimeout = 10 * 60 * currentFramerate; // 10 minutes in frames
			framesSinceLastUser *= ratioBeforeToAfter;
			noMasterConnectionTimeout = 5 * currentFramerate; // 5 seconds in frames
			framesSinceMasterDisconnect *= ratioBeforeToAfter;
			chatReconnectionTime = 30 * currentFramerate; // 30 seconds in frames
			framesSinceChatDisconnect *= ratioBeforeToAfter;
			saveTime = 10 * 60 * currentFramerate; // 10 minutes in frames
			framesSinceLastUsersSave *= ratioBeforeToAfter;
			sqlPingTime = 10 * 60 * currentFramerate; // 10 minutes in frames
			framesSinceLastSQLPing *= ratioBeforeToAfter;
			emptyShutdownTime = (cloneID == 0 ? 30 : 5) * 60 * currentFramerate; // 30 minutes for main worlds, 5 for all others.
			framesSinceLastUser *= ratioBeforeToAfter;
		}

		//Warning if we ran slow
		if (deltaTime > currentFrameDelta) {
			LOG("We're running behind, dT: %f > %i (framerate %i)", deltaTime, currentFrameDelta, currentFramerate);
		}

		//Check if we're still connected to master:
		if (!Game::server->GetIsConnectedToMaster()) {
			framesSinceMasterDisconnect++;

			if (framesSinceMasterDisconnect >= noMasterConnectionTimeout && !Game::ShouldShutdown()) {
				LOG("Game loop running but no connection to master for %d frames, shutting down", noMasterConnectionTimeout);
				Game::lastSignal = -1;
			}
		} else framesSinceMasterDisconnect = 0;

		// Check if we're still connected to chat:
		if (!g_ChatConnected) {
			framesSinceChatDisconnect++;

			if (framesSinceChatDisconnect >= chatReconnectionTime) {
				framesSinceChatDisconnect = 0;

				Game::chatServer->Connect(masterIP.c_str(), chatPort, NET_PASSWORD_EXTERNAL, strnlen(NET_PASSWORD_EXTERNAL, sizeof(NET_PASSWORD_EXTERNAL)));
			}
		} else framesSinceChatDisconnect = 0;

		//In world we'd update our other systems here.

		if (zoneID != 0 && deltaTime > 0.0f) {
			Metrics::StartMeasurement(MetricVariable::UpdateEntities);
			Game::entityManager->UpdateEntities(deltaTime);
			Metrics::EndMeasurement(MetricVariable::UpdateEntities);

			Metrics::StartMeasurement(MetricVariable::Physics);
			dpWorld::StepWorld(deltaTime);
			Metrics::EndMeasurement(MetricVariable::Physics);

			Metrics::StartMeasurement(MetricVariable::Ghosting);
			if (std::chrono::duration<float>(currentTime - ghostingLastTime).count() >= 1.0f) {
				Game::entityManager->UpdateGhosting();
				ghostingLastTime = currentTime;
			}
			Metrics::EndMeasurement(MetricVariable::Ghosting);

			Metrics::StartMeasurement(MetricVariable::UpdateSpawners);
			Game::zoneManager->Update(deltaTime);
			Metrics::EndMeasurement(MetricVariable::UpdateSpawners);
		}

		Metrics::StartMeasurement(MetricVariable::PacketHandling);

		//Check for packets here:
		packet = Game::server->ReceiveFromMaster();
		while (packet) { //We can get messages not handle-able by the dServer class, so handle them if we returned anything.
			HandleMasterPacket(packet);
			Game::server->DeallocateMasterPacket(packet);
			packet = Game::server->ReceiveFromMaster();
		}

		//Handle our chat packets:
		packet = Game::chatServer->Receive();
		while (packet) {
			HandlePacketChat(packet);
			Game::chatServer->DeallocatePacket(packet);
			packet = Game::chatServer->Receive();
		}

		//Handle world-specific packets:
		float timeSpent = 0.0f;

		UserManager::Instance()->DeletePendingRemovals();

		for (uint32_t curPacket = 0; curPacket < maxPacketsToProcess && timeSpent < maxPacketProcessingTime; curPacket++) {
			packet = Game::server->Receive();
			if (packet) {
				auto t1 = std::chrono::high_resolution_clock::now();
				HandlePacket(packet);
				auto t2 = std::chrono::high_resolution_clock::now();

				timeSpent += std::chrono::duration_cast<std::chrono::duration<float>>(t2 - t1).count();
				Game::server->DeallocatePacket(packet);
				packet = nullptr;
			} else {
				break;
			}
		}

		Metrics::EndMeasurement(MetricVariable::PacketHandling);

		Metrics::StartMeasurement(MetricVariable::UpdateReplica);

		//Update our replica objects:
		Game::server->UpdateReplica();

		Metrics::EndMeasurement(MetricVariable::UpdateReplica);

		//Push our log every 15s:
		if (framesSinceLastFlush >= logFlushTime) {
			Game::logger->Flush();
			framesSinceLastFlush = 0;
		} else framesSinceLastFlush++;

		if (zoneID != 0 && !occupied) {
			framesSinceLastUser++;

			//If we haven't had any players for a while, time out and shut down:
			if (framesSinceLastUser >= emptyShutdownTime) {
				Game::lastSignal = -1;
			}
		} else {
			framesSinceLastUser = 0;
		}

		//Save all connected users every 10 minutes:
		if (framesSinceLastUsersSave >= saveTime && zoneID != 0) {
			UserManager::Instance()->SaveAllActiveCharacters();
			framesSinceLastUsersSave = 0;

			if (PropertyManagementComponent::Instance() != nullptr) {
				PropertyManagementComponent::Instance()->Save();
			}
		} else framesSinceLastUsersSave++;

		//Every 10 min we ping our sql server to keep it alive hopefully:
		if (framesSinceLastSQLPing >= sqlPingTime) {
			//Find out the master's IP for absolutely no reason:
			std::string masterIP;
			uint32_t masterPort;
			auto masterInfo = Database::Get()->GetMasterInfo();
			if (masterInfo) {
				masterIP = masterInfo->ip;
				masterPort = masterInfo->port;
			}

			framesSinceLastSQLPing = 0;
		} else framesSinceLastSQLPing++;

		Metrics::EndMeasurement(MetricVariable::GameLoop);

		Metrics::StartMeasurement(MetricVariable::Sleep);

		t += std::chrono::milliseconds(currentFrameDelta);
		std::this_thread::sleep_until(t);

		Metrics::EndMeasurement(MetricVariable::Sleep);

		if (!ready && Game::server->GetIsConnectedToMaster()) {
			LOG("Finished loading world with zone (%i), ready up!", Game::server->GetZoneID());

			MasterPackets::SendWorldReady(Game::server, Game::server->GetZoneID(), Game::server->GetInstanceID());

			ready = true;
		}

		if (Game::ShouldShutdown() && !g_WorldShutdownSequenceComplete) {
			WorldShutdownProcess(zoneID);
			break;
		}

		Metrics::AddMeasurement(MetricVariable::CPUTime, (1e6 * (1000.0 * (std::clock() - metricCPUTimeStart))) / CLOCKS_PER_SEC);
		Metrics::EndMeasurement(MetricVariable::Frame);
	}
	FinalizeShutdown();
	return EXIT_SUCCESS;
}

void HandlePacketChat(Packet* packet) {
	if (packet->length < 1) return;
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		LOG("Lost our connection to chat, zone(%i), instance(%i)", Game::server->GetZoneID(), Game::server->GetInstanceID());

		g_ChatConnected = false;
	}

	if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED) {
		LOG("Established connection to chat, zone(%i), instance (%i)", Game::server->GetZoneID(), Game::server->GetInstanceID());
		Game::chatSysAddr = packet->systemAddress;

		g_ChatConnected = true;
	}

	if (packet->data[0] == ID_USER_PACKET_ENUM && packet->length >= 4) {
		if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::CHAT) {
			switch (static_cast<MessageType::Chat>(packet->data[3])) {
			case MessageType::Chat::WORLD_ROUTE_PACKET: {
				CINSTREAM_SKIP_HEADER;
				LWOOBJID playerID;
				inStream.Read(playerID);

				auto player = Game::entityManager->GetEntity(playerID);
				if (!player) return;

				auto sysAddr = player->GetSystemAddress();

				//Write our stream outwards:
				CBITSTREAM;
				unsigned char data;
				while (inStream.Read(data)) {
					bitStream.Write(data);
				}

				SEND_PACKET; //send routed packet to player
				break;
			}

			case MessageType::Chat::GM_ANNOUNCE: {
				CINSTREAM_SKIP_HEADER;

				std::string title;
				std::string msg;

				uint32_t len;
				inStream.Read<uint32_t>(len);
				for (uint32_t i = 0; len > i; i++) {
					char character;
					inStream.Read<char>(character);
					title += character;
				}

				len = 0;
				inStream.Read<uint32_t>(len);
				for (uint32_t i = 0; len > i; i++) {
					char character;
					inStream.Read<char>(character);
					msg += character;
				}

				//Send to our clients:
				AMFArrayValue args;

				args.Insert("title", title);
				args.Insert("message", msg);

				GameMessages::SendUIMessageServerToAllClients("ToggleAnnounce", args);

				break;
			}

			case MessageType::Chat::GM_MUTE: {
				CINSTREAM_SKIP_HEADER;
				LWOOBJID playerId;
				time_t expire = 0;
				inStream.Read(playerId);
				inStream.Read(expire);

				auto* entity = Game::entityManager->GetEntity(playerId);
				auto* character = entity != nullptr ? entity->GetCharacter() : nullptr;
				auto* user = character != nullptr ? character->GetParentUser() : nullptr;
				if (user) {
					user->SetMuteExpire(expire);

					entity->GetCharacter()->SendMuteNotice();
				}

				break;
			}

			case MessageType::Chat::TEAM_GET_STATUS: {
				CINSTREAM_SKIP_HEADER;

				LWOOBJID teamID = 0;
				char lootOption = 0;
				char memberCount = 0;
				std::vector<LWOOBJID> members;

				inStream.Read(teamID);
				bool deleteTeam = inStream.ReadBit();

				if (deleteTeam) {
					TeamManager::Instance()->DeleteTeam(teamID);

					LOG("Deleting team (%llu)", teamID);

					break;
				}

				inStream.Read(lootOption);
				inStream.Read(memberCount);
				LOG("Updating team ID:(%llu), Loot:(%i), #Members:(%i)", teamID, lootOption, memberCount);
				for (char i = 0; i < memberCount; i++) {
					LWOOBJID member = LWOOBJID_EMPTY;
					inStream.Read(member);
					members.push_back(member);

					LOG("Added member (%llu) to the team", member);
				}

				TeamManager::Instance()->UpdateTeam(teamID, lootOption, members);

				break;
			}
			default:
				LOG("Received an unknown chat: %s", StringifiedEnum::ToString(static_cast<MessageType::Chat>(packet->data[3])).data());
			}
		}
	}
}

void HandleMasterPacket(Packet* packet) {
	if (packet->length < 2) return;
	if (static_cast<eConnectionType>(packet->data[1]) != eConnectionType::MASTER || packet->length < 4) return;
	switch (static_cast<MessageType::Master>(packet->data[3])) {
	case MessageType::Master::REQUEST_PERSISTENT_ID_RESPONSE: {
		CINSTREAM_SKIP_HEADER;
		uint64_t requestID;
		inStream.Read(requestID);
		uint32_t objectID;
		inStream.Read(objectID);
		ObjectIDManager::HandleRequestPersistentIDResponse(requestID, objectID);
		break;
	}

	case MessageType::Master::SESSION_KEY_RESPONSE: {
		//Read our session key and to which user it belongs:
		CINSTREAM_SKIP_HEADER;
		uint32_t sessionKey = 0;
		inStream.Read(sessionKey);
		LUWString username;
		inStream.Read(username);

		//Find them:
		auto it = g_PendingUsers.find(username.GetAsString());
		if (it == g_PendingUsers.end()) return;

		//Convert our key:
		std::string userHash = std::to_string(sessionKey);
		userHash = md5(userHash);

		//Verify it:
		if (userHash != it->second.hash) {
			LOG("SOMEONE IS TRYING TO HACK? SESSION KEY MISMATCH: ours: %s != master: %s", userHash.c_str(), it->second.hash.c_str());
			Game::server->Disconnect(it->second.sysAddr, eServerDisconnectIdentifiers::INVALID_SESSION_KEY);
			return;
		} else {
			LOG("User %s authenticated with correct key.", username.GetAsString().c_str());

			UserManager::Instance()->DeleteUser(packet->systemAddress);

			//Create our user and send them in:
			UserManager::Instance()->CreateUser(it->second.sysAddr, username.GetAsString(), userHash);

			if (Game::zoneManager->HasZone()) {
				float x = 0.0f;
				float y = 0.0f;
				float z = 0.0f;

				auto zone = Game::zoneManager->GetZone();
				if (zone->GetZoneID().GetMapID() == 1100) {
					auto pos = zone->GetSpawnPos();
					x = pos.x;
					y = pos.y;
					z = pos.z;
				}

				WorldPackets::SendLoadStaticZone(it->second.sysAddr, x, y, z, zone->GetChecksum(), Game::zoneManager->GetZoneID());
			}

			if (Game::server->GetZoneID() == 0) {
				//Since doing this reroute breaks the client's request, we have to call this manually.
				UserManager::Instance()->RequestCharacterList(it->second.sysAddr);
			}

			g_PendingUsers.erase(username.GetAsString());

			//Notify master:
			{
				CBITSTREAM;
				BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, MessageType::Master::PLAYER_ADDED);
				bitStream.Write<LWOMAPID>(Game::server->GetZoneID());
				bitStream.Write<LWOINSTANCEID>(g_InstanceID);
				Game::server->SendToMaster(bitStream);
			}
		}

		break;
	}
	case MessageType::Master::AFFIRM_TRANSFER_REQUEST: {
		CINSTREAM_SKIP_HEADER;
		uint64_t requestID;
		inStream.Read(requestID);
		LOG("Got affirmation request of transfer %llu", requestID);

		CBITSTREAM;

		BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, MessageType::Master::AFFIRM_TRANSFER_RESPONSE);
		bitStream.Write(requestID);
		Game::server->SendToMaster(bitStream);

		break;
	}

	case MessageType::Master::SHUTDOWN: {
		Game::lastSignal = -1;
		LOG("Got shutdown request from master, zone (%i), instance (%i)", Game::server->GetZoneID(), Game::server->GetInstanceID());
		break;
	}

	case MessageType::Master::NEW_SESSION_ALERT: {
		CINSTREAM_SKIP_HEADER;
		uint32_t sessionKey = inStream.Read(sessionKey);

		LUString username;
		inStream.Read(username);
		LOG("Got new session alert for user %s", username.string.c_str());
		//Find them:
		User* user = UserManager::Instance()->GetUser(username.string.c_str());
		if (!user) {
			LOG("But they're not logged in?");
			return;
		}

		//Check the key:
		if (sessionKey != std::atoi(user->GetSessionKey().c_str())) {
			LOG("But the session key is invalid!", username.string.c_str());
			Game::server->Disconnect(user->GetSystemAddress(), eServerDisconnectIdentifiers::INVALID_SESSION_KEY);
			return;
		}
		break;
	}
	default:
		LOG("Unknown packet ID from master %i", int(packet->data[3]));
	}
}

void HandlePacket(Packet* packet) {
	if (packet->length < 1) return;
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		auto user = UserManager::Instance()->GetUser(packet->systemAddress);
		if (!user) return;

		auto c = user->GetLastUsedChar();
		if (!c) {
			UserManager::Instance()->DeleteUser(packet->systemAddress);
			return;
		}

		auto* entity = Game::entityManager->GetEntity(c->GetObjectID());

		if (!entity) {
			entity = PlayerManager::GetPlayer(packet->systemAddress);
		}

		if (entity) {
			auto* skillComponent = entity->GetComponent<SkillComponent>();

			if (skillComponent != nullptr) {
				skillComponent->Reset();
			}


			entity->GetCharacter()->SetLastVisitedZoneID(Game::server->GetZoneID());							

			entity->GetCharacter()->SaveXMLToDatabase();

			LOG("Deleting player %llu", entity->GetObjectID());

			Game::entityManager->DestroyEntity(entity);
		}

		{
			CBITSTREAM;
			BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::UNEXPECTED_DISCONNECT);
			bitStream.Write(user->GetLoggedInChar());
			Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
		}

		UserManager::Instance()->DeleteUser(packet->systemAddress);

		if (PropertyManagementComponent::Instance() != nullptr) {
			PropertyManagementComponent::Instance()->Save();
		}

		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, MessageType::Master::PLAYER_REMOVED);
		bitStream.Write<LWOMAPID>(Game::server->GetZoneID());
		bitStream.Write<LWOINSTANCEID>(g_InstanceID);
		Game::server->SendToMaster(bitStream);
	}

	if (packet->data[0] != ID_USER_PACKET_ENUM || packet->length < 4) return;

	CINSTREAM;
	LUBitStream luBitStream;
	luBitStream.ReadHeader(inStream);

	if (luBitStream.connectionType == eConnectionType::SERVER) {
		if (static_cast<MessageType::Server>(luBitStream.internalPacketID) == MessageType::Server::VERSION_CONFIRM) {
			AuthPackets::HandleHandshake(Game::server, packet);
		}
	}

	if (luBitStream.connectionType != eConnectionType::WORLD) return;

	switch (static_cast<MessageType::World>(luBitStream.internalPacketID)) {
	case MessageType::World::VALIDATION: {
		CINSTREAM_SKIP_HEADER;
		LUWString username;
		inStream.Read(username);

		LUWString sessionKey;
		// sometimes client puts a null terminator at the end of the checksum and sometimes doesn't, weird
		inStream.Read(sessionKey);
		LUString clientDatabaseChecksum(32);
		inStream.Read(clientDatabaseChecksum);

		// If the check is turned on, validate the client's database checksum.
		if (Game::config->GetValue("check_fdb") == "1" && !g_DatabaseChecksum.empty()) {
			auto accountInfo = Database::Get()->GetAccountInfo(username.GetAsString());
			if (!accountInfo) {
				LOG("Client's account does not exist in the database, aborting connection.");
				Game::server->Disconnect(packet->systemAddress, eServerDisconnectIdentifiers::CHARACTER_NOT_FOUND);
				return;
			}

			// Developers may skip this check
			if (clientDatabaseChecksum.string != g_DatabaseChecksum) {

				if (accountInfo->maxGmLevel < eGameMasterLevel::DEVELOPER) {
					LOG("Client's database checksum does not match the server's, aborting connection.");
					std::vector<Stamp> stamps;

					// Using the LoginResponse here since the UI is still in the login screen state
					// and we have a way to send a message about the client mismatch.
					AuthPackets::SendLoginResponse(
						Game::server, packet->systemAddress, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH,
						Game::config->GetValue("cdclient_mismatch_message"), "", 0, "", stamps);
					return;
				} else {
					AMFArrayValue args;

					args.Insert("title", Game::config->GetValue("cdclient_mismatch_title"));
					args.Insert("message", Game::config->GetValue("cdclient_mismatch_message"));

					GameMessages::SendUIMessageServerToSingleClient("ToggleAnnounce", args, packet->systemAddress);
					LOG("Account (%s) with GmLevel (%s) does not have a matching FDB, but is a developer and will skip this check."
						, username.GetAsString().c_str(), StringifiedEnum::ToString(accountInfo->maxGmLevel).data());
				}
			}
		}

		//Request the session info from Master:
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, MessageType::Master::REQUEST_SESSION_KEY);
		bitStream.Write(username);
		Game::server->SendToMaster(bitStream);

		//Insert info into our pending list
		TempSessionInfo info;
		info.sysAddr = packet->systemAddress;
		info.hash = sessionKey.GetAsString();
		g_PendingUsers[username.GetAsString()] = info;

		break;
	}

	case MessageType::World::CHARACTER_LIST_REQUEST: {
		//We need to delete the entity first, otherwise the char list could delete it while it exists in the world!
		if (Game::server->GetZoneID() != 0) {
			auto user = UserManager::Instance()->GetUser(packet->systemAddress);
			if (!user || !user->GetLastUsedChar()) return;
			Game::entityManager->DestroyEntity(user->GetLastUsedChar()->GetEntity());
		}

		//This loops prevents users who aren't authenticated to double-request the char list, which
		//would make the login screen freeze sometimes.
		if (g_PendingUsers.size() > 0) {
			for (const auto& it : g_PendingUsers) {
				if (it.second.sysAddr == packet->systemAddress) {
					return;
				}
			}
		}

		UserManager::Instance()->RequestCharacterList(packet->systemAddress);
		break;
	}

	case MessageType::World::GAME_MSG: {
		RakNet::BitStream bitStream(packet->data, packet->length, false);

		uint64_t header;
		LWOOBJID objectID;
		MessageType::Game messageID;

		bitStream.Read(header);
		bitStream.Read(objectID);
		bitStream.Read(messageID);

		RakNet::BitStream dataStream;
		bitStream.Read(dataStream, bitStream.GetNumberOfUnreadBits());

		auto isSender = CheatDetection::VerifyLwoobjidIsSender(
			objectID,
			packet->systemAddress,
			CheckType::Entity,
			"Sending GM with a sending player that does not match their own. GM ID: %i",
			static_cast<int32_t>(messageID)
		);

		if (isSender) GameMessageHandler::HandleMessage(dataStream, packet->systemAddress, objectID, messageID);
		break;
	}

	case MessageType::World::CHARACTER_CREATE_REQUEST: {
		UserManager::Instance()->CreateCharacter(packet->systemAddress, packet);
		break;
	}

	case MessageType::World::LOGIN_REQUEST: {
		RakNet::BitStream inStream(packet->data, packet->length, false);
		uint64_t header = inStream.Read(header);

		LWOOBJID playerID = 0;
		inStream.Read(playerID);

		bool valid = CheatDetection::VerifyLwoobjidIsSender(
			playerID,
			packet->systemAddress,
			CheckType::User,
			"Sending login request with a sending player that does not match their own. Player ID: %llu",
			playerID
		);

		if (!valid) return;

		GeneralUtils::ClearBit(playerID, eObjectBits::CHARACTER);
		GeneralUtils::ClearBit(playerID, eObjectBits::PERSISTENT);

		auto user = UserManager::Instance()->GetUser(packet->systemAddress);

		if (user) {
			auto lastCharacter = user->GetLoggedInChar();
			// This means we swapped characters and we need to remove the previous player from the container.
			if (static_cast<uint32_t>(lastCharacter) != playerID) {
				CBITSTREAM;
				BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::UNEXPECTED_DISCONNECT);
				bitStream.Write(lastCharacter);
				Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
			}
		}

		UserManager::Instance()->LoginCharacter(packet->systemAddress, static_cast<uint32_t>(playerID));
		break;
	}

	case MessageType::World::CHARACTER_DELETE_REQUEST: {
		UserManager::Instance()->DeleteCharacter(packet->systemAddress, packet);
		break;
	}

	case MessageType::World::CHARACTER_RENAME_REQUEST: {
		UserManager::Instance()->RenameCharacter(packet->systemAddress, packet);
		break;
	}

	case MessageType::World::LEVEL_LOAD_COMPLETE: {
		LOG("Received level load complete from user.");
		User* user = UserManager::Instance()->GetUser(packet->systemAddress);
		if (user) {
			Character* c = user->GetLastUsedChar();
			if (c != nullptr) {
				std::u16string username = GeneralUtils::ASCIIToUTF16(c->GetName());
				Game::server->GetReplicaManager()->AddParticipant(packet->systemAddress);

				EntityInfo info{};
				info.lot = 1;
				Entity* player = Game::entityManager->CreateEntity(info, UserManager::Instance()->GetUser(packet->systemAddress));

				auto* characterComponent = player->GetComponent<CharacterComponent>();
				if (!characterComponent) return;

				WorldPackets::SendCreateCharacter(packet->systemAddress, player->GetComponent<CharacterComponent>()->GetReputation(), player->GetObjectID(), c->GetXMLData(), username, c->GetGMLevel());
				WorldPackets::SendServerState(packet->systemAddress);

				const auto respawnPoint = player->GetCharacter()->GetRespawnPoint(Game::zoneManager->GetZone()->GetWorldID());

				Game::entityManager->ConstructEntity(player, UNASSIGNED_SYSTEM_ADDRESS);

				if (respawnPoint != NiPoint3Constant::ZERO) {
					GameMessages::SendPlayerReachedRespawnCheckpoint(player, respawnPoint, NiQuaternionConstant::IDENTITY);
				}

				Game::entityManager->ConstructAllEntities(packet->systemAddress);

				characterComponent->RocketUnEquip(player);

				// Do charxml fixes here
				auto* levelComponent = player->GetComponent<LevelProgressionComponent>();
				auto* const inventoryComponent = player->GetComponent<InventoryComponent>();
				auto* const missionComponent = player->GetComponent<MissionComponent>();
				if (!levelComponent || !missionComponent || !inventoryComponent) return;

				auto version = levelComponent->GetCharacterVersion();
				switch (version) {
				case eCharacterVersion::RELEASE:
					// TODO: Implement, super low priority
					[[fallthrough]];
				case eCharacterVersion::LIVE:
					LOG("Updating Character Flags");
					c->SetRetroactiveFlags();
					levelComponent->SetCharacterVersion(eCharacterVersion::PLAYER_FACTION_FLAGS);
					[[fallthrough]];
				case eCharacterVersion::PLAYER_FACTION_FLAGS:
					LOG("Updating Vault Size");
					player->RetroactiveVaultSize();
					levelComponent->SetCharacterVersion(eCharacterVersion::VAULT_SIZE);
					[[fallthrough]];
				case eCharacterVersion::VAULT_SIZE:
					LOG("Updaing Speedbase");
					levelComponent->SetRetroactiveBaseSpeed();
					levelComponent->SetCharacterVersion(eCharacterVersion::SPEED_BASE);
					[[fallthrough]];
				case eCharacterVersion::SPEED_BASE: {
					LOG("Removing lots from NJ Jay missions bugged at foss");
					// https://explorer.lu/missions/1789
					const auto* mission = missionComponent->GetMission(1789);
					if (mission && mission->IsComplete()) {
						inventoryComponent->RemoveItem(14474, 1, eInventoryType::ITEMS);
						inventoryComponent->RemoveItem(14474, 1, eInventoryType::VAULT_ITEMS);
					}
					// https://explorer.lu/missions/1927
					mission = missionComponent->GetMission(1927);
					if (mission && mission->IsComplete()) {
						inventoryComponent->RemoveItem(14493, 1, eInventoryType::ITEMS);
						inventoryComponent->RemoveItem(14493, 1, eInventoryType::VAULT_ITEMS);
					}
					levelComponent->SetCharacterVersion(eCharacterVersion::NJ_JAYMISSIONS);
					[[fallthrough]];
				}
				case eCharacterVersion::NJ_JAYMISSIONS: {
					LOG("Fixing Nexus Force Explorer missions");
					auto missions = { 502 /* Pet Cove */, 593/* Nimbus Station */, 938/* Avant Gardens */, 284/* Gnarled Forest */, 754/* Forbidden Valley */ };
					bool complete = true;
					for (auto missionID : missions) {
						auto* mission = missionComponent->GetMission(missionID);
						if (!mission || !mission->IsComplete()) {
							complete = false;
						}
					}

					if (complete) missionComponent->CompleteMission(937 /* Nexus Force explorer */);
					levelComponent->SetCharacterVersion(eCharacterVersion::UP_TO_DATE);
					[[fallthrough]];
				}
				case eCharacterVersion::UP_TO_DATE:
					break;
				}

				player->GetCharacter()->SetTargetScene("");

				// Fix the destroyable component
				auto* destroyableComponent = player->GetComponent<DestroyableComponent>();

				if (destroyableComponent != nullptr) {
					destroyableComponent->FixStats();
				}

				//Tell the player to generate BBB models, if any:
				if (g_CloneID != 0) {
					const auto& worldId = Game::zoneManager->GetZone()->GetZoneID();

					const auto zoneId = worldId.GetMapID();
					const auto cloneId = g_CloneID;

					//Check for BBB models:
					auto propertyInfo = Database::Get()->GetPropertyInfo(zoneId, cloneId);

					LWOOBJID propertyId = LWOOBJID_EMPTY;
					if (propertyInfo) propertyId = propertyInfo->id;
					else {
						LOG("Couldn't find property ID for zone %i, clone %i", zoneId, cloneId);
						goto noBBB;
					}
					for (auto& bbbModel : Database::Get()->GetUgcModels(propertyId)) {
						LOG("Getting lxfml ugcID: %llu", bbbModel.id);

						bbbModel.lxfmlData.seekg(0, std::ios::end);
						size_t lxfmlSize = bbbModel.lxfmlData.tellg();
						bbbModel.lxfmlData.seekg(0);

						//Send message:
						LWOOBJID blueprintID = bbbModel.id;
						GeneralUtils::SetBit(blueprintID, eObjectBits::CHARACTER);
						GeneralUtils::SetBit(blueprintID, eObjectBits::PERSISTENT);

						// Workaround for not having a UGC server to get model LXFML onto the client so it
						// can generate the physics and nif for the object.
						CBITSTREAM;
						BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, MessageType::Client::BLUEPRINT_SAVE_RESPONSE);
						bitStream.Write<LWOOBJID>(LWOOBJID_EMPTY); //always zero so that a check on the client passes
						bitStream.Write(eBlueprintSaveResponseType::EverythingWorked);
						bitStream.Write<uint32_t>(1);
						bitStream.Write(blueprintID);

						bitStream.Write<uint32_t>(lxfmlSize);

						bitStream.WriteAlignedBytes(reinterpret_cast<const unsigned char*>(bbbModel.lxfmlData.str().c_str()), lxfmlSize);

						SystemAddress sysAddr = packet->systemAddress;
						SEND_PACKET;
					}
				}

			noBBB:

				// Tell the client it's done loading:
				GameMessages::SendInvalidZoneTransferList(player, packet->systemAddress, GeneralUtils::ASCIIToUTF16(Game::config->GetValue("source")), u"", false, false);
				GameMessages::SendServerDoneLoadingAllObjects(player, packet->systemAddress);

				//Send the player it's mail count:
				//update: this might not be needed so im going to try disabling this here.
				//Mail::HandleNotificationRequest(packet->systemAddress, player->GetObjectID());

				//Notify chat that a player has loaded:
				auto* character = player->GetCharacter();
				auto* user = character != nullptr ? character->GetParentUser() : nullptr;
				if (user) {
					const auto& playerName = character->GetName();

					CBITSTREAM;
					BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::LOGIN_SESSION_NOTIFY);
					bitStream.Write(player->GetObjectID());
					bitStream.Write<uint32_t>(playerName.size());
					for (size_t i = 0; i < playerName.size(); i++) {
						bitStream.Write(playerName[i]);
					}

					auto zone = Game::zoneManager->GetZone()->GetZoneID();
					bitStream.Write(zone.GetMapID());
					bitStream.Write(zone.GetInstanceID());
					bitStream.Write(zone.GetCloneID());
					bitStream.Write(user->GetMuteExpire());
					bitStream.Write(player->GetGMLevel());

					Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE, 0, Game::chatSysAddr, false);
				}
			} else {
				LOG("Couldn't find character to log in with for user %s (%i)!", user->GetUsername().c_str(), user->GetAccountID());
				Game::server->Disconnect(packet->systemAddress, eServerDisconnectIdentifiers::CHARACTER_NOT_FOUND);
			}
		} else {
			LOG("Couldn't get user for level load complete!");
		}
		break;
	}

	case MessageType::World::POSITION_UPDATE: {
		auto positionUpdate = ClientPackets::HandleClientPositionUpdate(packet);

		User* user = UserManager::Instance()->GetUser(packet->systemAddress);
		if (!user) {
			LOG("Unable to get user to parse position update");
			return;
		}

		Entity* entity = Game::entityManager->GetEntity(user->GetLastUsedChar()->GetObjectID());
		if (entity) entity->ProcessPositionUpdate(positionUpdate);
		break;
	}

	case MessageType::World::MAIL: {
		Mail::HandleMail(inStream, packet->systemAddress, UserManager::Instance()->GetUser(packet->systemAddress)->GetLastUsedChar()->GetEntity());
		break;
	}

	case MessageType::World::ROUTE_PACKET: {
		//Yeet to chat
		CINSTREAM_SKIP_HEADER;
		uint32_t size = 0;
		inStream.Read(size);

		if (size > 20000) {
			LOG("Tried to route a packet with a read size > 20000, so likely a false packet.");
			return;
		}

		CBITSTREAM;

		BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, packet->data[14]);

		//We need to insert the player's objectID so the chat server can find who originated this request:
		LWOOBJID objectID = 0;
		auto user = UserManager::Instance()->GetUser(packet->systemAddress);
		if (user) {
			objectID = user->GetLastUsedChar()->GetObjectID();
		}

		bitStream.Write(objectID);

		//Now write the rest of the data:
		auto data = inStream.GetData();
		for (uint32_t i = 23; i - 23 < size && i < packet->length; ++i) {
			bitStream.Write(data[i]);
		}

		Game::chatServer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, Game::chatSysAddr, false);
		break;
	}

	case MessageType::World::STRING_CHECK: {
		auto request = ClientPackets::HandleChatModerationRequest(packet);

		// TODO: Find a good home for the logic in this case.
		User* user = UserManager::Instance()->GetUser(packet->systemAddress);
		if (!user) {
			LOG("Unable to get user to parse chat moderation request");
			return;
		}

		auto* entity = PlayerManager::GetPlayer(packet->systemAddress);

		if (entity == nullptr) {
			LOG("Unable to get player to parse chat moderation request");
			return;
		}

		// Check if the player has restricted chat access
		auto* character = entity->GetCharacter();

		if (character->HasPermission(ePermissionMap::RestrictedChatAccess)) {
			// Send a message to the player
			ChatPackets::SendSystemMessage(
				packet->systemAddress,
				u"This character has restricted chat access."
			);

			return;
		}

		bool isBestFriend = false;

		if (request.chatLevel == 1) {
			// Private chat
			LWOOBJID idOfReceiver = LWOOBJID_EMPTY;

			{
				auto characterIdFetch = Database::Get()->GetCharacterInfo(request.receiver);

				if (characterIdFetch) {
					idOfReceiver = characterIdFetch->id;
				}
			}
			const auto& bffMap = user->GetIsBestFriendMap();
			if (bffMap.find(request.receiver) == bffMap.end() && idOfReceiver != LWOOBJID_EMPTY) {
				auto bffInfo = Database::Get()->GetBestFriendStatus(entity->GetObjectID(), idOfReceiver);

				if (bffInfo) {
					isBestFriend = bffInfo->bestFriendStatus == 3;
				}

				if (isBestFriend) {
					user->UpdateBestFriendValue(request.receiver, true);
				}
			} else if (bffMap.find(request.receiver) != bffMap.end()) {
				isBestFriend = true;
			}
		}

		const auto segments = Game::chatFilter->IsSentenceOkay(request.message, entity->GetGMLevel(), !(isBestFriend && request.chatLevel == 1));

		bool bAllClean = segments.empty();

		if (user->GetIsMuted()) {
			bAllClean = false;
		}

		user->SetLastChatMessageApproved(bAllClean);
		WorldPackets::SendChatModerationResponse(packet->systemAddress, bAllClean, request.requestID, request.receiver, segments);
		break;
	}

	case MessageType::World::GENERAL_CHAT_MESSAGE: {
		if (g_ChatDisabled) {
			ChatPackets::SendMessageFail(packet->systemAddress);
		} else {
			auto chatMessage = ClientPackets::HandleChatMessage(packet);

			// TODO: Find a good home for the logic in this case.
			User* user = UserManager::Instance()->GetUser(packet->systemAddress);
			if (!user) {
				LOG("Unable to get user to parse chat message");
				return;
			}

			if (user->GetIsMuted()) {
				user->GetLastUsedChar()->SendMuteNotice();
				return;
			}
			std::string playerName = user->GetLastUsedChar()->GetName();
			bool isMythran = user->GetLastUsedChar()->GetGMLevel() > eGameMasterLevel::CIVILIAN;
			bool isOk = Game::chatFilter->IsSentenceOkay(GeneralUtils::UTF16ToWTF8(chatMessage.message), user->GetLastUsedChar()->GetGMLevel()).empty();
			LOG_DEBUG("Msg: %s was approved previously? %i", GeneralUtils::UTF16ToWTF8(chatMessage.message).c_str(), user->GetLastChatMessageApproved());
			if (!isOk) return;
			if (!isOk && !isMythran) return;

			std::string sMessage = GeneralUtils::UTF16ToWTF8(chatMessage.message);
			LOG("%s: %s", playerName.c_str(), sMessage.c_str());
			ChatPackets::SendChatMessage(packet->systemAddress, chatMessage.chatChannel, playerName, user->GetLoggedInChar(), isMythran, chatMessage.message);
			if (PropertyManagementComponent::Instance()) PropertyManagementComponent::Instance()->OnChatMessageReceived(sMessage);
		}

		break;
	}

	case MessageType::World::HANDLE_FUNNESS: {
		//This means the client is running slower or faster than it should.
		//Could be insane lag, but I'mma just YEET them as it's usually speedhacking.
		//This is updated to now count the amount of times we've been caught "speedhacking" to kick with a delay
		//This is hopefully going to fix the random disconnects people face sometimes.
		if (Game::config->GetValue("disable_anti_speedhack") == "1") {
			return;
		}

		User* user = UserManager::Instance()->GetUser(packet->systemAddress);
		if (user) {
			user->UserOutOfSync();
		} else {
			Game::server->Disconnect(packet->systemAddress, eServerDisconnectIdentifiers::KICK);
		}
		break;
	}


	case MessageType::World::UI_HELP_TOP_5: {
		auto language = ClientPackets::SendTop5HelpIssues(packet);
		// TODO: Handle different languages in a nice way
		// 0: en_US
		// 1: pl_US
		// 2: de_DE
		// 3: en_GB

		// TODO: Find a good home for the logic in this case.
		auto* user = UserManager::Instance()->GetUser(packet->systemAddress);
		if (!user) return;
		auto* character = user->GetLastUsedChar();
		if (!character) return;
		auto* entity = character->GetEntity();
		if (!entity) return;

		AMFArrayValue data;
		// Summaries
		data.Insert("Summary0", Game::config->GetValue("help_0_summary"));
		data.Insert("Summary1", Game::config->GetValue("help_1_summary"));
		data.Insert("Summary2", Game::config->GetValue("help_2_summary"));
		data.Insert("Summary3", Game::config->GetValue("help_3_summary"));
		data.Insert("Summary4", Game::config->GetValue("help_4_summary"));

		// Descriptions
		data.Insert("Description0", Game::config->GetValue("help_0_description"));
		data.Insert("Description1", Game::config->GetValue("help_1_description"));
		data.Insert("Description2", Game::config->GetValue("help_2_description"));
		data.Insert("Description3", Game::config->GetValue("help_3_description"));
		data.Insert("Description4", Game::config->GetValue("help_4_description"));

		GameMessages::SendUIMessageServerToSingleClient(entity, packet->systemAddress, "UIHelpTop5", data);
		break;
	}

	default:
		const auto messageId = *reinterpret_cast<MessageType::World*>(&packet->data[3]);
		const std::string_view messageIdString = StringifiedEnum::ToString(messageId);
		LOG("Unknown world packet received: %4i, %s", messageId, messageIdString.data());
	}
}

void WorldShutdownProcess(uint32_t zoneId) {
	LOG("Saving map %i instance %i", zoneId, g_InstanceID);
	for (auto i = 0; i < Game::server->GetReplicaManager()->GetParticipantCount(); ++i) {
		const auto& player = Game::server->GetReplicaManager()->GetParticipantAtIndex(i);

		auto* entity = PlayerManager::GetPlayer(player);
		LOG("Saving data!");
		if (entity != nullptr && entity->GetCharacter() != nullptr) {
			auto* skillComponent = entity->GetComponent<SkillComponent>();

			if (skillComponent != nullptr) {
				skillComponent->Reset();
			}
				
			entity->GetCharacter()->SetLastVisitedZoneID(Game::server->GetZoneID());				
				
			LOG("Saving character %s...", entity->GetCharacter()->GetName().c_str());
			entity->GetCharacter()->SaveXMLToDatabase();
			LOG("Character data for %s was saved!", entity->GetCharacter()->GetName().c_str());
		}
	}

	if (PropertyManagementComponent::Instance() != nullptr) {
		LOG("Saving ALL property data for zone %i clone %i!", zoneId, PropertyManagementComponent::Instance()->GetCloneId());
		PropertyManagementComponent::Instance()->Save();
		LOG("ALL property data saved for zone %i clone %i!", zoneId, PropertyManagementComponent::Instance()->GetCloneId());
	}

	LOG("ALL DATA HAS BEEN SAVED FOR ZONE %i INSTANCE %i!", zoneId, g_InstanceID);

	while (Game::server->GetReplicaManager()->GetParticipantCount() > 0) {
		const auto& player = Game::server->GetReplicaManager()->GetParticipantAtIndex(0);

		Game::server->Disconnect(player, eServerDisconnectIdentifiers::SERVER_SHUTDOWN);
	}
	SendShutdownMessageToMaster();
}

void WorldShutdownSequence() {
	bool shouldShutdown = Game::ShouldShutdown() || g_WorldShutdownSequenceComplete;
	Game::lastSignal = -1;
#ifndef DARKFLAME_PLATFORM_WIN32
	if (shouldShutdown)
#endif
	{
		return;
	}

	if (!Game::logger) return;

	LOG("Zone (%i) instance (%i) shutting down outside of main loop!", Game::server->GetZoneID(), g_InstanceID);
	WorldShutdownProcess(Game::server->GetZoneID());
	FinalizeShutdown();
}

void FinalizeShutdown() {
	LOG("Shutdown complete, zone (%i), instance (%i)", Game::server->GetZoneID(), g_InstanceID);

	//Delete our objects here:
	Metrics::Clear();
	dpWorld::Shutdown();
	Database::Destroy("WorldServer");
	if (Game::chatFilter) delete Game::chatFilter;
	Game::chatFilter = nullptr;
	if (Game::zoneManager) delete Game::zoneManager;
	Game::zoneManager = nullptr;
	if (Game::server) delete Game::server;
	Game::server = nullptr;
	if (Game::config) delete Game::config;
	Game::config = nullptr;
	if (Game::entityManager) delete Game::entityManager;
	Game::entityManager = nullptr;
	if (Game::logger) delete Game::logger;
	Game::logger = nullptr;

	g_WorldShutdownSequenceComplete = true;

	exit(EXIT_SUCCESS);
}

void SendShutdownMessageToMaster() {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, MessageType::Master::SHUTDOWN_RESPONSE);
	Game::server->SendToMaster(bitStream);
}
