#pragma once

#include "extensions/extensions.h"
#include "mxtypes.h"

#include <SDL3/SDL_events.h>
#include <map>
#include <string>

class IslePathActor;
class LegoEventNotificationParam;
class LegoNavController;
class LegoPathActor;
class LegoROI;
class LegoWorld;
class Vector3;

namespace Extensions
{

namespace ThirdPersonCamera
{
class Controller;
}

class ThirdPersonCameraExt {
public:
	static void Initialize();

	static void HandleActorEnter(IslePathActor* p_actor);
	static void HandleActorExit(IslePathActor* p_actor);
	static void HandleCamAnimEnd(LegoPathActor* p_actor);
	static void OnSDLEvent(SDL_Event* p_event);
	static MxBool IsThirdPersonCameraActive();
	static MxBool HandleTouchInput(SDL_Event* p_event);
	static MxBool HandleNavOverride(
		LegoNavController* p_nav,
		const Vector3& p_curPos,
		const Vector3& p_curDir,
		Vector3& p_newPos,
		Vector3& p_newDir,
		float p_deltaTime
	);
	static MxBool HandleWorldEnable(LegoWorld* p_world, MxBool p_enable);

	static MxBool HandleROIClick(LegoROI* p_rootROI, LegoEventNotificationParam& p_param);
	static MxBool IsClonedCharacter(const char* p_name);
	static void HandleCreate();
	LEGO1_EXPORT static void HandleSDLEvent(SDL_Event* p_event);

	static ThirdPersonCamera::Controller* GetCamera();

	static std::map<std::string, std::string> options;
	static bool enabled;

private:
	static ThirdPersonCamera::Controller* s_camera;
	static bool s_registered;
	static bool s_inIsleWorld;
};

namespace TP
{
#ifdef EXTENSIONS
constexpr auto HandleCreate = &ThirdPersonCameraExt::HandleCreate;
constexpr auto HandleWorldEnable = &ThirdPersonCameraExt::HandleWorldEnable;
constexpr auto HandleActorEnter = &ThirdPersonCameraExt::HandleActorEnter;
constexpr auto HandleActorExit = &ThirdPersonCameraExt::HandleActorExit;
constexpr auto HandleCamAnimEnd = &ThirdPersonCameraExt::HandleCamAnimEnd;
constexpr auto HandleSDLEvent = &ThirdPersonCameraExt::OnSDLEvent;
constexpr auto IsThirdPersonCameraActive = &ThirdPersonCameraExt::IsThirdPersonCameraActive;
constexpr auto HandleTouchInput = &ThirdPersonCameraExt::HandleTouchInput;
constexpr auto HandleNavOverride = &ThirdPersonCameraExt::HandleNavOverride;
constexpr auto HandleROIClick = &ThirdPersonCameraExt::HandleROIClick;
constexpr auto IsClonedCharacter = &ThirdPersonCameraExt::IsClonedCharacter;
#else
constexpr decltype(&ThirdPersonCameraExt::HandleCreate) HandleCreate = nullptr;
constexpr decltype(&ThirdPersonCameraExt::HandleWorldEnable) HandleWorldEnable = nullptr;
constexpr decltype(&ThirdPersonCameraExt::HandleActorEnter) HandleActorEnter = nullptr;
constexpr decltype(&ThirdPersonCameraExt::HandleActorExit) HandleActorExit = nullptr;
constexpr decltype(&ThirdPersonCameraExt::HandleCamAnimEnd) HandleCamAnimEnd = nullptr;
constexpr decltype(&ThirdPersonCameraExt::OnSDLEvent) HandleSDLEvent = nullptr;
constexpr decltype(&ThirdPersonCameraExt::IsThirdPersonCameraActive) IsThirdPersonCameraActive = nullptr;
constexpr decltype(&ThirdPersonCameraExt::HandleTouchInput) HandleTouchInput = nullptr;
constexpr decltype(&ThirdPersonCameraExt::HandleNavOverride) HandleNavOverride = nullptr;
constexpr decltype(&ThirdPersonCameraExt::HandleROIClick) HandleROIClick = nullptr;
constexpr decltype(&ThirdPersonCameraExt::IsClonedCharacter) IsClonedCharacter = nullptr;
#endif
} // namespace TP

}; // namespace Extensions
