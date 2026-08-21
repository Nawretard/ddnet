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

// Resting on the floor of Room(): low enough for IsOnGround, high enough not to overlap.
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
