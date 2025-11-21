#include "mortar/mortar_events.h"
#include "sdl3_internal.h"

#include <stdlib.h>

typedef enum {
	MORTAR_EVENT_CATEGORY_QUIT,
	MORTAR_EVENT_CATEGORY_SYSTEM,
	MORTAR_EVENT_CATEGORY_WINDOW,
	MORTAR_EVENT_CATEGORY_KEYBOARD,
	MORTAR_EVENT_CATEGORY_KEY,
	MORTAR_EVENT_CATEGORY_MOUSE_MOTION,
	MORTAR_EVENT_CATEGORY_MOUSE_BUTTON,
	MORTAR_EVENT_CATEGORY_MOUSE_DEVICE,
	MORTAR_EVENT_CATEGORY_GAMEPAD_AXIS,
	MORTAR_EVENT_CATEGORY_GAMEPAD_BUTTON,
	MORTAR_EVENT_CATEGORY_GAMEPAD_DEVICE,
	MORTAR_EVENT_CATEGORY_FINGER,
	MORTAR_EVENT_CATEGORY_USER,
} MORTAR_EventCategory;

static int eventtype_mortar_to_mortarcategory(int event)
{
	switch (event) {
	case MORTAR_EVENT_QUIT:
		return MORTAR_EVENT_CATEGORY_QUIT;
	case SDL_EVENT_TERMINATING:
		return MORTAR_EVENT_CATEGORY_SYSTEM;
	case MORTAR_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
	case MORTAR_EVENT_WINDOW_FOCUS_GAINED:
	case MORTAR_EVENT_WINDOW_FOCUS_LOST:
	case MORTAR_EVENT_WINDOW_CLOSE_REQUESTED:
		return MORTAR_EVENT_CATEGORY_WINDOW;
	case MORTAR_EVENT_KEYBOARD_ADDED:
	case MORTAR_EVENT_KEYBOARD_REMOVED:
		return MORTAR_EVENT_CATEGORY_KEYBOARD;
	case MORTAR_EVENT_KEY_DOWN:
	case MORTAR_EVENT_KEY_UP:
		return MORTAR_EVENT_CATEGORY_KEY;
	case MORTAR_EVENT_MOUSE_MOTION:
		return MORTAR_EVENT_CATEGORY_MOUSE_MOTION;
	case MORTAR_EVENT_MOUSE_BUTTON_DOWN:
	case MORTAR_EVENT_MOUSE_BUTTON_UP:
		return MORTAR_EVENT_CATEGORY_MOUSE_BUTTON;
	case MORTAR_EVENT_MOUSE_ADDED:
	case MORTAR_EVENT_MOUSE_REMOVED:
		return MORTAR_EVENT_CATEGORY_MOUSE_DEVICE;
	case MORTAR_EVENT_GAMEPAD_AXIS_MOTION:
		return MORTAR_EVENT_CATEGORY_GAMEPAD_AXIS;
	case MORTAR_EVENT_GAMEPAD_BUTTON_DOWN:
	case MORTAR_EVENT_GAMEPAD_BUTTON_UP:
		return MORTAR_EVENT_CATEGORY_GAMEPAD_BUTTON;
	case MORTAR_EVENT_GAMEPAD_ADDED:
	case MORTAR_EVENT_GAMEPAD_REMOVED:
		return MORTAR_EVENT_CATEGORY_GAMEPAD_DEVICE;
	case MORTAR_EVENT_FINGER_DOWN:
	case MORTAR_EVENT_FINGER_UP:
	case MORTAR_EVENT_FINGER_MOTION:
	case MORTAR_EVENT_FINGER_CANCELED:
		return MORTAR_EVENT_CATEGORY_FINGER;
	default:
		if (event >= MORTAR_EVENT_USER) {
			return MORTAR_EVENT_CATEGORY_USER;
		}
		abort();
	}
}

static int eventtype_mortar_to_sdl3(MORTAR_EventType eventtype)
{
	switch (eventtype) {
	case MORTAR_EVENT_QUIT:
		return SDL_EVENT_QUIT;
	case MORTAR_EVENT_TERMINATING:
		return SDL_EVENT_TERMINATING;
	case MORTAR_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		return SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
	case MORTAR_EVENT_WINDOW_FOCUS_GAINED:
		return SDL_EVENT_WINDOW_FOCUS_GAINED;
	case MORTAR_EVENT_WINDOW_FOCUS_LOST:
		return SDL_EVENT_WINDOW_FOCUS_LOST;
	case MORTAR_EVENT_WINDOW_CLOSE_REQUESTED:
		return SDL_EVENT_WINDOW_CLOSE_REQUESTED;
	case MORTAR_EVENT_KEY_DOWN:
		return SDL_EVENT_KEY_DOWN;
	case MORTAR_EVENT_KEY_UP:
		return SDL_EVENT_KEY_UP;
	case MORTAR_EVENT_KEYBOARD_ADDED:
		return SDL_EVENT_KEYBOARD_ADDED;
	case MORTAR_EVENT_KEYBOARD_REMOVED:
		return SDL_EVENT_KEYBOARD_REMOVED;
	case MORTAR_EVENT_MOUSE_MOTION:
		return SDL_EVENT_MOUSE_MOTION;
	case MORTAR_EVENT_MOUSE_BUTTON_DOWN:
		return SDL_EVENT_MOUSE_BUTTON_DOWN;
	case MORTAR_EVENT_MOUSE_BUTTON_UP:
		return SDL_EVENT_MOUSE_BUTTON_UP;
	case MORTAR_EVENT_MOUSE_ADDED:
		return SDL_EVENT_MOUSE_ADDED;
	case MORTAR_EVENT_MOUSE_REMOVED:
		return SDL_EVENT_MOUSE_REMOVED;
	case MORTAR_EVENT_GAMEPAD_AXIS_MOTION:
		return SDL_EVENT_GAMEPAD_AXIS_MOTION;
	case MORTAR_EVENT_GAMEPAD_BUTTON_DOWN:
		return SDL_EVENT_GAMEPAD_BUTTON_DOWN;
	case MORTAR_EVENT_GAMEPAD_BUTTON_UP:
		return SDL_EVENT_GAMEPAD_BUTTON_UP;
	case MORTAR_EVENT_GAMEPAD_ADDED:
		return SDL_EVENT_GAMEPAD_ADDED;
	case MORTAR_EVENT_GAMEPAD_REMOVED:
		return SDL_EVENT_GAMEPAD_REMOVED;
	case MORTAR_EVENT_FINGER_DOWN:
		return SDL_EVENT_FINGER_DOWN;
	case MORTAR_EVENT_FINGER_UP:
		return SDL_EVENT_FINGER_UP;
	case MORTAR_EVENT_FINGER_MOTION:
		return SDL_EVENT_FINGER_MOTION;
	case MORTAR_EVENT_FINGER_CANCELED:
		return SDL_EVENT_FINGER_CANCELED;
	default:
		if (eventtype >= MORTAR_EVENT_USER) {
			return (SDL_EventType) eventtype;
		}
		// abort();
		return -1;
	}
}

bool eventtype_sdl3_to_mortar(int eventtype, MORTAR_EventType* mortar_eventtype)
{
	switch (eventtype) {
	case SDL_EVENT_QUIT:
		*mortar_eventtype = MORTAR_EVENT_QUIT;
		return true;
	case SDL_EVENT_TERMINATING:
		*mortar_eventtype = MORTAR_EVENT_TERMINATING;
		return true;
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		*mortar_eventtype = MORTAR_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
		return true;
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		*mortar_eventtype = MORTAR_EVENT_WINDOW_FOCUS_GAINED;
		return true;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		*mortar_eventtype = MORTAR_EVENT_WINDOW_FOCUS_LOST;
		return true;
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		*mortar_eventtype = MORTAR_EVENT_WINDOW_CLOSE_REQUESTED;
		return true;
	case SDL_EVENT_KEY_DOWN:
		*mortar_eventtype = MORTAR_EVENT_KEY_DOWN;
		return true;
	case SDL_EVENT_KEY_UP:
		*mortar_eventtype = MORTAR_EVENT_KEY_UP;
		return true;
	case SDL_EVENT_KEYBOARD_ADDED:
		*mortar_eventtype = MORTAR_EVENT_KEYBOARD_ADDED;
		return true;
	case SDL_EVENT_KEYBOARD_REMOVED:
		*mortar_eventtype = MORTAR_EVENT_KEYBOARD_REMOVED;
		return true;
	case SDL_EVENT_MOUSE_MOTION:
		*mortar_eventtype = MORTAR_EVENT_MOUSE_MOTION;
		return true;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		*mortar_eventtype = MORTAR_EVENT_MOUSE_BUTTON_DOWN;
		return true;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		*mortar_eventtype = MORTAR_EVENT_MOUSE_BUTTON_UP;
		return true;
	case SDL_EVENT_MOUSE_ADDED:
		*mortar_eventtype = MORTAR_EVENT_MOUSE_ADDED;
		return true;
	case SDL_EVENT_MOUSE_REMOVED:
		*mortar_eventtype = MORTAR_EVENT_MOUSE_REMOVED;
		return true;
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		*mortar_eventtype = MORTAR_EVENT_GAMEPAD_AXIS_MOTION;
		return true;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		*mortar_eventtype = MORTAR_EVENT_GAMEPAD_BUTTON_DOWN;
		return true;
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
		*mortar_eventtype = MORTAR_EVENT_GAMEPAD_BUTTON_UP;
		return true;
	case SDL_EVENT_GAMEPAD_ADDED:
		*mortar_eventtype = MORTAR_EVENT_GAMEPAD_ADDED;
		return true;
	case SDL_EVENT_GAMEPAD_REMOVED:
		*mortar_eventtype = MORTAR_EVENT_GAMEPAD_REMOVED;
		return true;
	case SDL_EVENT_FINGER_DOWN:
		*mortar_eventtype = MORTAR_EVENT_FINGER_DOWN;
		return true;
	case SDL_EVENT_FINGER_UP:
		*mortar_eventtype = MORTAR_EVENT_FINGER_UP;
		return true;
	case SDL_EVENT_FINGER_MOTION:
		*mortar_eventtype = MORTAR_EVENT_FINGER_MOTION;
		return true;
	case SDL_EVENT_FINGER_CANCELED:
		*mortar_eventtype = MORTAR_EVENT_FINGER_CANCELED;
		return true;
	default:
		if (eventtype >= SDL_EVENT_USER) {
			*mortar_eventtype = (MORTAR_EventType) eventtype;
			return true;
		}
		return false;
	}
}

void event_mortar_to_sdl3(const MORTAR_Event* mortar_event, SDL_Event* sdl3_event)
{
	SDL_zerop(sdl3_event);
	sdl3_event->type = eventtype_mortar_to_sdl3(mortar_event->type);
	switch (eventtype_mortar_to_mortarcategory(mortar_event->type)) {
	case MORTAR_EVENT_CATEGORY_QUIT:
		sdl3_event->quit.timestamp = mortar_event->quit.timestamp;
		break;
	case MORTAR_EVENT_CATEGORY_SYSTEM:
		sdl3_event->common.timestamp = mortar_event->common.timestamp;
		break;
	case MORTAR_EVENT_CATEGORY_WINDOW:
		sdl3_event->window.timestamp = mortar_event->quit.timestamp;
		break;
	case MORTAR_EVENT_CATEGORY_KEYBOARD:
		sdl3_event->kdevice.timestamp = mortar_event->kdevice.timestamp;
		sdl3_event->kdevice.which = mortar_event->kdevice.which;
		break;
	case MORTAR_EVENT_CATEGORY_KEY:
		sdl3_event->key.timestamp = mortar_event->key.timestamp;
		sdl3_event->key.which = mortar_event->key.which;
		sdl3_event->key.key = keycode_mortar_to_sdl3(mortar_event->key.key);
		sdl3_event->key.mod = keymod_mortar_to_sdl3(mortar_event->key.mod);
		sdl3_event->key.repeat = mortar_event->key.repeat;
		break;
	case MORTAR_EVENT_CATEGORY_MOUSE_BUTTON:
		sdl3_event->button.timestamp = mortar_event->button.timestamp;
		sdl3_event->button.which = mortar_event->button.which;
		sdl3_event->button.x = mortar_event->button.x;
		sdl3_event->button.y = mortar_event->button.y;
		break;
	case MORTAR_EVENT_CATEGORY_MOUSE_MOTION:
		sdl3_event->motion.timestamp = mortar_event->motion.timestamp;
		sdl3_event->motion.which = mortar_event->motion.which;
		sdl3_event->motion.state = mousebuttonflags_mortar_to_sdl3(mortar_event->motion.state);
		sdl3_event->motion.x = mortar_event->motion.x;
		sdl3_event->motion.y = mortar_event->motion.y;
		break;
	case MORTAR_EVENT_CATEGORY_MOUSE_DEVICE:
		sdl3_event->mdevice.timestamp = mortar_event->mdevice.timestamp;
		sdl3_event->mdevice.which = mortar_event->mdevice.which;
		break;
	case MORTAR_EVENT_CATEGORY_GAMEPAD_AXIS:
		sdl3_event->gaxis.timestamp = mortar_event->gaxis.timestamp;
		sdl3_event->gaxis.which = mortar_event->gaxis.which;
		sdl3_event->gaxis.axis = mortar_event->gaxis.axis;
		sdl3_event->gaxis.value = mortar_event->gaxis.value;
		break;
	case MORTAR_EVENT_CATEGORY_GAMEPAD_BUTTON:
		sdl3_event->gbutton.timestamp = mortar_event->gbutton.timestamp;
		sdl3_event->gbutton.which = mortar_event->gbutton.which;
		sdl3_event->gbutton.button = mortar_event->gbutton.button;
		break;
	case MORTAR_EVENT_CATEGORY_GAMEPAD_DEVICE:
		sdl3_event->jdevice.timestamp = mortar_event->jdevice.timestamp;
		sdl3_event->jdevice.which = mortar_event->jdevice.which;
		break;
	case MORTAR_EVENT_CATEGORY_FINGER:
		sdl3_event->tfinger.timestamp = mortar_event->tfinger.timestamp;
		sdl3_event->tfinger.touchID = mortar_event->tfinger.touchID;
		sdl3_event->tfinger.fingerID = mortar_event->tfinger.fingerID;
		sdl3_event->tfinger.x = mortar_event->tfinger.x;
		sdl3_event->tfinger.y = mortar_event->tfinger.y;
		break;
	case MORTAR_EVENT_CATEGORY_USER:
		sdl3_event->user.timestamp = mortar_event->user.timestamp;
		sdl3_event->user.code = mortar_event->user.code;
		sdl3_event->user.data1 = mortar_event->user.data1;
		sdl3_event->user.data2 = mortar_event->user.data2;
		break;
	default:
		abort();
	}
}

bool event_sdl3_to_mortar(const SDL_Event* sdl3_event, MORTAR_Event* mortar_event)
{
	bool success;
	SDL_zerop(mortar_event);
	if (!eventtype_sdl3_to_mortar(sdl3_event->type, &mortar_event->type)) {
		return false;
	}
	switch (eventtype_mortar_to_mortarcategory(mortar_event->type)) {
	case MORTAR_EVENT_CATEGORY_QUIT:
		mortar_event->quit.timestamp = sdl3_event->quit.timestamp;
		break;
	case MORTAR_EVENT_CATEGORY_WINDOW:
		mortar_event->window.timestamp = sdl3_event->quit.timestamp;
		break;
	case MORTAR_EVENT_CATEGORY_KEYBOARD:
		mortar_event->kdevice.timestamp = sdl3_event->kdevice.timestamp;
		mortar_event->kdevice.which = sdl3_event->kdevice.which;
		break;
	case MORTAR_EVENT_CATEGORY_KEY:
		mortar_event->key.timestamp = sdl3_event->key.timestamp;
		mortar_event->key.which = sdl3_event->key.which;
		mortar_event->key.key = keycode_sdl3_to_mortar(sdl3_event->key.key);
		mortar_event->key.mod = keymod_sdl3_to_mortar(sdl3_event->key.mod);
		mortar_event->key.repeat = sdl3_event->key.repeat;
		break;
	case MORTAR_EVENT_CATEGORY_MOUSE_BUTTON:
		mortar_event->button.timestamp = sdl3_event->button.timestamp;
		mortar_event->button.which = sdl3_event->button.which;
		mortar_event->button.x = sdl3_event->button.x;
		mortar_event->button.y = sdl3_event->button.y;
		break;
	case MORTAR_EVENT_CATEGORY_MOUSE_MOTION:
		mortar_event->motion.timestamp = sdl3_event->motion.timestamp;
		mortar_event->motion.which = sdl3_event->motion.which;
		mortar_event->motion.state = mousebuttonflags_sdl3_to_mortar(sdl3_event->motion.state);
		mortar_event->motion.x = sdl3_event->motion.x;
		mortar_event->motion.y = sdl3_event->motion.y;
		break;
	case MORTAR_EVENT_CATEGORY_MOUSE_DEVICE:
		mortar_event->mdevice.timestamp = sdl3_event->mdevice.timestamp;
		mortar_event->mdevice.which = sdl3_event->mdevice.which;
		break;
	case MORTAR_EVENT_CATEGORY_GAMEPAD_AXIS:
		mortar_event->gaxis.timestamp = sdl3_event->gaxis.timestamp;
		mortar_event->gaxis.which = sdl3_event->gaxis.which;
		mortar_event->gaxis.axis = sdl3_event->gaxis.axis;
		mortar_event->gaxis.value = sdl3_event->gaxis.value;
		break;
	case MORTAR_EVENT_CATEGORY_GAMEPAD_BUTTON:
		mortar_event->gbutton.timestamp = sdl3_event->gbutton.timestamp;
		mortar_event->gbutton.which = sdl3_event->gbutton.which;
		mortar_event->gbutton.button = sdl3_event->gbutton.button;
		break;
	case MORTAR_EVENT_CATEGORY_GAMEPAD_DEVICE:
		mortar_event->jdevice.timestamp = sdl3_event->jdevice.timestamp;
		mortar_event->jdevice.which = sdl3_event->jdevice.which;
		break;
	case MORTAR_EVENT_CATEGORY_FINGER:
		mortar_event->tfinger.timestamp = sdl3_event->tfinger.timestamp;
		mortar_event->tfinger.touchID = sdl3_event->tfinger.touchID;
		mortar_event->tfinger.fingerID = sdl3_event->tfinger.fingerID;
		mortar_event->tfinger.x = sdl3_event->tfinger.x;
		mortar_event->tfinger.y = sdl3_event->tfinger.y;
		break;
	case MORTAR_EVENT_CATEGORY_USER:
		mortar_event->user.timestamp = sdl3_event->user.timestamp;
		mortar_event->user.code = sdl3_event->user.code;
		mortar_event->user.data1 = sdl3_event->user.data1;
		mortar_event->user.data2 = sdl3_event->user.data2;
		break;
	default:
		return false;
	}
	return true;
}

bool MORTAR_PushEvent(MORTAR_Event* event)
{
	SDL_Event sdl3_event;
	event_mortar_to_sdl3(event, &sdl3_event);
	return SDL_PushEvent(&sdl3_event);
}

uint32_t MORTAR_RegisterEvents(int numevents)
{
	return SDL_RegisterEvents(numevents);
}

bool MORTAR_AddEventWatch(MORTAR_EventFilter filter, void* userdata)
{
	return SDL_AddEventWatch((SDL_EventFilter) filter, userdata);
}
