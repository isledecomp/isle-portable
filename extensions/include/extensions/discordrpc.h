#pragma once

#include "extensions.h"
#include "../../LEGO1/lego1_export.h"

#include <SDL3/SDL.h>
#include <string>

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
#define DISCORD_RPC_SUPPORTED 1
#else
#define DISCORD_RPC_SUPPORTED 0
#endif

namespace DiscordRPC
{
extern bool enabled;

// Discord RPC configuration
struct RPCConfig {
    const char* applicationId;
    const char* largeImageKey;
    const char* largeImageText;
    const char* smallImageKey;
    const char* smallImageText;
};

// Game state information for RPC
struct GameStateInfo {
    std::string currentAct;
    std::string currentArea;
    std::string currentActor;
    std::string activity;
    int64_t startTime;
    bool isPlaying;
};

#if DISCORD_RPC_SUPPORTED
LEGO1_EXPORT void Initialize();
LEGO1_EXPORT void Shutdown();
LEGO1_EXPORT void UpdatePresence(const GameStateInfo& gameState);
LEGO1_EXPORT void RunCallbacks();
#else
inline void Initialize() {}
inline void Shutdown() {}
inline void UpdatePresence(const GameStateInfo&) {}
inline void RunCallbacks() {}
#endif

// Helper functions
std::string GetActName(int act);
std::string GetAreaName(int area);
std::string GetActorName(int actorId);
std::string GetActivityDescription(const GameStateInfo& gameState);

} // namespace DiscordRPC 