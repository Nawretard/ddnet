#include "ascii_world.h"

#include <base/vmath.h>

#include <game/collision.h>
#include <game/gamecore.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

// Characterization tests: they freeze what MoveBox and IsOnGround currently do,
// not what they ought to do. The expected values were read off the unmodified
// implementation, and the velocities and elasticities are deliberately ones
// whose products round, so that a rewrite which is algebraically equivalent but
// rounds elsewhere still fails.

namespace {

// Collects what MoveBox reports, so a test can assert on the whole sequence.
struct SContactLog
{
	std::vector<SContact> m_vContacts;

	static void Record(const SContact &Contact, void *pUser)
	{
		static_cast<SContactLog *>(pUser)->m_vContacts.push_back(Contact);
	}
};

} // namespace

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

TEST(Collision, MoveBoxBouncesOffTheFloor)
{
	CAsciiWorld World = Room();
	vec2 Pos(160.0f, 130.0f);
	vec2 Vel(0.0f, 51.0f);

	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.0f, 0.1f));

	EXPECT_EQ(Pos.y, 176.782898f);
	EXPECT_EQ(Vel.y, -5.0999999f);
}

TEST(Collision, MoveBoxStopsDeadWhenElasticityIsZero)
{
	CAsciiWorld World = Room();
	vec2 Pos(160.0f, 100.0f);
	vec2 Vel(0.0f, 90.0f);

	// The default tuning has ground_elasticity_y 0, so this is the case real play takes.
	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.0f, 0.0f));

	EXPECT_EQ(Pos.y, 177.143066f);
	EXPECT_EQ(Vel.y, 0.0f);
	EXPECT_TRUE(std::signbit(Vel.y)) << "the stop comes from multiplying by -0";
}

TEST(Collision, MoveBoxBouncesOffAWall)
{
	CAsciiWorld World = Room();
	vec2 Pos(250.0f, 100.0f);
	vec2 Vel(51.0f, 0.0f);

	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.1f, 0.1f));

	EXPECT_EQ(Pos.x, 269.811462f);
	EXPECT_EQ(Vel.x, -5.0999999f);
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

	// The corner of the box cuts across the corner of the tile: moving along
	// either axis alone stays free, so neither single-axis test catches it.
	ASSERT_TRUE(World.Collision()->TestBox(Pos + Vel, Size));
	ASSERT_FALSE(World.Collision()->TestBox(vec2(Pos.x, Pos.y + Vel.y), Size));
	ASSERT_FALSE(World.Collision()->TestBox(vec2(Pos.x + Vel.x, Pos.y), Size));

	World.Collision()->MoveBox(&Pos, &Vel, Size, vec2(0.03f, 0.06f));

	EXPECT_EQ(Pos, vec2(145.4f, 145.4f));
	EXPECT_EQ(Vel.x, -0.0120000001f);
	EXPECT_EQ(Vel.y, -0.0240000002f);
}

TEST(Collision, IsOnGroundProbesFivePixelsBelowTheFeet)
{
	CAsciiWorld World = Room();
	const float Size = CCharacterCore::PhysicalSize();

	// The floor starts at y 192 and the probe rounds, so it catches at y + 14 + 5 >= 191.5.
	EXPECT_FALSE(World.Collision()->IsOnGround(vec2(160.0f, 172.0f), Size));
	EXPECT_TRUE(World.Collision()->IsOnGround(vec2(160.0f, 173.0f), Size));
}

TEST(Collision, MoveBoxReportsNoContactOverEmptySpace)
{
	CAsciiWorld World = Room();
	vec2 Pos(160.0f, 100.0f);
	vec2 Vel(0.0f, 5.0f);
	SContactLog Log;

	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.0f, 0.0f), SContactLog::Record, &Log);

	EXPECT_TRUE(Log.m_vContacts.empty());
}

TEST(Collision, MoveBoxReportsAFloorContactPointingUp)
{
	CAsciiWorld World = Room();
	vec2 Pos(160.0f, 130.0f);
	vec2 Vel(0.0f, 51.0f);
	SContactLog Log;

	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.0f, 0.1f), SContactLog::Record, &Log);

	ASSERT_EQ(Log.m_vContacts.size(), 1u);
	EXPECT_EQ(Log.m_vContacts[0].Normal, vec2(0.0f, -1.0f));
	// Both bottom corners are over floor tiles, so the point is the middle of that
	// face, caught just past the y 192 the floor starts at.
	EXPECT_EQ(Log.m_vContacts[0].Point, vec2(160.0f, 192.057922f));
	EXPECT_EQ(Log.m_vContacts[0].Material, TILE_SOLID);
}

TEST(Collision, MoveBoxReportsAWallContactPointingAwayFromTheWall)
{
	CAsciiWorld World = Room();
	vec2 Pos(250.0f, 100.0f);
	vec2 Vel(51.0f, 0.0f);
	SContactLog Log;

	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.1f, 0.1f), SContactLog::Record, &Log);

	ASSERT_EQ(Log.m_vContacts.size(), 1u);
	EXPECT_EQ(Log.m_vContacts[0].Normal, vec2(-1.0f, 0.0f));
	EXPECT_EQ(Log.m_vContacts[0].Point, vec2(287.538574f, 100.0f));
	EXPECT_EQ(Log.m_vContacts[0].Material, TILE_SOLID);
}

TEST(Collision, MoveBoxReportsBothFacesForTheSingleCornerPoint)
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
	vec2 Pos(145.4f, 145.4f);
	vec2 Vel(0.4f, 0.4f);
	SContactLog Log;

	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.03f, 0.06f), SContactLog::Record, &Log);

	// One corner of the box sits in the tile, and it belongs to two faces at once.
	ASSERT_EQ(Log.m_vContacts.size(), 2u);
	EXPECT_EQ(Log.m_vContacts[0].Normal, vec2(0.0f, -1.0f));
	EXPECT_EQ(Log.m_vContacts[1].Normal, vec2(-1.0f, 0.0f));
	EXPECT_EQ(Log.m_vContacts[0].Point, Log.m_vContacts[1].Point);
	EXPECT_EQ(Log.m_vContacts[0].Point, vec2(159.799988f, 159.799988f));
}

TEST(Collision, MoveBoxReportsACeilingContactPointingDown)
{
	CAsciiWorld World = Room();
	vec2 Pos(160.0f, 90.0f);
	vec2 Vel(0.0f, -51.0f);
	SContactLog Log;

	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.0f, 0.1f), SContactLog::Record, &Log);

	// The normal follows the face the box leads with, so going up it flips.
	ASSERT_EQ(Log.m_vContacts.size(), 1u);
	EXPECT_EQ(Log.m_vContacts[0].Normal, vec2(0.0f, 1.0f));
	EXPECT_EQ(Log.m_vContacts[0].Point, vec2(160.0f, 30.8846741f));
	EXPECT_EQ(Log.m_vContacts[0].Material, TILE_SOLID);
}

TEST(Collision, MoveBoxReportsALeftWallContactPointingRight)
{
	CAsciiWorld World = Room();
	vec2 Pos(70.0f, 100.0f);
	vec2 Vel(-51.0f, 0.0f);
	SContactLog Log;

	World.Collision()->MoveBox(&Pos, &Vel, CCharacterCore::PhysicalSizeVec2(), vec2(0.1f, 0.1f), SContactLog::Record, &Log);

	ASSERT_EQ(Log.m_vContacts.size(), 1u);
	EXPECT_EQ(Log.m_vContacts[0].Normal, vec2(1.0f, 0.0f));
	EXPECT_EQ(Log.m_vContacts[0].Point, vec2(31.4807701f, 100.0f));
	EXPECT_EQ(Log.m_vContacts[0].Material, TILE_SOLID);
}
