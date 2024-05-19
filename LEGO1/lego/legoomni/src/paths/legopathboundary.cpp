#include "legopathboundary.h"

#include "decomp.h"
#include "geom/legounkown100db7f4.h"
#include "legopathactor.h"

DECOMP_SIZE_ASSERT(LegoPathBoundary, 0x74)

// FUNCTION: LEGO1 0x10056a70
// FUNCTION: BETA10 0x100b1360
LegoPathBoundary::LegoPathBoundary()
{
}

// FUNCTION: LEGO1 0x10057260
// FUNCTION: BETA10 0x100b140d
LegoPathBoundary::~LegoPathBoundary()
{
	for (LegoPathActorSet::iterator it = m_actors.begin(); !(it == m_actors.end()); it++) {
		(*it)->SetBoundary(NULL);
	}

	m_actors.erase(m_actors.begin(), m_actors.end());
}

// FUNCTION: LEGO1 0x100573f0
// FUNCTION: BETA10 0x100b1536
MxResult LegoPathBoundary::AddActor(LegoPathActor* p_actor)
{
	m_actors.insert(p_actor);
	p_actor->SetBoundary(this);
	return SUCCESS;
}

// FUNCTION: LEGO1 0x100574a0
// FUNCTION: BETA10 0x100b156f
MxResult LegoPathBoundary::RemoveActor(LegoPathActor* p_actor)
{
	m_actors.erase(p_actor);
	return SUCCESS;
}

// STUB: LEGO1 0x100575b0
void LegoPathBoundary::FUN_100575b0(Vector3& p_point1, Vector3& p_point2, LegoPathActor* p_actor)
{
}

// FUNCTION: LEGO1 0x10057950
// FUNCTION: BETA10 0x100b1adc
MxU32 LegoPathBoundary::Intersect(
	float p_scale,
	Vector3& p_point1,
	Vector3& p_point2,
	Vector3& p_point3,
	LegoEdge*& p_edge
)
{
	LegoUnknown100db7f4* e = NULL;
	float localc;
	MxU32 local10 = 0;
	float len = 0.0f;
	Mx3DPointFloat vec;

	for (MxS32 i = 0; i < m_numEdges; i++) {
		LegoUnknown100db7f4* edge = (LegoUnknown100db7f4*) m_edges[i];

		if (p_point2.Dot(&m_edgeNormals[i], &p_point2) + m_edgeNormals[i][3] <= -1e-07) {
			if (local10 == 0) {
				local10 = 1;
				vec = p_point2;
				((Vector3&) vec).Sub(&p_point1);

				len = vec.LenSquared();
				if (len <= 0.0f) {
					return 0;
				}

				len = sqrt(len);
				((Vector3&) vec).Div(len);
			}

			float dot = vec.Dot(&vec, &m_edgeNormals[i]);
			if (dot != 0.0f) {
				float local34 = (-m_edgeNormals[i][3] - p_point1.Dot(&p_point1, &m_edgeNormals[i])) / dot;

				if (local34 >= -0.001 && local34 <= len && (e == NULL || local34 < localc)) {
					e = edge;
					localc = local34;
				}
			}
		}
	}

	if (e != NULL) {
		if (localc < 0.0f) {
			localc = 0.0f;
		}

		Mx3DPointFloat local50;
		Mx3DPointFloat local70;
		Vector3* local5c = e->GetOpposingPoint(this);

		p_point3 = vec;
		p_point3.Mul(localc);
		p_point3.Add(&p_point1);

		local50 = p_point2;
		((Vector3&) local50).Sub(local5c);

		e->FUN_1002ddc0(*this, local70);

		float local58 = local50.Dot(&local50, &local70);
		LegoUnknown100db7f4* local54 = NULL;

		if (local58 < 0.0f) {
			Mx3DPointFloat local84;

			for (LegoUnknown100db7f4* local88 = (LegoUnknown100db7f4*) e->GetClockwiseEdge(this); e != local88;
				 local88 = (LegoUnknown100db7f4*) local88->GetClockwiseEdge(this)) {
				local88->FUN_1002ddc0(*this, local84);

				if (local84.Dot(&local84, &local70) <= 0.9) {
					break;
				}

				Vector3* local90 = local88->GetOpposingPoint(this);
				Mx3DPointFloat locala4(p_point3);
				((Vector3&) locala4).Sub(local90);

				float local8c = locala4.Dot(&locala4, &local84);

				if (local8c > local58 && local8c < local88->m_unk0x3c) {
					local54 = local88;
					local58 = local8c;
					local70 = local84;
					local5c = local90;
				}
			}
		}
		else {
			if (e->m_unk0x3c < local58) {
				Mx3DPointFloat localbc;

				for (LegoUnknown100db7f4* locala8 = (LegoUnknown100db7f4*) e->GetCounterclockwiseEdge(this);
					 e != locala8;
					 locala8 = (LegoUnknown100db7f4*) locala8->GetCounterclockwiseEdge(this)) {
					locala8->FUN_1002ddc0(*this, localbc);

					if (localbc.Dot(&localbc, &local70) <= 0.9) {
						break;
					}

					Vector3* localc4 = locala8->GetOpposingPoint(this);
					Mx3DPointFloat locald8(p_point3);
					((Vector3&) locald8).Sub(localc4);

					float localc0 = locald8.Dot(&locald8, &localbc);

					if (localc0 < local58 && localc0 >= 0.0f) {
						local54 = locala8;
						local58 = localc0;
						local70 = localbc;
						local5c = localc4;
					}
				}
			}
		}

		if (local54 != NULL) {
			e = local54;
		}

		if (local58 <= 0.0f) {
			if (!e->GetMask0x03()) {
				p_edge = e->GetClockwiseEdge(this);
			}
			else {
				p_edge = e;
			}

			p_point3 = *local5c;
			return 2;
		}
		else if (local58 > 0.0f && e->m_unk0x3c > local58) {
			p_point3 = local70;
			p_point3.Mul(local58);
			p_point3.Add(local5c);
			p_edge = e;
			return 1;
		}
		else {
			p_point3 = *e->GetPoint(this);

			if (!e->GetMask0x03()) {
				p_edge = e->GetCounterclockwiseEdge(this);
			}
			else {
				p_edge = e;
			}

			return 2;
		}
	}

	return 0;
}

// STUB: LEGO1 0x10057fe0
// FUNCTION: BETA10 0x100b2220
MxU32 LegoPathBoundary::FUN_10057fe0(LegoAnimPresenter* p_presenter)
{
	// TODO
	return 0;
}

// STUB: LEGO1 0x100586e0
// FUNCTION: BETA10 0x100b22d1
MxU32 LegoPathBoundary::FUN_100586e0(LegoAnimPresenter* p_presenter)
{
	// TODO
	return 0;
}
