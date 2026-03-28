#include "extensions/multiplayer/protocol.h"

#include "legogamestate.h"
#include "misc.h"

#include <SDL3/SDL_stdinc.h>

namespace Multiplayer
{

void EncodeUsername(char p_out[USERNAME_BUFFER_SIZE])
{
	SDL_memset(p_out, 0, USERNAME_BUFFER_SIZE);
	LegoGameState* gs = GameState();
	if (gs && gs->m_playerCount > 0) {
		const LegoGameState::Username& username = gs->m_players[0];
		for (int i = 0; i < 7; i++) {
			MxS16 letter = username.m_letters[i];
			if (letter < 0) {
				break;
			}
			if (letter <= 25) {
				p_out[i] = (char) ('A' + letter);
			}
			else {
				p_out[i] = '?';
			}
		}
	}
}

} // namespace Multiplayer
