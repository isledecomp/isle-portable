#pragma once

#include <cstdint>

namespace Extensions
{
namespace Common
{

struct CustomizeState {
	uint8_t colorIndices[10] = {}; // m_nameIndex per body part (matching LegoActorInfo::Part::m_nameIndex)
	uint8_t hatVariantIndex = 0;   // m_partNameIndex for infohat part
	uint8_t sound = 0;             // 0 to 8
	uint8_t move = 0;              // 0 to 3
	uint8_t mood = 0;              // 0 to 3

	void InitFromActorInfo(uint8_t p_actorInfoIndex);
	void DeriveDependentIndices();
};

} // namespace Common
} // namespace Extensions
