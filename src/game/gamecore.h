/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_GAMECORE_H
#define GAME_GAMECORE_H

#include "prng.h"

#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/teamscore.h>

#include <limits>
#include <optional>
#include <set>
#include <vector>

class CCollision;
class CTeamsCore;
struct SContact;

class CTuneParam
{
	int m_Value;

public:
	int Get() const { return m_Value; }
	CTuneParam &operator=(float v)
	{
		const float Fixed = v * 100.0f;
		if(Fixed >= static_cast<float>(std::numeric_limits<int>::min()) && Fixed < static_cast<float>(std::numeric_limits<int>::max()))
			m_Value = static_cast<int>(Fixed);
		else
			m_Value = std::numeric_limits<int>::min();
		return *this;
	}
	operator float() const { return m_Value / 100.0f; }
};

class CTuningParams
{
	static const char *ms_apNames[];

public:
	CTuningParams()
	{
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) m_##Name = (Value);
#include "tuning.h"
#undef MACRO_TUNING_PARAM
	}

#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) CTuneParam m_##Name;
#include "tuning.h"
#undef MACRO_TUNING_PARAM

	static int Num()
	{
		return sizeof(CTuningParams) / sizeof(int);
	}
	int *NetworkArray() { return (int *)this; }
	const int *NetworkArray() const { return (const int *)this; }
	bool Set(int Index, float Value);
	bool Set(const char *pName, float Value);
	bool Get(int Index, float *pValue) const;
	bool Get(const char *pName, float *pValue) const;
	static const char *Name(int Index) { return ms_apNames[Index]; }
	float GetWeaponFireDelay(int Weapon) const;

	static const CTuningParams DEFAULT;
};

// Do not use these function unless for legacy code!
void StrToInts(int *pInts, size_t NumInts, const char *pStr);
bool IntsToStr(const int *pInts, size_t NumInts, char *pStr, size_t StrSize);

inline vec2 CalcPos(vec2 Pos, vec2 Velocity, float Curvature, float Speed, float Time)
{
	vec2 n;
	Time *= Speed;
	n.x = Pos.x + Velocity.x * Time;
	n.y = Pos.y + Velocity.y * Time + Curvature / 10000 * (Time * Time);
	return n;
}

template<typename T>
inline T SaturatedAdd(T Min, T Max, T Current, T Modifier)
{
	if(Modifier < 0)
	{
		if(Current < Min)
			return Current;
		Current += Modifier;
		if(Current < Min)
			Current = Min;
		return Current;
	}
	else
	{
		if(Current > Max)
			return Current;
		Current += Modifier;
		if(Current > Max)
			Current = Max;
		return Current;
	}
}

float VelocityRamp(float Value, float Start, float Range, float Curvature);

// hooking stuff
enum
{
	HOOK_RETRACTED = -1,
	HOOK_IDLE = 0,
	HOOK_RETRACT_START = 1,
	HOOK_RETRACT_END = 3,
	HOOK_FLYING = 4,
	HOOK_GRABBED = 5,

	COREEVENT_GROUND_JUMP = 0x01,
	COREEVENT_AIR_JUMP = 0x02,
	COREEVENT_HOOK_LAUNCH = 0x04,
	COREEVENT_HOOK_ATTACH_PLAYER = 0x08,
	COREEVENT_HOOK_ATTACH_GROUND = 0x10,
	COREEVENT_HOOK_HIT_NOHOOK = 0x20,
	COREEVENT_HOOK_RETRACT = 0x40,
};

// show others values - do not change them
enum
{
	SHOW_OTHERS_NOT_SET = -1, // show others value before it is set
	SHOW_OTHERS_OFF = 0, // show no other players in solo or other teams
	SHOW_OTHERS_ON = 1, // show all other players in solo and other teams
	SHOW_OTHERS_ONLY_TEAM = 2 // show players that are in solo and are in the same team
};

struct SSwitchers
{
	bool m_aStatus[NUM_DDRACE_TEAMS];
	bool m_Initial;
	int m_aEndTick[NUM_DDRACE_TEAMS];
	int m_aType[NUM_DDRACE_TEAMS];
	int m_aLastUpdateTick[NUM_DDRACE_TEAMS];
};

class CWorldCore
{
public:
	CWorldCore()
	{
		for(auto &pCharacter : m_apCharacters)
		{
			pCharacter = nullptr;
		}
		m_pPrng = nullptr;
	}

	int RandomOr0(int BelowThis) // NOLINT(readability-make-member-function-const)
	{
		if(BelowThis <= 1 || !m_pPrng)
		{
			return 0;
		}
		// This makes the random number slightly biased if `BelowThis`
		// is not a power of two, but we have decided that this is not
		// significant for DDNet and favored the simple implementation.
		return m_pPrng->RandomBits() % BelowThis;
	}

	class CCharacterCore *m_apCharacters[MAX_CLIENTS];
	CPrng *m_pPrng;

	void InitSwitchers(int HighestSwitchNumber);
	std::vector<SSwitchers> m_vSwitchers;
};

typedef std::function<void(int ClientId, bool DisallowReset)> FAntiPingInterfereCallback;

// A unit direction. The axis operations below read a component with a dot product, which
// only measures one when the axis is normalized, so the only way in normalizes.
class CDirection2
{
	vec2 m_Unit;

	constexpr explicit CDirection2(vec2 Unit) :
		m_Unit(Unit) {}

public:
	static CDirection2 Normalized(vec2 V) { return CDirection2(normalize(V)); }

	vec2 Unit() const { return m_Unit; }

	// Turning or flipping a unit vector leaves it unit, so neither renormalizes.
	CDirection2 Opposite() const { return CDirection2(-m_Unit); }
	// The axis a body moves along under left/right input, pointing right when this
	// points down. It is a fixed quarter turn, so a body's left and right always
	// follow its own down rather than the world's.
	CDirection2 Side() const { return CDirection2(vec2(m_Unit.y, -m_Unit.x)); }
};

// The directions gravity can be set to. Their order is an eighth turn per step, in the
// same sense as CDirection2::Side, so two presets apart is a quarter turn to the right.
enum EGravityPreset
{
	GRAVITY_DOWN = 0,
	GRAVITY_DOWN_RIGHT,
	GRAVITY_RIGHT,
	GRAVITY_UP_RIGHT,
	GRAVITY_UP,
	GRAVITY_UP_LEFT,
	GRAVITY_LEFT,
	GRAVITY_DOWN_LEFT,
	NUM_GRAVITY_PRESETS,
};

CDirection2 ResolveGravity(EGravityPreset Preset);

inline bool IsGravityPreset(int Value) { return Value >= 0 && Value < NUM_GRAVITY_PRESETS; }

// The wire shape of a gravity wish, the same one m_WantedWeapon uses: zero asks for
// nothing, so a client that never heard of gravity leaves the body's own alone.
inline int AskForGravity(EGravityPreset Preset) { return Preset + 1; }

inline std::optional<EGravityPreset> GravityAskedFor(int Wanted)
{
	if(!IsGravityPreset(Wanted - 1))
		return std::nullopt;
	return (EGravityPreset)(Wanted - 1);
}

// The way a body falls until something gives it another one.
inline CDirection2 DefaultGravityDown() { return ResolveGravity(GRAVITY_DOWN); }

// How far past its feet a body still counts as standing on a surface.
constexpr float GROUND_REACH = 5.0f;

// Whether a surface facing against gravity is within reach of the body's feet. This is
// the geometric half of the question; CCharacter::IsGrounded also accepts a blocking tile.
bool StandsOnSurface(const CCollision *pCollision, vec2 Pos, vec2 Size, CDirection2 Down);

// A direction the body expressed in its own frame, brought back into the world's:
// the body's +x is its Side and its +y its Down. Exactly the identity when the body
// falls down, so nothing an ordinary tee aims at rounds differently than it used to.
inline vec2 FromBodyFrame(vec2 V, CDirection2 Down)
{
	return Down.Side().Unit() * V.x + Down.Unit() * V.y;
}

// Reading and writing one component of a vector along an axis.
inline float Along(vec2 V, CDirection2 Axis)
{
	return dot(V, Axis.Unit());
}

// Rebuilt from the axis and its perpendicular rather than by adding the difference to
// V. Both are the same algebra, but only this one lands on the exact same float as the
// hand-written `V.y = Value` when the axis is cardinal.
inline void SetAlong(vec2 &V, CDirection2 Axis, float Value)
{
	const vec2 Side = Axis.Side().Unit();
	V = Side * dot(V, Side) + Axis.Unit() * Value;
}

inline void AddAlong(vec2 &V, CDirection2 Axis, float Value)
{
	V += Axis.Unit() * Value;
}

inline void ScaleAlong(vec2 &V, CDirection2 Axis, float Factor)
{
	const vec2 Side = Axis.Side().Unit();
	V = Side * dot(V, Side) + Axis.Unit() * (dot(V, Axis.Unit()) * Factor);
}

// The inverse of FromBodyFrame: a world vector read the way the body reads it. The
// frame is orthonormal, so the inverse is a component on each of its axes.
inline vec2 ToBodyFrame(vec2 V, CDirection2 Down)
{
	return vec2(Along(V, Down.Side()), Along(V, Down));
}

// A surface answers with the elasticity of the axis its normal lies on.
inline float ElasticityAlong(vec2 Normal, vec2 Elasticity)
{
	return Normal.x != 0.0f ? Elasticity.x : Elasticity.y;
}

// DDNet's answer to touching a surface: the velocity component along the contact
// normal flips, scaled by that surface's elasticity. Written per axis rather than as
// a reflection off the normal, because the two are the same algebra but do not round
// the same way.
void BounceOffContact(const SContact &Contact, vec2 Elasticity, vec2 *pVel);

// What a moving body does with the contacts a sweep reports: it bounces off them, and
// a surface facing against gravity puts it back on the ground.
struct SBodyContacts
{
	vec2 m_Elasticity;
	CDirection2 m_Down = DefaultGravityDown();
	bool m_Grounded = false;

	static void OnContact(const SContact &Contact, vec2 *pVel, void *pUser);
};

class CCharacterCore
{
	CWorldCore *m_pWorld = nullptr;
	CCollision *m_pCollision;

public:
	static constexpr float PhysicalSize() { return 28.0f; }
	static constexpr vec2 PhysicalSizeVec2() { return vec2(28.0f, 28.0f); }
	vec2 m_Pos;
	vec2 m_Vel;

	vec2 m_HookPos;
	vec2 m_HookDir;
	vec2 m_HookTeleBase;
	int m_HookTick;
	int m_HookState;
	std::set<int> m_AttachedPlayers;
	int HookedPlayer() const { return m_HookedPlayer; }
	void SetHookedPlayer(int HookedPlayer);

	int m_ActiveWeapon;
	class CWeaponStat
	{
	public:
		int m_AmmoRegenStart;
		int m_Ammo;
		int m_Ammocost;
		bool m_Got;
	} m_aWeapons[NUM_WEAPONS];

	// ninja
	struct
	{
		vec2 m_ActivationDir;
		int m_ActivationTick;
		int m_CurrentMoveTime;
		int m_OldVelAmount;
	} m_Ninja;

	bool m_NewHook;

	int m_Jumped;
	// m_JumpedTotal counts the jumps performed in the air
	int m_JumpedTotal;
	int m_Jumps;

	int m_Direction;
	int m_Angle;

	// The way this body falls, as the preset it was chosen from and as the direction
	// its physics reads. Both are written only by SetGravity, so they cannot drift.
	EGravityPreset m_Gravity = GRAVITY_DOWN;
	CDirection2 m_GravityDown = DefaultGravityDown();
	CNetObj_PlayerInput m_Input;

	int m_TriggeredEvents;

	void Init(CWorldCore *pWorld, CCollision *pCollision, CTeamsCore *pTeams = nullptr);
	void SetCoreWorld(CWorldCore *pWorld, CCollision *pCollision, CTeamsCore *pTeams);
	// Sets the frame and moves nothing: what the wire and a reset do to a body.
	void SetGravity(EGravityPreset Preset);
	// The body itself turning: its momentum comes along into the new frame.
	void TurnTo(EGravityPreset Preset);
	void Reset();
	void TickDeferred();
	void Tick(bool UseInput, bool DoDeferredTick = true);
	void Move();

	void Read(const CNetObj_CharacterCore *pObjCore);
	void Write(CNetObj_CharacterCore *pObjCore) const;
	void Quantize();

	// DDRace
	int m_Id;
	bool m_Reset;
	CCollision *Collision() { return m_pCollision; }

	int m_Colliding;
	bool m_LeftWall;

	// DDNet Character
	void SetTeamsCore(CTeamsCore *pTeams);
	void ReadDDNet(const CNetObj_DDNetCharacter *pObjDDNet);
	bool m_Solo;
	bool m_Jetpack;
	bool m_CollisionDisabled;
	bool m_EndlessHook;
	bool m_EndlessJump;
	bool m_HammerHitDisabled;
	bool m_GrenadeHitDisabled;
	bool m_LaserHitDisabled;
	bool m_ShotgunHitDisabled;
	bool m_HookHitDisabled;
	bool m_Super;
	bool m_Invincible;
	bool m_HasTelegunGun;
	bool m_HasTelegunGrenade;
	bool m_HasTelegunLaser;
	int m_FreezeStart;
	int m_FreezeEnd;
	bool m_IsInFreeze;
	bool m_DeepFrozen;
	bool m_LiveFrozen;
	CTuningParams m_Tuning;

	// clientside only: antiping
	void SetAntiPingInterfereCallback(FAntiPingInterfereCallback Callback);

private:
	CTeamsCore *m_pTeams;
	int m_MoveRestrictions;
	int m_HookedPlayer;
	static bool IsSwitchActiveCb(unsigned char Number, void *pUser);

	FAntiPingInterfereCallback m_AntiPingInterfereCallback = [](int ClientId, bool DisallowReset) {};
};

// Whether a client could tell these two cores apart, which is what dead reckoning
// asks: the item a snapshot carries of a core is all a client is given, so anything
// the two differ in beyond it cannot reach one.
bool SameToAClient(const CCharacterCore &A, const CCharacterCore &B);

// input count
struct CInputCount
{
	int m_Presses;
	int m_Releases;
};

inline CInputCount CountInput(int Prev, int Cur)
{
	CInputCount c = {0, 0};
	Prev &= INPUT_STATE_MASK;
	Cur &= INPUT_STATE_MASK;
	int i = Prev;

	while(i != Cur)
	{
		i = (i + 1) & INPUT_STATE_MASK;
		if(i & 1)
			c.m_Presses++;
		else
			c.m_Releases++;
	}

	return c;
}

#endif
