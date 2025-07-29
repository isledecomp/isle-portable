#include "events.h"

#include "mxdsaction.h"

#include <emscripten.h>

// clang-format off
void Emscripten_SendEvent(const char* p_event, const char* p_json)
{
    MAIN_THREAD_EM_ASM({
        const eventName = UTF8ToString($0);
        let eventDetail = {};

        if ($1 && UTF8ToString($1).length > 0) {
            eventDetail = JSON.parse(UTF8ToString($1));
        }

        const targetElement = Module.canvas || window;
        const event = new CustomEvent(eventName, {detail : eventDetail});
        targetElement.dispatchEvent(event);
    }, p_event, p_json);
}
// clang-format on

void Emscripten_SendPresenterProgress(MxDSAction* p_action, MxPresenter::TickleState p_tickleState)
{
	char buf[128];
	SDL_snprintf(
		buf,
		sizeof(buf),
		"{\"objectId\": %d, \"objectName\": \"%s\", \"tickleState\": %d}",
		p_action ? p_action->GetObjectId() : 0,
		p_action ? p_action->GetObjectName() : "",
		p_tickleState
	);

	Emscripten_SendEvent("presenterProgress", buf);
}

void Emscripten_SendExtensionProgress(const char* p_extension, MxU32 p_progress)
{
	char buf[128];
	SDL_snprintf(buf, sizeof(buf), "{\"name\": \"%s\", \"progress\": %d}", p_extension, p_progress);

	Emscripten_SendEvent("extensionProgress", buf);
}
