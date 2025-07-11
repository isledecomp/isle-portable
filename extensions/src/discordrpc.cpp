#include "extensions/discordrpc.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_log.h>
#include <ctime>

// Discord RPC library
#include <discord_rpc.h>

namespace DiscordRPC
{
bool enabled = false;

// Discord RPC configuration
// This includes a already provided application ID, but you can change it if you want
static const RPCConfig rpcConfig = {
    "1392967803421589544", // If needed, replace with your Discord application ID
    "lego_island_logo",    // Large image key
    "LEGO Island",         // Large image text
    "playing",             // Small image key
    "Playing"              // Small image text
};

// Current game state
static GameStateInfo currentGameState;
static bool isInitialized = false;

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
void Initialize()
{
    if (!enabled || isInitialized) {
        return;
    }

    SDL_Log("Initializing Discord RPC...");
    
    Discord_Initialize(rpcConfig.applicationId, nullptr, 1, nullptr);
    isInitialized = true;
    
    // Set initial presence
    currentGameState.isPlaying = false;
    // Set start time to current UNIX timestamp (seconds since epoch)
    currentGameState.startTime = time(NULL);
    UpdatePresence(currentGameState);
    
    SDL_Log("Discord RPC initialized successfully");
}

void Shutdown()
{
    if (!enabled || !isInitialized) {
        return;
    }

    SDL_Log("Shutting down Discord RPC...");
    Discord_Shutdown();
    isInitialized = false;
}

void UpdatePresence(const GameStateInfo& gameState)
{
    if (!enabled || !isInitialized) {
        return;
    }

    currentGameState = gameState;

    DiscordRichPresence presence = {};
    presence.state = gameState.activity.c_str();
    presence.details = gameState.currentAct.c_str();
    presence.largeImageKey = rpcConfig.largeImageKey;
    presence.largeImageText = rpcConfig.largeImageText;
    presence.smallImageKey = rpcConfig.smallImageKey;
    presence.smallImageText = rpcConfig.smallImageText;
    
    if (gameState.isPlaying) {
        presence.startTimestamp = gameState.startTime;
    }

    Discord_UpdatePresence(&presence);
}

void RunCallbacks()
{
    if (!enabled || !isInitialized) {
        return;
    }

    Discord_RunCallbacks();
}

std::string GetActName(int act)
{
    switch (act) {
    case 0:
        return "Act 1: Welcome to LEGO Island";
    case 1:
        return "Act 2: The Brickster's Revenge";
    case 2:
        return "Act 3: The Final Showdown";
    default:
        return "Unknown Act";
    }
}

std::string GetAreaName(int area)
{
    switch (area) {
    case 1: return "LEGO Island";
    case 2: return "Information Center";
    case 3: return "Information Door";
    case 4: return "Elevator Bottom";
    case 5: return "Elevator Ride";
    case 6: return "Elevator Ride 2";
    case 7: return "Elevator Open";
    case 8: return "Sea View";
    case 9: return "Observation Deck";
    case 10: return "Elevator Down";
    case 11: return "Registration Book";
    case 12: return "Information Score";
    case 13: return "Jet Ski Race";
    case 14: return "Jet Ski Race 2";
    case 15: return "Jet Ski Race Exterior";
    case 16: return "Jet Ski Building Exited";
    case 17: return "Car Race";
    case 18: return "Car Race Exterior";
    case 19: return "Race Car Building Exited";
    case 20: return "Pizzeria Exterior";
    case 21: return "Garage Exterior";
    case 22: return "Garage";
    case 23: return "Garage Door";
    case 24: return "Garage Exited";
    case 25: return "Hospital Exterior";
    case 26: return "Hospital";
    case 27: return "Hospital Exited";
    case 28: return "Police Exterior";
    case 29: return "Police Exited";
    case 30: return "Police Station";
    case 31: return "Police Door";
    case 32: return "Copter Building";
    case 33: return "Dune Car Building";
    case 34: return "Jet Ski Building";
    case 35: return "Race Car Building";
    case 36: return "Act 2 Main";
    case 37: return "Act 3 Script";
    case 38: return "Jukebox";
    case 39: return "Jukebox Exterior";
    case 40: return "History Book";
    case 41: return "Bike";
    case 42: return "Dune Car";
    case 43: return "Motorcycle";
    case 44: return "Copter";
    case 45: return "Skateboard";
    case 46: return "Ambulance";
    case 47: return "Tow Track";
    case 48: return "Jet Ski";
    default: return "Unknown Area";
    }
}

std::string GetActorName(int actorId)
{
    switch (actorId) {
    case 0: return "Pepper Roni";
    case 1: return "Mama";
    case 2: return "Papa";
    case 3: return "Nick";
    case 4: return "Laura";
    default: return "Unknown Character";
    }
}

std::string GetActivityDescription(const GameStateInfo& gameState)
{
    if (!gameState.isPlaying) {
        return "In Menu";
    }

    std::string activity = "Playing as " + gameState.currentActor;
    
    if (!gameState.currentArea.empty()) {
        activity += " in " + gameState.currentArea;
    }
    
    return activity;
}
#endif // desktop platforms

} // namespace DiscordRPC 