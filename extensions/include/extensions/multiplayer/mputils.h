#pragma once

#include "mxgeometry/mxmatrix.h"
#include "mxtypes.h"

#include <cstdint>
#include <string>
#include <vector>

class LegoROI;

namespace Extensions
{
namespace Common
{
struct CustomizeState;
}
} // namespace Extensions

namespace Multiplayer
{

// Broadcast world ID indicating the player is not visible in any world.
static constexpr int8_t WORLD_NOT_VISIBLE = -1;

// Deep-clone an ROI hierarchy (geometry shared via LOD refcount, independent transforms).
LegoROI* DeepCloneROI(LegoROI* p_source, const char* p_name);

// Compute child-to-parent local offsets for a hierarchical ROI.
std::vector<MxMatrix> ComputeChildOffsets(LegoROI* p_parent);

// Apply a world transform to a parent ROI and recompute child transforms from saved offsets.
void ApplyHierarchyTransform(LegoROI* p_parent, const MxMatrix& p_transform, const std::vector<MxMatrix>& p_offsets);

// Trim trailing digits and underscores from a model name to get its LOD base name.
std::string TrimLODSuffix(const std::string& p_name);

// Serialize a CustomizeState into a 5-byte buffer.
void PackCustomizeState(const Extensions::Common::CustomizeState& p_state, uint8_t p_out[5]);

// Deserialize a CustomizeState from a 5-byte buffer.
void UnpackCustomizeState(Extensions::Common::CustomizeState& p_state, const uint8_t p_in[5]);

// Compare two CustomizeState instances for equality.
bool CustomizeStatesEqual(const Extensions::Common::CustomizeState& p_a, const Extensions::Common::CustomizeState& p_b);

} // namespace Multiplayer
