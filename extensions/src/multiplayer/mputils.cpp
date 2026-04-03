#include "extensions/multiplayer/mputils.h"

#include "extensions/common/customizestate.h"
#include "legoactors.h"
#include "legovideomanager.h"
#include "misc.h"
#include "roi/legoroi.h"
#include "viewmanager/viewlodlist.h"

#include <SDL3/SDL_stdinc.h>

using namespace Multiplayer;

LegoROI* Multiplayer::DeepCloneROI(LegoROI* p_source, const char* p_name)
{
	Tgl::Renderer* renderer = VideoManager()->GetRenderer();
	ViewLODList* lodList = reinterpret_cast<ViewLODList*>(const_cast<LODListBase*>(p_source->GetLODs()));

	LegoROI* clone;
	if (lodList && lodList->Size() > 0) {
		clone = new LegoROI(renderer, lodList);
	}
	else {
		clone = new LegoROI(renderer);
	}

	clone->SetName(p_name);
	clone->SetBoundingSphere(p_source->GetBoundingSphere());
	clone->WrappedSetLocal2WorldWithWorldDataUpdate(p_source->GetLocal2World());

	const CompoundObject* children = p_source->GetComp();
	if (children && !children->empty()) {
		CompoundObject* clonedChildren = new CompoundObject();
		for (CompoundObject::const_iterator it = children->begin(); it != children->end(); it++) {
			LegoROI* childSource = (LegoROI*) *it;
			const char* childName = childSource->GetName() ? childSource->GetName() : "";
			LegoROI* childClone = DeepCloneROI(childSource, childName);
			if (childClone) {
				clonedChildren->push_back(childClone);
			}
		}
		clone->SetComp(clonedChildren);
	}

	return clone;
}

// Inverse of an orthonormal affine matrix (rotation + translation).
// R^-1 = R^T, t^-1 = -R^T * t.
static void InvertOrthonormal(MxMatrix& p_out, const MxMatrix& p_in)
{
	p_out.SetIdentity();
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			p_out[r][c] = p_in[c][r];
		}
	}
	for (int c = 0; c < 3; c++) {
		p_out[3][c] = -(p_in[3][0] * p_out[0][c] + p_in[3][1] * p_out[1][c] + p_in[3][2] * p_out[2][c]);
	}
}

std::vector<MxMatrix> Multiplayer::ComputeChildOffsets(LegoROI* p_parent)
{
	std::vector<MxMatrix> offsets;
	const CompoundObject* children = p_parent->GetComp();
	if (!children) {
		return offsets;
	}

	MxMatrix parentInv;
	InvertOrthonormal(parentInv, p_parent->GetLocal2World());

	for (auto it = children->begin(); it != children->end(); it++) {
		MxMatrix offset;
		offset.Product(((LegoROI*) *it)->GetLocal2World(), parentInv);
		offsets.push_back(offset);
	}

	return offsets;
}

void Multiplayer::ApplyHierarchyTransform(
	LegoROI* p_parent,
	const MxMatrix& p_transform,
	const std::vector<MxMatrix>& p_offsets
)
{
	p_parent->WrappedSetLocal2WorldWithWorldDataUpdate(p_transform);

	const CompoundObject* children = p_parent->GetComp();
	if (!children) {
		return;
	}

	size_t i = 0;
	for (auto it = children->begin(); it != children->end() && i < p_offsets.size(); it++, i++) {
		MxMatrix childWorld;
		childWorld.Product(p_offsets[i], p_transform);
		((LegoROI*) *it)->WrappedSetLocal2WorldWithWorldDataUpdate(childWorld);
	}
}

std::string Multiplayer::TrimLODSuffix(const std::string& p_name)
{
	std::string result(p_name);
	while (result.size() > 1) {
		char c = result.back();
		if ((c >= '0' && c <= '9') || c == '_') {
			result.pop_back();
		}
		else {
			break;
		}
	}
	return result;
}

void Multiplayer::PackCustomizeState(const Extensions::Common::CustomizeState& p_state, uint8_t p_out[5])
{
	// byte 0: hatVariantIndex(5 bits) | reserved(3 bits)
	p_out[0] = (p_state.hatVariantIndex & 0x1F);

	// byte 1: sound(4 bits) | move(2 bits) | mood(2 bits)
	p_out[1] = (p_state.sound & 0x0F) | ((p_state.move & 0x03) << 4) | ((p_state.mood & 0x03) << 6);

	// byte 2: infohatColor(4 bits) | infogronColor(4 bits)
	p_out[2] = (p_state.colorIndices[c_infohatPart] & 0x0F) | ((p_state.colorIndices[c_infogronPart] & 0x0F) << 4);

	// byte 3: armlftColor(4 bits) | armrtColor(4 bits)
	p_out[3] = (p_state.colorIndices[c_armlftPart] & 0x0F) | ((p_state.colorIndices[c_armrtPart] & 0x0F) << 4);

	// byte 4: leglftColor(4 bits) | legrtColor(4 bits)
	p_out[4] = (p_state.colorIndices[c_leglftPart] & 0x0F) | ((p_state.colorIndices[c_legrtPart] & 0x0F) << 4);
}

void Multiplayer::UnpackCustomizeState(Extensions::Common::CustomizeState& p_state, const uint8_t p_in[5])
{
	// byte 0: hatVariantIndex(5 bits) | reserved(3 bits)
	p_state.hatVariantIndex = p_in[0] & 0x1F;

	// byte 1: sound(4 bits) | move(2 bits) | mood(2 bits)
	p_state.sound = p_in[1] & 0x0F;
	p_state.move = (p_in[1] >> 4) & 0x03;
	p_state.mood = (p_in[1] >> 6) & 0x03;

	// byte 2: infohatColor(4 bits) | infogronColor(4 bits)
	p_state.colorIndices[c_infohatPart] = p_in[2] & 0x0F;
	p_state.colorIndices[c_infogronPart] = (p_in[2] >> 4) & 0x0F;

	// byte 3: armlftColor(4 bits) | armrtColor(4 bits)
	p_state.colorIndices[c_armlftPart] = p_in[3] & 0x0F;
	p_state.colorIndices[c_armrtPart] = (p_in[3] >> 4) & 0x0F;

	// byte 4: leglftColor(4 bits) | legrtColor(4 bits)
	p_state.colorIndices[c_leglftPart] = p_in[4] & 0x0F;
	p_state.colorIndices[c_legrtPart] = (p_in[4] >> 4) & 0x0F;

	p_state.DeriveDependentIndices();
}

bool Multiplayer::CustomizeStatesEqual(
	const Extensions::Common::CustomizeState& p_a,
	const Extensions::Common::CustomizeState& p_b
)
{
	return SDL_memcmp(&p_a, &p_b, sizeof(Extensions::Common::CustomizeState)) == 0;
}
