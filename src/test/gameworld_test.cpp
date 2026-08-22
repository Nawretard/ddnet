#include "test.h"

#include <base/logger.h>
#include <base/types.h>

#include <engine/engine.h>
#include <engine/http.h>
#include <engine/kernel.h>
#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/server/register.h>
#include <engine/server/server.h>
#include <engine/server/server_logger.h>
#include <engine/shared/assertion_logger.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/gamecore.h>
#include <game/version.h>

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <thread>

bool IsInterrupted()
{
	return false;
}

#if defined(CONF_PLATFORM_ANDROID)
std::vector<std::string> FetchAndroidServerCommandQueue()
{
	return {};
}
#endif

class GameWorld : public ::testing::Test // NOLINT(readability-identifier-naming)
{
public:
	IGameServer *m_pGameServer = nullptr;
	CServer *m_pServer = nullptr;
	std::unique_ptr<IKernel> m_pKernel;
	CTestInfo m_TestInfo;
	std::unique_ptr<IStorage> m_pStorage;

	CGameContext *GameServer() // NOLINT(readability-make-member-function-const)
	{
		return (CGameContext *)m_pGameServer;
	}

	GameWorld()
	{
		CServer *pServer = CreateServer();
		m_pServer = pServer;

		m_pKernel = std::unique_ptr<IKernel>(IKernel::Create());
		m_pKernel->RegisterInterface(m_pServer);

		IEngine *pEngine = CreateTestEngine(GAME_NAME);
		m_pKernel->RegisterInterface(pEngine);

		m_TestInfo.m_DeleteTestStorageFilesOnSuccess = true;
		m_pStorage = m_TestInfo.CreateTestStorage();
		EXPECT_NE(m_pStorage, nullptr);
		m_pKernel->RegisterInterface(m_pStorage.get(), false);

		IConsole *pConsole = CreateConsole(CFGFLAG_SERVER | CFGFLAG_ECON).release();
		m_pKernel->RegisterInterface(pConsole);

		IConfigManager *pConfigManager = CreateConfigManager();
		m_pKernel->RegisterInterface(pConfigManager);

		IEngineHttp *pEngineHttp = CreateEngineHttp();
		m_pKernel->RegisterInterface(pEngineHttp); // IEngineHttp
		m_pKernel->RegisterInterface(static_cast<IHttp *>(pEngineHttp), false);

		IEngineAntibot *pEngineAntibot = CreateEngineAntibot();
		m_pKernel->RegisterInterface(pEngineAntibot);
		m_pKernel->RegisterInterface(static_cast<IAntibot *>(pEngineAntibot), false);

		m_pGameServer = CreateGameServer();
		m_pKernel->RegisterInterface(m_pGameServer);

		pEngine->Init();
		pConsole->Init();
		pConfigManager->Init();

		m_pServer->RegisterCommands();

		EXPECT_NE(m_pServer->LoadMap("coverage"), 0);

		m_pServer->m_RunServer = CServer::RUNNING;

		m_pServer->m_AuthManager.Init();

		{
			int Size = GameServer()->PersistentClientDataSize();
			for(auto &Client : m_pServer->m_aClients)
			{
				Client.m_HasPersistentData = false;
				Client.m_pPersistentData = malloc(Size);
			}
		}
		m_pServer->m_pPersistentData = malloc(GameServer()->PersistentDataSize());
		EXPECT_NE(m_pServer->LoadMap("coverage"), 0);

		EXPECT_TRUE(pEngineHttp->Init(std::chrono::seconds{2})) << "Failed to initialize the HTTP client";

		pServer->m_NetServer.SetCallbacks(
			CServer::NewClientCallback,
			CServer::NewClientNoAuthCallback,
			CServer::ClientRejoinCallback,
			CServer::DelClientCallback, pServer);

		pServer->m_Econ.Init(pServer->Config(), pServer->Console(), &pServer->m_ServerBan);

		pServer->m_Fifo.Init(pServer->Console(), pServer->Config()->m_SvInputFifo, CFGFLAG_SERVER);
		m_pServer->Antibot()->Init();
		GameServer()->OnInit(nullptr);
		pServer->ReadAnnouncementsFile();
		pServer->InitMaplist();
	}

	~GameWorld() override
	{
		m_pServer->m_Econ.Shutdown();
		m_pServer->m_Fifo.Shutdown();
		m_pGameServer->OnShutdown(nullptr);
		m_pServer->DbPool()->OnShutdown();
	}
};

TEST_F(GameWorld, ClosestCharacter)
{
	CNetObj_PlayerInput Input = {};
	CCharacter *pChr1 = new(0) CCharacter(&GameServer()->m_World, Input);
	pChr1->m_Pos = vec2(0, 0);
	GameServer()->m_World.InsertEntity(pChr1);

	CCharacter *pChr2 = new(1) CCharacter(&GameServer()->m_World, Input);
	pChr2->m_Pos = vec2(10, 10);
	GameServer()->m_World.InsertEntity(pChr2);

	CCharacter *pClosest = GameServer()->m_World.ClosestCharacter(vec2(1, 1), 20, nullptr);
	EXPECT_EQ(pClosest, pChr1);
}

TEST_F(GameWorld, IntersectEntity)
{
	CNetObj_PlayerInput Input = {};
	CCharacter *pChrLeft = new(0) CCharacter(&GameServer()->m_World, Input);
	pChrLeft->m_Pos = vec2(15, 10);
	GameServer()->m_World.InsertEntity(pChrLeft);

	CCharacter *pChrRight = new(1) CCharacter(&GameServer()->m_World, Input);
	pChrRight->m_Pos = vec2(16, 10);
	GameServer()->m_World.InsertEntity(pChrRight);

	float Radius = 5.0f;
	vec2 IntersectAt;
	CCharacter *pIntersectedChar;

	// both tees are exactly on the line
	// if we go intersect left to right we find the left one

	pIntersectedChar = (CCharacter *)GameServer()->m_World.IntersectEntity(
		vec2(10, 10), // intersect from
		vec2(20, 10), // intersect to
		Radius,
		CGameWorld::ENTTYPE_CHARACTER,
		IntersectAt,
		nullptr, // pNotThis
		-1, // CollideWith
		nullptr /* pThisOnly */);
	EXPECT_EQ(pIntersectedChar, pChrLeft);

	// if we intersect right to left we find the right one

	pIntersectedChar = (CCharacter *)GameServer()->m_World.IntersectEntity(
		vec2(20, 10), // intersect from
		vec2(10, 10), // intersect to
		Radius,
		CGameWorld::ENTTYPE_CHARACTER,
		IntersectAt,
		nullptr, // pNotThis
		-1, // CollideWith
		nullptr /* pThisOnly */);
	EXPECT_EQ(pIntersectedChar, pChrRight);

	// but not if we ignore the right one

	pIntersectedChar = (CCharacter *)GameServer()->m_World.IntersectEntity(
		vec2(20, 10), // intersect from
		vec2(10, 10), // intersect to
		Radius,
		CGameWorld::ENTTYPE_CHARACTER,
		IntersectAt,
		pChrRight, // pNotThis
		-1, // CollideWith
		nullptr /* pThisOnly */);
	EXPECT_EQ(pIntersectedChar, pChrLeft);

	// or we force find the left one

	pIntersectedChar = (CCharacter *)GameServer()->m_World.IntersectEntity(
		vec2(20, 10), // intersect from
		vec2(10, 10), // intersect to
		Radius,
		CGameWorld::ENTTYPE_CHARACTER,
		IntersectAt,
		nullptr, // pNotThis
		-1, // CollideWith
		pChrLeft /* pThisOnly */);
	EXPECT_EQ(pIntersectedChar, pChrLeft);

	// pNotThis == pThisOnly => nullptr

	pIntersectedChar = (CCharacter *)GameServer()->m_World.IntersectEntity(
		vec2(20, 10), // intersect from
		vec2(10, 10), // intersect to
		Radius,
		CGameWorld::ENTTYPE_CHARACTER,
		IntersectAt,
		pChrLeft, // pNotThis
		-1, // CollideWith
		pChrLeft /* pThisOnly */);
	EXPECT_EQ(pIntersectedChar, nullptr);

	// the tee closer to the start of the intersection line
	// will not be matched if it is further than Radius away
	// from the line

	vec2 CloserToFromButTooFarFromLine = vec2(11, 11 + Radius + pChrLeft->GetProximityRadius());
	pChrLeft->SetPosition(CloserToFromButTooFarFromLine);
	pChrLeft->m_Pos = CloserToFromButTooFarFromLine;

	pIntersectedChar = (CCharacter *)GameServer()->m_World.IntersectEntity(
		vec2(10, 10), // intersect from
		vec2(20, 10), // intersect to
		Radius,
		CGameWorld::ENTTYPE_CHARACTER,
		IntersectAt,
		nullptr, // pNotThis
		-1, // CollideWith
		nullptr /* pThisOnly */);
	EXPECT_EQ(pIntersectedChar, pChrRight);
}

TEST_F(GameWorld, BasicTick)
{
	int ClientId = 0;
	bool Afk = true;
	int LastWhisperTo = -1;
	const int StartTeam = GameServer()->m_pController->GetAutoTeam(ClientId);
	GameServer()->CreatePlayer(ClientId, StartTeam, Afk, LastWhisperTo);

	GameServer()->OnTick();
}

TEST_F(GameWorld, CharacterEmote)
{
	int ClientId = 0;
	bool Afk = true;
	int LastWhisperTo = -1;
	GameServer()->CreatePlayer(ClientId, TEAM_GAME, Afk, LastWhisperTo);
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	pPlayer->ForceSpawn(vec2(0, 0));
	CCharacter *pChr = pPlayer->GetCharacter();
	ASSERT_NE(pChr, nullptr);

	// afk
	pPlayer->SetAfk(true);
	ASSERT_EQ(pChr->DetermineEyeEmote(), EMOTE_BLINK);

	// not afk
	pPlayer->SetAfk(false);
	ASSERT_EQ(pChr->DetermineEyeEmote(), EMOTE_NORMAL);

	// frozen
	pChr->Freeze(10);
	ASSERT_EQ(pChr->DetermineEyeEmote(), EMOTE_BLINK);

	// frozen and paused
	pPlayer->Pause(CPlayer::PAUSE_PAUSED, true);
	ASSERT_EQ(pChr->DetermineEyeEmote(), EMOTE_NORMAL);

	// ninja jetpack
	pPlayer->Pause(CPlayer::PAUSE_NONE, true);
	pChr->Unfreeze();
	pPlayer->m_NinjaJetpack = true;
	pChr->m_NinjaJetpack = true;
	pChr->SetJetpack(true);
	pChr->SetActiveWeapon(WEAPON_GUN);
	ASSERT_EQ(pChr->DetermineEyeEmote(), EMOTE_HAPPY);

	// /emote angry 3 chat command
	pChr->SetEmote(EMOTE_ANGRY, GameServer()->Server()->Tick() + GameServer()->Server()->TickSpeed() * 3);
	ASSERT_EQ(pChr->DetermineEyeEmote(), EMOTE_ANGRY);

	// /emote angry 3 chat command and frozen
	pChr->Freeze(10);
	ASSERT_EQ(pChr->DetermineEyeEmote(), EMOTE_ANGRY);
}

TEST(Tunings, OutOfRangeBecomesIntMin)
{
	const float IntMin = std::numeric_limits<int>::min() / 100.0f;
	CTuneParam Param;
	EXPECT_EQ((float)(Param = 555555555555555.0f), IntMin);
	EXPECT_EQ((float)(Param = -555555555555555.0f), IntMin);
	EXPECT_EQ((float)(Param = std::numeric_limits<float>::quiet_NaN()), IntMin);
	EXPECT_EQ((float)(Param = 0.5f), 0.5f);
}

namespace {

/** A tee that has just spawned where it was put, at rest, holding a hammer. */
CCharacter *Placed(CGameContext *pGameServer, int ClientId, vec2 Pos, EGravityPreset Preset)
{
	pGameServer->CreatePlayer(ClientId, TEAM_GAME, false, -1);
	CPlayer *pPlayer = pGameServer->m_apPlayers[ClientId];
	pPlayer->ForceSpawn(Pos);
	CCharacter *pChr = pPlayer->GetCharacter();
	pChr->SetActiveWeapon(WEAPON_HAMMER);
	pChr->SetGravity(Preset);
	pChr->SetVelocity(vec2(0.0f, 0.0f));
	return pChr;
}

// One swing, aimed the way the hammer's own body reads it. Two inputs, because a
// swing is a *press*: OnDirectInput copies the latest over the previous on its way
// out, so one call can never read as one.
void HammerTowards(CCharacter *pChr, vec2 AimInOwnFrame)
{
	CNetObj_PlayerInput Input;
	mem_zero(&Input, sizeof(Input));
	Input.m_TargetX = (int)AimInOwnFrame.x;
	Input.m_TargetY = (int)AimInOwnFrame.y;
	pChr->OnDirectInput(&Input);
	Input.m_Fire = 1;
	pChr->OnDirectInput(&Input);
}

} // namespace

TEST_F(GameWorld, TheHammerLiftsAlongTheSwingersOwnUp)
{
	// Aimed straight at a tee lying at its own feet, the kick is the drag and the
	// lift on one axis: 10 of impulse plus 1 of force, along the swinger's up.
	CCharacter *pHammer = Placed(GameServer(), 0, vec2(600.0f, 600.0f), GRAVITY_DOWN);
	CCharacter *pTarget = Placed(GameServer(), 1, vec2(600.0f, 621.0f), GRAVITY_DOWN);
	ASSERT_EQ(pHammer->m_Pos, vec2(600.0f, 600.0f));
	ASSERT_EQ(pTarget->m_Pos, vec2(600.0f, 621.0f));

	HammerTowards(pHammer, vec2(0.0f, 100.0f));

	EXPECT_EQ(pTarget->Core()->m_Vel, vec2(0.0f, -11.0f));
}

TEST_F(GameWorld, TheSameSwingDoesTheSameThingInEveryFrame)
{
	// What a player can do must not depend on which way they fall. The same swing
	// -- aimed at their own feet, at a tee standing there -- throws that tee the
	// same way in the swinger's own frame, which is the world's left for one that
	// falls right.
	CCharacter *pHammer = Placed(GameServer(), 0, vec2(600.0f, 600.0f), GRAVITY_RIGHT);
	CCharacter *pTarget = Placed(GameServer(), 1, vec2(621.0f, 600.0f), GRAVITY_DOWN);

	HammerTowards(pHammer, vec2(0.0f, 100.0f));

	EXPECT_EQ(pTarget->Core()->m_Vel, vec2(-11.0f, 0.0f));
}

TEST_F(GameWorld, TheHammerAsksTheSwingerWhichWayIsUpNotTheTeeItHits)
{
	// The two frames disagree here, which is the whole point: the swinger falls
	// down, the tee it hits falls right. The swinger's up decides.
	CCharacter *pHammer = Placed(GameServer(), 0, vec2(600.0f, 600.0f), GRAVITY_DOWN);
	CCharacter *pTarget = Placed(GameServer(), 1, vec2(600.0f, 621.0f), GRAVITY_RIGHT);

	HammerTowards(pHammer, vec2(0.0f, 100.0f));

	EXPECT_EQ(pTarget->Core()->m_Vel, vec2(0.0f, -11.0f));
}

namespace {

/** One tick of a ninja dash, thrown the way its own body reads it. */
void DashPast(CCharacter *pChr, vec2 DirInWorld)
{
	pChr->GiveNinja();
	pChr->SetNinjaActivationDir(DirInWorld);
	pChr->SetNinjaCurrentMoveTime(5);
	pChr->HandleNinja();
}

} // namespace

TEST_F(GameWorld, TheNinjaThrowsAlongItsOwnUp)
{
	CCharacter *pNinja = Placed(GameServer(), 0, vec2(600.0f, 600.0f), GRAVITY_DOWN);
	CCharacter *pTarget = Placed(GameServer(), 1, vec2(621.0f, 600.0f), GRAVITY_DOWN);

	DashPast(pNinja, vec2(1.0f, 0.0f));

	EXPECT_EQ(pTarget->Core()->m_Vel, vec2(0.0f, -10.0f));
}

TEST_F(GameWorld, TheNinjaThrowsTheSameWayWhicheverWayItFalls)
{
	// Same rule as the hammer, and the same reason: what a player can do must not
	// change because the world turned under them.
	CCharacter *pNinja = Placed(GameServer(), 0, vec2(600.0f, 600.0f), GRAVITY_RIGHT);
	CCharacter *pTarget = Placed(GameServer(), 1, vec2(621.0f, 600.0f), GRAVITY_DOWN);

	DashPast(pNinja, vec2(1.0f, 0.0f));

	EXPECT_EQ(pTarget->Core()->m_Vel, vec2(-10.0f, 0.0f));
}
