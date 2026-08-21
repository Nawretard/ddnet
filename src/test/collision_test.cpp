#include "ascii_world.h"

#include <base/vmath.h>

#include <game/collision.h>
#include <game/gamecore.h>

#include <gtest/gtest.h>

#include <cmath>

// Characterization tests: they freeze what MoveBox and IsOnGround currently do,
// not what they ought to do. The expected values were read off the unmodified
// implementation, and the velocities and elasticities are deliberately ones
// whose products round, so that a rewrite which is algebraically equivalent but
// rounds elsewhere still fails.

TEST(Collision, MoveBoxAccumulatesSubStepsRatherThanAddingVelocityOnce)
{
	CAsciiWorld World = Room();
	vec2 Pos(160.0f, 100.0f);
	vec2 Vel(0.0f, 5.0f);

	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.0f, 0.0f));

	// Six additions of Vel/6 land 1.5e-5 past Pos + Vel.
	EXPECT_EQ(Pos.x, 160.0f);
	EXPECT_EQ(Pos.y, 105.000015f);
	EXPECT_EQ(Vel.y, 5.0f);
}

TEST(Collision, MoveBoxBouncesOffTheFloorAndReportsGround)
{
	CAsciiWorld World = Room();
	vec2 Pos(160.0f, 130.0f);
	vec2 Vel(0.0f, 51.0f);
	bool Grounded = false;

	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.0f, 0.1f), &Grounded);

	EXPECT_EQ(Pos.y, 176.782898f);
	EXPECT_EQ(Vel.y, -5.0999999f);
	EXPECT_TRUE(Grounded);
}

TEST(Collision, MoveBoxReportsNoGroundWhenElasticityIsZero)
{
	CAsciiWorld World = Room();
	vec2 Pos(160.0f, 100.0f);
	vec2 Vel(0.0f, 90.0f);
	bool Grounded = false;

	// The default tuning has ground_elasticity_y 0, so this is the case real play
	// takes: MoveBox never reports ground and CCharacterCore uses IsOnGround instead.
	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.0f, 0.0f), &Grounded);

	EXPECT_EQ(Pos.y, 177.143066f);
	EXPECT_EQ(Vel.y, 0.0f);
	EXPECT_TRUE(std::signbit(Vel.y)) << "the stop comes from multiplying by -0";
	EXPECT_FALSE(Grounded);
}

TEST(Collision, MoveBoxBouncesOffAWall)
{
	CAsciiWorld World = Room();
	vec2 Pos(250.0f, 100.0f);
	vec2 Vel(51.0f, 0.0f);
	bool Grounded = false;

	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.1f, 0.1f), &Grounded);

	EXPECT_EQ(Pos.x, 269.811462f);
	EXPECT_EQ(Vel.x, -5.0999999f);
	EXPECT_FALSE(Grounded);
}

TEST(Collision, MoveBoxReflectsBothAxesWhenOnlyTheDiagonalCollides)
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
	const vec2 Size = CCharacterCore::PhysicalSizeVec2();
	vec2 Pos(145.4f, 145.4f);
	vec2 Vel(0.4f, 0.4f);
	bool Grounded = false;

	// The corner of the box cuts across the corner of the tile: moving along
	// either axis alone stays free, so neither single-axis test catches it.
	ASSERT_TRUE(World.Collision()->TestBox(Pos + Vel, Size));
	ASSERT_FALSE(World.Collision()->TestBox(vec2(Pos.x, Pos.y + Vel.y), Size));
	ASSERT_FALSE(World.Collision()->TestBox(vec2(Pos.x + Vel.x, Pos.y), Size));

	World.Collision()->MoveBox(&Pos, &Vel, Size, vec2(0.03f, 0.06f), &Grounded);

	EXPECT_EQ(Pos, vec2(145.4f, 145.4f));
	EXPECT_EQ(Vel.x, -0.0120000001f);
	EXPECT_EQ(Vel.y, -0.0240000002f);
	EXPECT_TRUE(Grounded);
}

TEST(Collision, IsOnGroundProbesFivePixelsBelowTheFeet)
{
	CAsciiWorld World = Room();
	const float Size = CCharacterCore::PhysicalSize();

	// The floor starts at y 192 and the probe rounds, so it catches at y + 14 + 5 >= 191.5.
	EXPECT_FALSE(World.Collision()->IsOnGround(vec2(160.0f, 172.0f), Size));
	EXPECT_TRUE(World.Collision()->IsOnGround(vec2(160.0f, 173.0f), Size));
}
