#include "ascii_world.h"

#include <base/mem.h>
#include <base/vmath.h>

#include <game/gamecore.h>

#include <gtest/gtest.h>

// Characterization tests, same contract as collision_test.cpp: exact values read
// off the unmodified implementation, so a refactoring that moves one bit shows up.

namespace {

// CCharacterCore::Reset does not clear m_Input, so a fresh core reads whatever
// the stack held.
CCharacterCore SpawnedAt(CWorldCore *pWorld, CCollision *pCollision, vec2 Pos)
{
	CCharacterCore Core;
	Core.Init(pWorld, pCollision, nullptr);
	Core.Reset();
	mem_zero(&Core.m_Input, sizeof(Core.m_Input));
	Core.m_Pos = Pos;
	return Core;
}

// Resting on the floor of Room(): low enough to be grounded, high enough not to overlap.
constexpr vec2 ON_FLOOR = vec2(160.0f, 177.0f);
constexpr vec2 MID_AIR = vec2(160.0f, 100.0f);

} // namespace

TEST(GameCore, GravityAccumulatesEachTick)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_AIR);

	Core.Tick(true);
	Core.Move();
	EXPECT_EQ(Core.m_Vel.y, 0.5f);
	EXPECT_EQ(Core.m_Pos.y, 100.5f);

	Core.Tick(true);
	Core.Move();
	EXPECT_EQ(Core.m_Vel.y, 1.0f);
	EXPECT_EQ(Core.m_Pos.y, 101.5f);

	Core.Tick(true);
	Core.Move();
	EXPECT_EQ(Core.m_Vel.y, 1.5f);
	EXPECT_EQ(Core.m_Pos.y, 103.0f);
}

TEST(GameCore, GroundJumpOverwritesVelocityInsteadOfAddingToIt)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), ON_FLOOR);
	Core.m_Input.m_Jump = 1;

	Core.Tick(true);

	// The gravity of this same tick is gone, not subtracted from the impulse.
	EXPECT_EQ(Core.m_Vel.y, -13.1999998f);
	EXPECT_EQ(Core.m_TriggeredEvents, COREEVENT_GROUND_JUMP);
}

TEST(GameCore, AirJumpNeedsTheJumpKeyReleasedFirst)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), ON_FLOOR);

	Core.m_Input.m_Jump = 1;
	Core.Tick(true);
	Core.Move();

	Core.m_Input.m_Jump = 0;
	Core.Tick(true);

	Core.m_Input.m_Jump = 1;
	Core.Tick(true);

	EXPECT_EQ(Core.m_Vel.y, -12.0f);
	EXPECT_EQ(Core.m_TriggeredEvents, COREEVENT_AIR_JUMP);
}

TEST(GameCore, AirControlAcceleratesSlowerThanGroundControl)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;

	CCharacterCore Air = SpawnedAt(&WorldCore, World.Collision(), MID_AIR);
	Air.m_Input.m_Direction = 1;
	Air.Tick(true);
	EXPECT_EQ(Air.m_Vel.x, 1.5f);
	Air.Tick(true);
	EXPECT_EQ(Air.m_Vel.x, 3.0f);

	CCharacterCore Ground = SpawnedAt(&WorldCore, World.Collision(), ON_FLOOR);
	Ground.m_Input.m_Direction = 1;
	Ground.Tick(true);
	EXPECT_EQ(Ground.m_Vel.x, 2.0f);
	Ground.Tick(true);
	EXPECT_EQ(Ground.m_Vel.x, 4.0f);
}

TEST(GameCore, MoveRampsHorizontalVelocityForTheStepAndRestoresItAfter)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_AIR);
	Core.m_Vel = vec2(30.0f, 0.0f);

	Core.Move();

	// The step covers 25.57 of the 30, then the ramp is divided back out exactly.
	EXPECT_EQ(Core.m_Pos.x, 185.568756f);
	EXPECT_EQ(Core.m_Vel.x, 30.0f);
}

// Ground is no longer something the collision answers: Move folds the contacts it
// reports through the body's own rule. These four cover the parts of that rule.

namespace {

CCharacterCore MidJump(CWorldCore *pWorld, CCollision *pCollision, vec2 Pos, float ElasticityX, float ElasticityY)
{
	CCharacterCore Core = SpawnedAt(pWorld, pCollision, Pos);
	Core.m_Tuning.m_GroundElasticityX = ElasticityX;
	Core.m_Tuning.m_GroundElasticityY = ElasticityY;
	Core.m_Jumped = 3;
	Core.m_JumpedTotal = 5;
	return Core;
}

} // namespace

TEST(GameCore, AnElasticFloorGivesTheAirJumpBack)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = MidJump(&WorldCore, World.Collision(), vec2(160.0f, 130.0f), 0.0f, 0.5f);
	Core.m_Vel = vec2(0.0f, 51.0f);

	Core.Move();

	EXPECT_EQ(Core.m_Jumped, 1);
	EXPECT_EQ(Core.m_JumpedTotal, 0);
}

TEST(GameCore, AFloorWithoutElasticityDoesNotGround)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = MidJump(&WorldCore, World.Collision(), vec2(160.0f, 130.0f), 0.0f, 0.0f);
	Core.m_Vel = vec2(0.0f, 51.0f);

	Core.Move();

	EXPECT_EQ(Core.m_Jumped, 3);
	EXPECT_EQ(Core.m_JumpedTotal, 5);
}

TEST(GameCore, AWallDoesNotGround)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = MidJump(&WorldCore, World.Collision(), vec2(250.0f, 100.0f), 0.5f, 0.5f);
	Core.m_Vel = vec2(51.0f, 0.0f);

	Core.Move();

	EXPECT_EQ(Core.m_Jumped, 3);
	EXPECT_EQ(Core.m_JumpedTotal, 5);
}

TEST(GameCore, ACeilingDoesNotGround)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = MidJump(&WorldCore, World.Collision(), vec2(160.0f, 90.0f), 0.0f, 0.5f);
	Core.m_Vel = vec2(0.0f, -51.0f);

	Core.Move();

	// Same surface axis as the floor, opposite normal.
	EXPECT_EQ(Core.m_Jumped, 3);
	EXPECT_EQ(Core.m_JumpedTotal, 5);
}

TEST(GameCore, TheCornerCaseStillGroundsThroughItsFloorNormal)
{
	CAsciiWorld World({
		"..........",
		"..........",
		"..........",
		"..........",
		"..........",
		".....#....",
		"..........",
		"..........",
	});
	CWorldCore WorldCore;
	CCharacterCore Core = MidJump(&WorldCore, World.Collision(), vec2(145.4f, 145.4f), 0.5f, 0.5f);
	Core.m_Vel = vec2(0.4f, 0.4f);

	Core.Move();

	// It reports two contacts; only the one facing up counts.
	EXPECT_EQ(Core.m_Jumped, 1);
	EXPECT_EQ(Core.m_JumpedTotal, 0);
}

TEST(GameCore, GroundReachesFivePixelsBelowTheFeet)
{
	CAsciiWorld World = Room();
	const vec2 Size = CCharacterCore::PhysicalSizeVec2();

	EXPECT_FALSE(StandsOnSurface(World.Collision(), vec2(160.0f, 172.0f), Size, GRAVITY_DOWN));
	EXPECT_TRUE(StandsOnSurface(World.Collision(), vec2(160.0f, 173.0f), Size, GRAVITY_DOWN));
}

TEST(GameCore, GroundIsWhicheverWayGravityPoints)
{
	CAsciiWorld World = Room();
	const vec2 Size = CCharacterCore::PhysicalSizeVec2();
	const vec2 UnderTheCeiling = vec2(160.0f, 50.0f);

	EXPECT_FALSE(StandsOnSurface(World.Collision(), UnderTheCeiling, Size, GRAVITY_DOWN));
	EXPECT_TRUE(StandsOnSurface(World.Collision(), UnderTheCeiling, Size, -GRAVITY_DOWN));
}

TEST(GameCore, OneFootOnTheLedgeIsEnough)
{
	CAsciiWorld World({
		"..........",
		"..........",
		"..........",
		"..........",
		"..........",
		"..........",
		"###.......",
		"..........",
	});
	const vec2 Size = CCharacterCore::PhysicalSizeVec2();

	// The floor ends at x 96: the left foot is over it, the right one is over nothing.
	EXPECT_TRUE(StandsOnSurface(World.Collision(), vec2(85.0f, 173.0f), Size, GRAVITY_DOWN));
	EXPECT_FALSE(StandsOnSurface(World.Collision(), vec2(120.0f, 173.0f), Size, GRAVITY_DOWN));
}

namespace {

vec2 VelocityAfterOneHookedTick(CAsciiWorld &World, CWorldCore *pWorldCore, vec2 Pos, vec2 HookPos, int Direction)
{
	CCharacterCore Core = SpawnedAt(pWorldCore, World.Collision(), Pos);
	Core.m_Input.m_Hook = 1;
	Core.m_Input.m_Direction = Direction;
	Core.m_HookState = HOOK_GRABBED;
	Core.m_HookPos = HookPos;
	Core.Tick(true);
	return Core.m_Vel;
}

constexpr vec2 MID_ROOM = vec2(160.0f, 112.0f);

} // namespace

TEST(GameCore, TheHookPullsAgainstGravityHarderThanWithIt)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;

	// Anchored straight up, the drag of 3 arrives whole against the gravity of 0.5.
	EXPECT_EQ(VelocityAfterOneHookedTick(World, &WorldCore, MID_ROOM, vec2(160.0f, 50.0f), 0).y, -2.5f);
	// Anchored straight down, it is cut to 0.3 of itself first.
	EXPECT_EQ(VelocityAfterOneHookedTick(World, &WorldCore, MID_ROOM, vec2(160.0f, 174.0f), 0).y, 1.4000001f);
}

TEST(GameCore, TheHookIsDampenedLessWhenSteeringIntoIt)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	const vec2 ToTheRight = vec2(250.0f, 112.0f);

	// Not steering: the drag of 3 keeps 0.75 of itself.
	EXPECT_EQ(VelocityAfterOneHookedTick(World, &WorldCore, MID_ROOM, ToTheRight, 0).x, 2.25f);
	// Steering into it: 0.95 of the drag, on top of the air control of 1.5.
	EXPECT_EQ(VelocityAfterOneHookedTick(World, &WorldCore, MID_ROOM, ToTheRight, 1).x, 4.3499999f);
}
