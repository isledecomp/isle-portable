#pragma once

#include "mxtypes.h"

#include <cstdint>

class LegoROI;

namespace Extensions
{
namespace Common
{

struct CustomizeState;

class CharacterCustomizer {
public:
	static uint8_t ResolveActorInfoIndex(uint8_t p_displayActorIndex);

	static bool SwitchColor(LegoROI* p_rootROI, uint8_t p_actorInfoIndex,
	                        CustomizeState& p_state, int p_partIndex);
	static bool SwitchVariant(LegoROI* p_rootROI, uint8_t p_actorInfoIndex,
	                          CustomizeState& p_state);
	static bool SwitchSound(CustomizeState& p_state);
	static bool SwitchMove(CustomizeState& p_state);
	static bool SwitchMood(CustomizeState& p_state);
	static void ApplyFullState(LegoROI* p_rootROI, uint8_t p_actorInfoIndex,
	                           const CustomizeState& p_state);
	static void ApplyChange(LegoROI* p_rootROI, uint8_t p_actorInfoIndex,
	                        CustomizeState& p_state, uint8_t p_changeType, uint8_t p_partIndex);
	static int MapClickedPartIndex(const char* p_partName);
	static void PlayClickSound(LegoROI* p_roi, const CustomizeState& p_state, bool p_basedOnMood);
	static MxU32 PlayClickAnimation(LegoROI* p_roi, const CustomizeState& p_state);
	static void StopClickAnimation(MxU32 p_objectId);

	// Resolves the current actor's click to a change type and optional part index.
	// Returns false if the click should be consumed with no effect (Pepper in act2/3, Brickster)
	// or if the actor is unknown.
	static bool ResolveClickChangeType(uint8_t& p_changeType, int& p_partIndex, LegoROI* p_clickedROI);

private:
	static LegoROI* FindChildROI(LegoROI* p_rootROI, const char* p_name);
	static void ApplyHatVariant(LegoROI* p_rootROI, uint8_t p_actorInfoIndex,
	                            const CustomizeState& p_state);
};

} // namespace Common
} // namespace Extensions
