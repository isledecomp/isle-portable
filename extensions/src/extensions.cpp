#include "extensions/extensions.h"

#include <SDL3/SDL_log.h>

std::vector<std::string> Extensions::enabledExtensions;

void Extensions::Enable(const char* p_key)
{
	enabledExtensions.emplace_back(p_key);

	SDL_Log("Enabled extension: %s", p_key);
}
