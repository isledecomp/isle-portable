#include "extensions/thirdpersoncamera.h"

#include "extensions/common/charactercustomizer.h"
#include "extensions/thirdpersoncamera/controller.h"
#include "islepathactor.h"
#include "legoeventnotificationparam.h"
#include "legonavcontroller.h"
#include "legopathactor.h"
#include "legovideomanager.h"
#include "misc.h"
#include "mxcore.h"
#include "mxmisc.h"
#include "mxticklemanager.h"
#include "realtime/vector.h"
#include "roi/legoroi.h"

#include <SDL3/SDL_stdinc.h>

using namespace Extensions;
using namespace Extensions::Common;

std::map<std::string, std::string> ThirdPersonCameraExt::options;
bool ThirdPersonCameraExt::enabled = false;
ThirdPersonCamera::Controller* ThirdPersonCameraExt::s_camera = nullptr;
bool ThirdPersonCameraExt::s_registered = false;
bool ThirdPersonCameraExt::s_inIsleWorld = false;

namespace Extensions
{
namespace ThirdPersonCamera
{

class TickleAdapter : public MxCore {
public:
	TickleAdapter(Controller* p_camera) : m_camera(p_camera) {}

	MxResult Tickle() override
	{
		if (m_camera) {
			m_camera->Tick(0.016f);
		}
		return SUCCESS;
	}

	const char* ClassName() const override { return "ThirdPersonCamera::TickleAdapter"; }

private:
	Controller* m_camera;
};

} // namespace ThirdPersonCamera
} // namespace Extensions

static Extensions::ThirdPersonCamera::TickleAdapter* s_tickleAdapter = nullptr;

void ThirdPersonCameraExt::Initialize()
{
	if (!s_camera) {
		s_camera = new ThirdPersonCamera::Controller();
	}
	s_camera->Enable();
}

void ThirdPersonCameraExt::HandleCreate()
{
	if (!s_registered && s_camera) {
		s_tickleAdapter = new Extensions::ThirdPersonCamera::TickleAdapter(s_camera);
		TickleManager()->RegisterClient(s_tickleAdapter, 10);
		s_registered = true;
	}
}

MxBool ThirdPersonCameraExt::HandleWorldEnable(LegoWorld* p_world, MxBool p_enable)
{
	if (!s_camera) {
		return FALSE;
	}

	if (p_enable) {
		s_camera->OnWorldEnabled(p_world);
		s_inIsleWorld = true;
	}
	else {
		s_camera->OnWorldDisabled(p_world);
		s_inIsleWorld = false;
	}

	return TRUE;
}

void ThirdPersonCameraExt::HandleActorEnter(IslePathActor* p_actor)
{
	if (s_camera) {
		s_camera->OnActorEnter(p_actor);
	}
}

void ThirdPersonCameraExt::HandleActorExit(IslePathActor* p_actor)
{
	if (s_camera) {
		s_camera->OnActorExit(p_actor);
	}
}

void ThirdPersonCameraExt::HandleCamAnimEnd(LegoPathActor* p_actor)
{
	if (s_camera) {
		s_camera->OnCamAnimEnd(p_actor);
	}
}

void ThirdPersonCameraExt::OnSDLEvent(SDL_Event* p_event)
{
	if (!s_camera || !s_inIsleWorld) {
		return;
	}

	s_camera->HandleSDLEventImpl(p_event);

	if (s_camera->ConsumeAutoDisable()) {
		s_camera->Disable();
	}
	else if (s_camera->ConsumeAutoEnable()) {
		s_camera->ResetTouchState();
		s_camera->SetOrbitDistance(ThirdPersonCamera::Controller::MIN_DISTANCE);
		s_camera->Enable();
	}
}

MxBool ThirdPersonCameraExt::IsThirdPersonCameraActive()
{
	if (s_camera && s_camera->IsActive()) {
		return TRUE;
	}

	return FALSE;
}

MxBool ThirdPersonCameraExt::HandleTouchInput(SDL_Event* p_event)
{
	if (!s_camera || !s_camera->IsActive()) {
		return FALSE;
	}

	switch (p_event->type) {
	case SDL_EVENT_FINGER_DOWN:
		if (s_camera->TryClaimFinger(p_event->tfinger)) {
			return TRUE;
		}
		return FALSE;

	case SDL_EVENT_FINGER_MOTION:
		if (s_camera->IsFingerTracked(p_event->tfinger.fingerID)) {
			return TRUE;
		}
		return FALSE;

	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED:
		if (s_camera->TryReleaseFinger(p_event->tfinger.fingerID)) {
			return TRUE;
		}
		return FALSE;

	default:
		return FALSE;
	}
}

MxBool ThirdPersonCameraExt::HandleNavOverride(
	LegoNavController* p_nav,
	const Vector3& p_curPos,
	const Vector3& p_curDir,
	Vector3& p_newPos,
	Vector3& p_newDir,
	float p_deltaTime
)
{
	if (!s_camera) {
		return FALSE;
	}

	if (!s_camera->IsActive()) {
		return FALSE;
	}

	return s_camera->HandleCameraRelativeMovement(p_nav, p_curPos, p_curDir, p_newPos, p_newDir, p_deltaTime);
}

MxBool ThirdPersonCameraExt::HandleROIClick(LegoROI* p_rootROI, LegoEventNotificationParam& p_param)
{
	if (!s_camera) {
		return FALSE;
	}

	if (!s_camera->GetDisplayROI() || s_camera->GetDisplayROI() != p_rootROI) {
		return FALSE;
	}

	uint8_t changeType;
	int partIndex;
	if (!CharacterCustomizer::ResolveClickChangeType(changeType, partIndex, p_param.GetROI())) {
		return TRUE;
	}

	s_camera->ApplyCustomizeChange(changeType, static_cast<uint8_t>(partIndex >= 0 ? partIndex : 0xFF));

	LegoROI* effectROI = s_camera->GetDisplayROI();
	if (effectROI) {
		CharacterCustomizer::PlayClickSound(effectROI, s_camera->GetCustomizeState(), changeType == CHANGE_MOOD);
		if (!s_camera->IsInVehicle() && !s_camera->IsInMultiPartEmote()) {
			s_camera->StopClickAnimation();
			MxU32 clickAnimId = CharacterCustomizer::PlayClickAnimation(effectROI, s_camera->GetCustomizeState());
			s_camera->SetClickAnimObjectId(clickAnimId);
		}
	}
	return TRUE;
}

MxBool ThirdPersonCameraExt::IsClonedCharacter(const char* p_name)
{
	if (!s_camera) {
		return FALSE;
	}

	if (s_camera->GetDisplayROI() != nullptr && !SDL_strcasecmp(s_camera->GetDisplayROI()->GetName(), p_name)) {
		return TRUE;
	}

	return FALSE;
}

ThirdPersonCamera::Controller* ThirdPersonCameraExt::GetCamera()
{
	return s_camera;
}

void ThirdPersonCameraExt::HandleSDLEvent(SDL_Event* p_event)
{
	Extension<ThirdPersonCameraExt>::Call(TP::HandleSDLEvent, p_event);
}
