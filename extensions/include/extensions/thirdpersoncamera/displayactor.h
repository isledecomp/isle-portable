#pragma once

#include "extensions/common/customizestate.h"

#include <cstdint>

class LegoROI;

namespace Extensions
{
namespace ThirdPersonCamera
{

class DisplayActor {
public:
	DisplayActor();

	void SetDisplayActorIndex(uint8_t p_displayActorIndex);
	uint8_t GetDisplayActorIndex() const { return m_displayActorIndex; }

	bool EnsureDisplayROI();
	void CreateDisplayClone();
	void DestroyDisplayClone();

	bool HasDisplayOverride() const { return m_displayROI != nullptr; }
	LegoROI* GetDisplayROI() const { return m_displayROI; }

	Common::CustomizeState& GetCustomizeState() { return m_customizeState; }

	void ApplyCustomizeChange(uint8_t p_changeType, uint8_t p_partIndex);

	void SyncTransformFromNative(LegoROI* p_nativeROI);

	void FreezeDisplayActor() { m_displayActorFrozen = true; }
	void UnfreezeDisplayActor() { m_displayActorFrozen = false; }
	bool IsDisplayActorFrozen() const { return m_displayActorFrozen; }

private:
	uint8_t m_displayActorIndex;
	bool m_displayActorFrozen;
	LegoROI* m_displayROI;
	char m_displayUniqueName[32];
	Common::CustomizeState m_customizeState;
};

} // namespace ThirdPersonCamera
} // namespace Extensions
