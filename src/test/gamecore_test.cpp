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

	EXPECT_FALSE(StandsOnSurface(World.Collision(), vec2(160.0f, 172.0f), Size, DefaultGravityDown()));
	EXPECT_TRUE(StandsOnSurface(World.Collision(), vec2(160.0f, 173.0f), Size, DefaultGravityDown()));
}

TEST(GameCore, GroundIsWhicheverWayGravityPoints)
{
	CAsciiWorld World = Room();
	const vec2 Size = CCharacterCore::PhysicalSizeVec2();
	const vec2 UnderTheCeiling = vec2(160.0f, 50.0f);

	EXPECT_FALSE(StandsOnSurface(World.Collision(), UnderTheCeiling, Size, DefaultGravityDown()));
	EXPECT_TRUE(StandsOnSurface(World.Collision(), UnderTheCeiling, Size, DefaultGravityDown().Opposite()));
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
	EXPECT_TRUE(StandsOnSurface(World.Collision(), vec2(85.0f, 173.0f), Size, DefaultGravityDown()));
	EXPECT_FALSE(StandsOnSurface(World.Collision(), vec2(120.0f, 173.0f), Size, DefaultGravityDown()));
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

TEST(GameCore, LeftAndRightFollowTheBodysOwnDown)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Upright = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	CCharacterCore Inverted = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	Inverted.SetGravity(GRAVITY_UP);
	Upright.m_Input.m_Direction = 1;
	Inverted.m_Input.m_Direction = 1;

	Upright.Tick(true);
	Inverted.Tick(true);

	// The same input moves each body the same way in its own frame, which is opposite
	// ways in the world's. Gravity mirrors with it.
	EXPECT_EQ(Upright.m_Vel, vec2(1.5f, 0.5f));
	EXPECT_EQ(Inverted.m_Vel, vec2(-1.5f, -0.5f));
}

TEST(GameCore, ADirectionBuiltFromAnyLengthIsTheUnitOne)
{
	// The invariant the axis operations rest on: they read a component with a dot
	// product, which only measures one when the direction is unit.
	EXPECT_EQ(CDirection2::Normalized(vec2(0.0f, 5.0f)).Unit(), vec2(0.0f, 1.0f));
	EXPECT_EQ(CDirection2::Normalized(vec2(-3.0f, 0.0f)).Unit(), vec2(-1.0f, 0.0f));
}

TEST(GameCore, SetAlongLandsExactlyOnTheValueItIsGiven)
{
	// A tee five ticks into a fall, then jumping. Writing the impulse as a difference
	// added to the speed it already has misses it by 1e-6 at exactly this speed.
	vec2 V(3.0f, 2.4000001f);

	SetAlong(V, DefaultGravityDown(), -13.1999998f);

	EXPECT_EQ(V.y, -13.1999998f);
	EXPECT_EQ(V.x, 3.0f);
}

TEST(GameCore, ScaleAlongScalesExactlyTheComponentItIsGiven)
{
	// A tee drifting at 3.9 under air friction. Writing the scale as a difference added
	// to the speed it already has misses it by 2e-7 at exactly this speed.
	vec2 V(2.4000001f, 3.9000001f);

	ScaleAlong(V, DefaultGravityDown(), 0.94999999f);

	EXPECT_EQ(V.y, 3.70500016f);
	EXPECT_EQ(V.x, 2.4000001f);
}

TEST(GameCore, TheCardinalGravityPresetsAreExactlyTheCardinalDirections)
{
	// The exactness the axis operations rest on: a cardinal down must be the cardinal
	// vector itself, not a rounded rotation of one.
	EXPECT_EQ(ResolveGravity(GRAVITY_DOWN).Unit(), vec2(0.0f, 1.0f));
	EXPECT_EQ(ResolveGravity(GRAVITY_RIGHT).Unit(), vec2(1.0f, 0.0f));
	EXPECT_EQ(ResolveGravity(GRAVITY_UP).Unit(), vec2(0.0f, -1.0f));
	EXPECT_EQ(ResolveGravity(GRAVITY_LEFT).Unit(), vec2(-1.0f, 0.0f));
}

TEST(GameCore, EveryGravityPresetIsAUnitDirection)
{
	for(int Preset = 0; Preset < NUM_GRAVITY_PRESETS; Preset++)
	{
		EXPECT_NEAR(length(ResolveGravity((EGravityPreset)Preset).Unit()), 1.0f, 1e-6f) << "preset " << Preset;
	}
}

TEST(GameCore, TwoPresetsApartIsAQuarterTurnTowardsTheBodysRight)
{
	// The order is an eighth turn per step, in the same sense as Side(). Inserting a
	// preset out of order would break the wire value's meaning, so it is checked here.
	for(int Preset = 0; Preset < NUM_GRAVITY_PRESETS; Preset++)
	{
		const int QuarterTurn = (Preset + 2) % NUM_GRAVITY_PRESETS;
		EXPECT_EQ(ResolveGravity((EGravityPreset)Preset).Side().Unit(), ResolveGravity((EGravityPreset)QuarterTurn).Unit()) << "preset " << Preset;
	}
}

TEST(GameCore, ABodyFallsUnderTheGravityItsInputAsksFor)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	Core.m_Input.m_WantedGravity = AskForGravity(GRAVITY_UP);

	Core.Tick(true);

	EXPECT_EQ(Core.m_GravityDown.Unit(), vec2(0.0f, -1.0f));
	// The new gravity applies on the very tick that asks for it, not the next one.
	EXPECT_EQ(Core.m_Vel.y, -0.5f);
}

TEST(GameCore, AGravityNobodyDefinedLeavesTheBodyFallingAsItWas)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	const vec2 Down = DefaultGravityDown().Unit();

	for(const int Wanted : {-1, AskForGravity(GRAVITY_DOWN_LEFT) + 1, 12345})
	{
		CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
		Core.m_Input.m_WantedGravity = Wanted;

		Core.Tick(true);

		EXPECT_EQ(Core.m_GravityDown.Unit(), Down) << "wanted " << Wanted;
	}
}

TEST(GameCore, ACoreTickedWithoutInputKeepsItsGravity)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	Core.m_Input.m_WantedGravity = AskForGravity(GRAVITY_UP);

	Core.Tick(false);

	EXPECT_EQ(Core.m_GravityDown.Unit(), DefaultGravityDown().Unit());
}

TEST(GameCore, AnInputThatAsksForNothingLeavesTheBodyOnTheGravityItHad)
{
	// The zero an old client sends, and the one a body keeps when its gravity was set
	// by something other than its own input.
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	Core.SetGravity(GRAVITY_LEFT);
	Core.m_Input.m_WantedGravity = 0;

	Core.Tick(true);

	EXPECT_EQ(Core.m_GravityDown.Unit(), vec2(-1.0f, 0.0f));
}

TEST(GameCore, ABodyFallingDownReadsItsOwnFrameAsTheWorlds)
{
	// The exactness the whole migration rests on: under normal gravity the change
	// of frame must not move a single bit, or every aim upstream ever recorded
	// would land somewhere else.
	const CDirection2 Down = ResolveGravity(GRAVITY_DOWN);
	for(const vec2 V : {vec2(1.0f, 0.0f), vec2(0.0f, -1.0f), vec2(137.699997f, -42.4000015f), vec2(-0.30000001f, 0.69999999f)})
	{
		EXPECT_EQ(FromBodyFrame(V, Down), V);
	}
}

TEST(GameCore, AimingRightMeansTheBodysRightNotTheWorlds)
{
	// Down is the body's +y and Side its +x, so the frame turns whole rather than
	// each axis on its own.
	EXPECT_EQ(FromBodyFrame(vec2(1.0f, 0.0f), ResolveGravity(GRAVITY_UP)), vec2(-1.0f, 0.0f));
	EXPECT_EQ(FromBodyFrame(vec2(0.0f, 1.0f), ResolveGravity(GRAVITY_UP)), vec2(0.0f, -1.0f));
	// Falling right, the body's own right points up the screen.
	EXPECT_EQ(FromBodyFrame(vec2(1.0f, 0.0f), ResolveGravity(GRAVITY_RIGHT)), vec2(0.0f, -1.0f));
	EXPECT_EQ(FromBodyFrame(vec2(0.0f, 1.0f), ResolveGravity(GRAVITY_RIGHT)), vec2(1.0f, 0.0f));
}

namespace {

vec2 HookDirectionAfterAiming(CAsciiWorld &World, CWorldCore *pWorldCore, EGravityPreset Preset, int AimX, int AimY)
{
	CCharacterCore Core = SpawnedAt(pWorldCore, World.Collision(), MID_ROOM);
	Core.SetGravity(Preset);
	Core.m_Input.m_Hook = 1;
	Core.m_Input.m_TargetX = AimX;
	Core.m_Input.m_TargetY = AimY;
	Core.Tick(true);
	return Core.m_HookDir;
}

} // namespace

TEST(GameCore, TheHookIsThrownWhereTheBodyAimsNotWhereTheWorldDoes)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;

	// The same "straight ahead, to my right" throws opposite ways in the world.
	EXPECT_EQ(HookDirectionAfterAiming(World, &WorldCore, GRAVITY_DOWN, 100, 0), vec2(1.0f, 0.0f));
	EXPECT_EQ(HookDirectionAfterAiming(World, &WorldCore, GRAVITY_UP, 100, 0), vec2(-1.0f, 0.0f));
	// And "towards my feet" follows the feet.
	EXPECT_EQ(HookDirectionAfterAiming(World, &WorldCore, GRAVITY_RIGHT, 0, 100), vec2(1.0f, 0.0f));
}

TEST(GameCore, TheAimAngleIsUnchangedForABodyFallingDown)
{
	// m_Angle crosses the wire and every client draws the arm from it, so the change
	// of frame owes it the same integer upstream produced. This aim is one of the 22
	// pairs in 4 million where computing the arc tangent in float rather than double
	// lands on a different m_Angle -- upstream passes ints, which promote to double.
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	Core.m_Input.m_TargetX = 646;
	Core.m_Input.m_TargetY = -682;

	Core.Tick(true);

	EXPECT_EQ(Core.m_Angle, -207);
}

TEST(GameCore, TheGravityPresetAndTheDirectionMoveTogether)
{
	// The preset is what crosses the wire and the direction is what the physics
	// reads. They are one state, so only SetGravity writes them.
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	EXPECT_EQ(Core.m_Gravity, GRAVITY_DOWN);

	Core.SetGravity(GRAVITY_UP_LEFT);

	EXPECT_EQ(Core.m_Gravity, GRAVITY_UP_LEFT);
	EXPECT_EQ(Core.m_GravityDown.Unit(), ResolveGravity(GRAVITY_UP_LEFT).Unit());
}

TEST(GameCore, AnInputThatAsksForAGravityMovesThePresetTheWireCarries)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	Core.m_Input.m_WantedGravity = AskForGravity(GRAVITY_RIGHT);

	Core.Tick(true);

	EXPECT_EQ(Core.m_Gravity, GRAVITY_RIGHT);
}

TEST(GameCore, TurningCarriesTheBodysMomentumIntoItsNewFrame)
{
	// The half a turning camera cannot do on its own: a body that kept its world
	// velocity would appear to reverse the instant the view turns under it.
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	Core.m_Vel = vec2(10.0f, 0.0f);

	Core.TurnTo(GRAVITY_UP);

	// Still ten to its own right, which is the world's left once it hangs over.
	EXPECT_EQ(Core.m_Vel, vec2(-10.0f, 0.0f));
	EXPECT_EQ(Along(Core.m_Vel, Core.m_GravityDown.Side()), 10.0f);
}

TEST(GameCore, TurningToTheGravityAlreadyHeldIsExactlyNothing)
{
	// m_WantedGravity is a standing wish that never returns to zero, so the turn is
	// re-applied every tick. A diagonal frame does not recompose to the same floats,
	// so a turn that was not a no-op here would drift the velocity every tick -- and
	// drift it only on the body, never on the input-free core the server reckons with.
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	Core.m_Vel = vec2(3.0f, -4.0f);
	Core.TurnTo(GRAVITY_UP_LEFT);
	const vec2 Turned = Core.m_Vel;

	for(int i = 0; i < 10; i++)
	{
		Core.TurnTo(GRAVITY_UP_LEFT);
	}

	EXPECT_EQ(Core.m_Vel, Turned);
}

TEST(GameCore, TheWireSetsTheFrameWithoutTurningTheBody)
{
	// A snapshot carries a velocity the server has already turned. Turning it again
	// on the way in would turn it twice.
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	Core.m_Vel = vec2(10.0f, 0.0f);

	Core.SetGravity(GRAVITY_UP);

	EXPECT_EQ(Core.m_Vel, vec2(10.0f, 0.0f));
	EXPECT_EQ(Core.m_Gravity, GRAVITY_UP);
}

TEST(GameCore, AnInputThatAsksForAGravityTurnsTheBodyItMoves)
{
	CAsciiWorld World = Room();
	CWorldCore WorldCore;
	CCharacterCore Core = SpawnedAt(&WorldCore, World.Collision(), MID_ROOM);
	Core.m_Vel = vec2(10.0f, 0.0f);
	Core.m_Input.m_WantedGravity = AskForGravity(GRAVITY_UP);

	Core.Tick(true);

	// Turned first, then one tick of the new gravity and of air friction, both read
	// on the body's own axes.
	EXPECT_EQ(Core.m_Vel, vec2(-9.5f, -0.5f));
}
