#ifndef GAME_FRAME_H
#define GAME_FRAME_H

#include <base/vmath.h>

// A body's own frame: a direction to call "down", and the reading of any vector on
// the two axes that follow from it.
//
// Nothing here knows about gravity, a tee, or the map -- it is the geometry a body
// carries, and it is separate for that reason: the concept is complete on its own,
// and it changes for reasons that have nothing to do with the ones gamecore.h
// changes for.

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

#endif // GAME_FRAME_H
