# Multiplayer MVP Feasibility Assessment for isle-portable

## Context

LEGO Island is a single-player game from 1997. The isle-portable project is a cross-platform decompilation/port running on Emscripten (WebAssembly), Windows, macOS, Linux, 3DS, Switch, Vita, iOS, Android, and Xbox. The goal is to assess feasibility and plan an MVP where players can see each other's characters walking around in the ISLE overworld. The approach prioritizes **maximum reuse of existing game code** and a **WebSocket relay via Cloudflare Workers**.

The original game shipped with DirectPlay headers (`3rdparty/dx5/inc/dplay.h`) but never implemented multiplayer. There is **zero existing networking code**.

---

## Feasibility Assessment: STRONG YES

The codebase is well-suited for this feature because:

1. **Character creation is already abstracted**: `LegoCharacterManager` builds fully-textured, multi-part character models (head, body, arms, legs with configurable textures/colors) on demand with ref counting.

2. **Transform system is clean**: `OrientableROI::SetLocal2WorldWithWorldDataUpdate(matrix)` positions any ROI in the world using a 4x4 matrix. No need to interact with path-following or collision systems.

3. **Animation is static and reusable**: `LegoROI::ApplyAnimationTransformation()` is a static function that animates any set of ROIs given an animation tree and time value - no `LegoAnimActor` or entity required.

4. **The game loop is hookable**: `MxTickleManager` allows any `MxCore` subclass to register for periodic callbacks at arbitrary intervals.

5. **Extensions pattern exists**: The `extensions/` system provides a clean model for conditionally-compiled features with runtime enable/disable.

6. **Emscripten support is mature**: pthreads, WebGL2, WASMFS, and JS interop patterns (`EM_JS`, `Emscripten_SendEvent`) are already established.

7. **The data to sync is tiny**: ~48 bytes per player at 10-15Hz = ~3 KB/s for 4 players.

**Key constraint**: Duplicate characters require a new `CreateCharacterClone()` function (see below), but it's a straightforward ~100 line addition. Vehicle cloning uses the existing `CreateAutoROI()` function with no core code changes needed.

---

## Scope: ISLE World Only

Only the main ISLE world (the island overworld) is relevant for multiplayer. All other areas (Act 1/2/3, Infocenter, Police/Fire/Hospital stations, races, etc.) are instanced single-player experiences.

This simplifies the design significantly:
- Remote players are only visible when **both** players are in the ISLE world
- Remote players are hidden whenever the local player enters any non-ISLE area
- Only one set of animation presenters to find (the ISLE world's)
- No cross-world state tracking needed

---

## The Duplicate Character Problem

**Question**: Can two players be the same character (e.g., both Pepper)?

**Answer**: Not with the existing API. `LegoCharacterManager::GetActorROI()` uses a name-keyed map (`m_characters`) and returns the **same shared ROI** for duplicate calls. One ROI cannot be in two positions simultaneously.

**Root cause** (`legocharactermanager.cpp:250-296`):
```cpp
LegoROI* LegoCharacterManager::GetActorROI(const char* p_name, ...) {
    it = m_characters->find(const_cast<char*>(p_name));
    if (!(it == m_characters->end())) {
        character = (*it).second;
        character->AddRef();  // just bumps refcount, same ROI
    }
    return character->m_roi;  // shared instance
}
```

Additionally, `CreateActorROI()` writes `info->m_roi = roi` into the global `g_actorInfo[66]` table, which has exactly one slot per character type.

**Solution: `CreateCharacterClone()` function**

Add a new method to `LegoCharacterManager` that decouples appearance lookup from ROI identity:

```cpp
// New function - reuses CreateActorROI's construction logic
// but with a unique name and no side effects on g_actorInfo
LegoROI* LegoCharacterManager::CreateCharacterClone(
    const char* p_uniqueName,    // e.g., "pepper_mp_42"
    const char* p_characterType  // e.g., "pepper" (for appearance lookup)
) {
    LegoActorInfo* info = GetActorInfo(p_characterType);
    if (!info) return NULL;

    // Same body construction loop as CreateActorROI():
    // - Create root ROI with p_uniqueName
    // - Build child ROIs (body, hat, head, arms, legs) from info->m_parts
    // - Apply textures and colors from g_actorInfo
    // - Store in m_characters under p_uniqueName
    // - Do NOT set info->m_roi (no global table side effect)
    // ~100 lines, mostly copied from CreateActorROI lines 459-603
}
```

This is one of only two modifications to existing LEGO1 code needed (the other being 2 extension hook lines in `LegoWorld::Enable()`). Multiple "pepper_mp_1", "pepper_mp_2" etc. ROIs can coexist independently.

**Alternative (simpler but limiting)**: Enforce unique character selection per room (max 6 players, one per character). This avoids the problem entirely but limits room size and player choice.

---

## Existing Systems to Reuse

| System | Key Class/Function | File | How It's Reused |
|--------|-------------------|------|-----------------|
| Character visuals | `LegoCharacterManager::CreateCharacterClone(uniqueName, type)` | `legocharactermanager.h` (new) | Creates independent ROI clone for remote player |
| Character release | `LegoCharacterManager::ReleaseActor(name)` | `legocharactermanager.h:80` | Cleanup on disconnect |
| Character names | `LegoActor::GetActorName(id)` | `legoactor.h:73` | Maps actorId to character name string (pepper=1..laura=5 are playable; brickster=6 is NPC-only) |
| Position remote player | `OrientableROI::SetLocal2WorldWithWorldDataUpdate(mat)` | `orientableroi.h:35` | Sets remote player transform each frame |
| Read local position | `OrientableROI::GetLocal2World()` | `orientableroi.h:48` | Reads local player transform to broadcast |
| Get local player | `LegoOmni::GetInstance()->GetUserActor()` | `legomain.h:166` | Access local player entity |
| Get current world | `LegoOmni::GetInstance()->GetCurrentWorld()` | `legomain.h:163` | Check if player is in ISLE world |
| World's ROI list | `LegoWorld::GetROIList()` | `legoworld.h:119` | Add/remove remote player ROIs |
| 3D scene add | `VideoManager()->Get3DManager()->Add(*roi)` | `lego3dmanager.h` | Add ROI to 3D rendering pipeline. Signature: `BOOL Add(ViewROI&)` - takes a reference, so pointer must be dereferenced. Also: `BOOL Remove(ViewROI&)`, `BOOL Moved(ViewROI&)` |
| Walk animation data | `LegoLocomotionAnimPresenter::GetAnimation()` | `legoanimpresenter.h:109` | Get `LegoAnim*` walk cycle from ISLE world |
| Animation application | `LegoROI::ApplyAnimationTransformation(node, mat, time, roiMap)` | `legoroi.cpp:435` | Static function - animate any ROIs, no entity needed |
| Animation presenter lookup | `world->Find("LegoAnimPresenter", g_cycles[type][mood])` | `legoanimationmanager.cpp:2716` | Find walking animation presenter by character type + mood; names are e.g. `"CNs001xx"` (generic), `"CNs001Pe"` (Pepper), etc. from `g_cycles[11][17]` table |
| Animation cycle table | `g_cycles[11][17]` | `legoanimationmanager.cpp:59` | Maps character type + mood/speed tier to animation presenter names; index 0-3 = slow walk (speed 0.7), 4-6 = fast walk (speed 4.0), 7-9 = run |
| Game loop hook | `MxTickleManager::RegisterClient(client, interval)` | `mxticklemanager.h:47` | Register NetworkManager for per-frame updates |
| World enable/disable hook | `Extension<Multiplayer>::Call(HandleWorldEnable, world, enable)` | `legoworld.cpp` (new) | Hook in `LegoWorld::Enable()` fires when worlds are enabled/disabled; same pattern as `Extension<SiLoader>::Call(HandleWorld)` in `LegoOmni::AddWorld()` |
| Player world speed | `LegoEntity::GetWorldSpeed()` | `legoentity.h:91` | Read local player speed for broadcast |
| Actor identity | `LegoActor::GetActorId()` | `legoactor.h:67` | Current player's character type (pepper/mama/etc.) |
| Vehicle ride animation (small) | `g_cycles[type][10]` via `world->Find("LegoAnimPresenter", name)` | `legoanimationmanager.cpp:59,2389` | Ride animation includes character pose + vehicle variant model in one animation tree. Character dynamically bound via variable table - any character works with any ride animation. |
| Vehicle ride animation map | Bike→`CNs001Bd`+"bikebd", SkateBoard→`CNs001sk`+"board", Motocycle→`CNs011Ni`+"motoni" | `legoanimationmanager.cpp:59-246` | Maps vehicle type to animation presenter name + required vehicle variant ROI name |
| Vehicle variant ROI list | `g_vehicles[7]` | `legoanimationmanager.cpp:48` | NPC vehicle variant names: bikebd, bikepg, bikerd, bikesy, motoni, motola, board |
| Vehicle ROI cloning | `CharacterManager()->CreateAutoROI(uniqueName, lodName, FALSE)` | `legocharactermanager.cpp:987` | Create vehicle ROI clone from LOD list. Works for all vehicles with known LOD names. |
| Vehicle entity ROI names | bike, moto, dunebugy, rcuser, jsuser, towtk, ambul | `legoroi.cpp:47-53`, `isle.cpp:860`, `legoact2.cpp:287` | Entity ROI names for player-drivable vehicles (rcuser excluded from multiplayer - leaves ISLE) |
| Detect vehicle enter | `UserActor()->IsA("Helicopter")` etc. or compare `UserActor()` pointer | `islepathactor.cpp:94` | `UserActor()` changes to vehicle instance on enter, restored on exit |
| INI config loading | `iniparser_getseckeys(dict, section, ...)` loop in `IsleApp::LoadConfig()` | `isleapp.cpp:1251-1267` | Automatically discovers and loads options for any extension in `availableExtensions[]`. No code changes to isleapp needed. |

---

## Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────────┐
│                    IsleApp Game Loop                      │
│  SDL_AppIterate → Tick → MxTickleManager::Tickle()       │
└────────────────────────┬────────────────────────────────┘
                         │ Tickle() every frame
┌────────────────────────▼────────────────────────────────┐
│                 NetworkManager : MxCore                   │
│                                                          │
│  Every SEND_INTERVAL (~66ms / 15Hz):                     │
│  ┌─────────────────────────────────────────────────┐     │
│  │ BroadcastLocalState()                           │     │
│  │  1. Check if in ISLE world                      │     │
│  │  2. Read UserActor()->GetROI()->GetLocal2World()│     │
│  │  3. Read UserActor()->GetWorldSpeed()           │     │
│  │  4. Serialize STATE packet                      │     │
│  │  5. transport->Send(packet)                     │     │
│  └─────────────────────────────────────────────────┘     │
│                                                          │
│  Every frame:                                            │
│  ┌─────────────────────────────────────────────────┐     │
│  │ ProcessIncomingPackets()                         │     │
│  │  For each received packet:                      │     │
│  │  - JOIN:  create RemotePlayer, spawn ROI        │     │
│  │  - LEAVE: destroy RemotePlayer, release ROI     │     │
│  │  - STATE: update RemotePlayer target transform  │     │
│  └─────────────────────────────────────────────────┘     │
│  ┌─────────────────────────────────────────────────┐     │
│  │ UpdateRemotePlayers(deltaTime)                  │     │
│  │  For each RemotePlayer:                         │     │
│  │  1. Interpolate position toward target          │     │
│  │  2. Apply interpolated transform to ROI         │     │
│  │  3. Advance & apply walking animation           │     │
│  └─────────────────────────────────────────────────┘     │
│                                                          │
│  ┌─────────────────────────────────────────────────┐     │
│  │ NetworkTransport (abstract interface)           │     │
│  │  Connect() / Disconnect() / Send() / Receive() │     │
│  │  ┌───────────────────────────────────────────┐  │     │
│  │  │ WebSocketTransport (Emscripten, MVP)      │  │     │
│  │  │  - Relay URL from INI: multiplayer:relay url│ │     │
│  │  │  - Binary messages via EM_JS interop      │  │     │
│  │  └───────────────────────────────────────────┘  │     │
│  │  ┌───────────────────────────────────────────┐  │     │
│  │  │ Future: UDPTransport, WebRTCTransport     │  │     │
│  │  └───────────────────────────────────────────┘  │     │
│  └─────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────┘
```

### Remote Player Lifecycle

```
Network EVENT              RemotePlayer STATE           What happens in game
─────────────────────────────────────────────────────────────────────────────
JOIN packet received   →   CREATED                      CreateCharacterClone()
                                                        → builds textured ROI
                                                        Add ROI to 3D scene
                                                        Build animation roiMap
                                                        Set visibility = FALSE

First STATE with       →   VISIBLE                      Set visibility = TRUE
worldId == e_act1                                       Begin interpolation

STATE with             →   VISIBLE, MOVING              Interpolate position
speed > 0                                               Advance walk animation
                                                        Apply animation to ROI

STATE with             →   VISIBLE, IDLE                Interpolate to final pos
speed == 0                                              Reset anim to frame 0
                                                        (standing pose)

STATE with             →   VISIBLE, IN SMALL VEHICLE    Swap to ride animation
vehicleType 3-5                                         (g_cycles[type][10])
(bike/skate/moto)                                       Animate character riding pose
                                                        + vehicle variant model together
                                                        Vehicle ROI via CreateAutoROI

STATE with             →   VISIBLE, IN LARGE VEHICLE    Hide character ROI
vehicleType 0-2,6-7                                     Show cloned vehicle ROI
(heli/jet/db/                                           Position at interpolated
 tw/amb)                                                transform

STATE with             →   VISIBLE, ON FOOT             Show character ROI again
vehicleType == -1                                       Hide/release vehicle ROI
(from vehicle)                                          Swap back to walk animation

STATE with             →   HIDDEN                       Set visibility = FALSE
worldId != e_act1                                       Stop animation

Local player           →   (all) REMOVED FROM SCENE     ViewManager::RemoveAll(NULL) is
leaves ISLE world                                        called by LegoWorld::Enable(FALSE)
                                                         which removes ALL ROIs from scene.
                                                         ROI objects survive in memory (we
                                                         hold pointers in RemotePlayer).

Local player           →   (all matching worldId)       Must RE-ADD ROIs to ViewManager
returns to ISLE             RE-ADDED TO SCENE            via VideoManager()->Get3DManager()
                                                         ->Add(*roi) for each RemotePlayer.
                                                         Then restore visibility for peers
                                                         that are in ISLE.

LEAVE packet or        →   DESTROYED                    Remove ROI from 3D scene
timeout (no STATE                                       ReleaseActor(uniqueName)
for >5 seconds)                                         Delete RemotePlayer
```

**IMPORTANT: World transition ROI lifecycle (verified)**

When the local player enters a building from ISLE:
1. `LegoWorld::Enable(FALSE)` is called on the ISLE world
2. This calls `ViewManager::RemoveAll(NULL)` which removes ALL ROIs from the 3D scene graph
3. ROIs are NOT deleted - only removed from rendering (LOD level reset, mesh detached from D3DRM scene)
4. The ISLE world object persists in memory (it is NOT destroyed during normal gameplay)

When the local player returns to ISLE:
1. `LegoWorld::Enable(TRUE)` is called on the ISLE world
2. This re-adds ONLY entity ROIs from `m_entityList` to the ViewManager (lines 708-720 in legoworld.cpp)
3. Remote player ROIs are NOT entities, so they are NOT automatically re-added

**Solution: Extension hook in `LegoWorld::Enable()`** (following SiLoader pattern)

Rather than polling `GetCurrentWorld()` in `Tickle()`, we add a proper extension hook in `LegoWorld::Enable()` that fires when any world is enabled or disabled. This follows the same `Extension<T>::Call()` pattern used by SiLoader (e.g., `Extension<SiLoader>::Call(HandleWorld, p_world)` in `LegoOmni::AddWorld()`).

**New hook point in `LegoWorld::Enable()` (`legoworld.cpp`):**

```cpp
// In LegoWorld::Enable(), enable path, after entity ROIs are re-added (after line 720):
Extension<Multiplayer>::Call(HandleWorldEnable, this, TRUE);

// In LegoWorld::Enable(), disable path, after ViewManager::RemoveAll (after line 817):
Extension<Multiplayer>::Call(HandleWorldEnable, this, FALSE);
```

**Handler in Multiplayer extension:**

```cpp
// extensions/include/extensions/multiplayer.h
class Multiplayer {
public:
    static void Initialize();
    static void HandleWorldEnable(LegoWorld* p_world, MxBool p_enable);

    static std::map<std::string, std::string> options;
    static bool enabled;

    // Parsed from INI config during Initialize()
    static std::string relayUrl;   // from "multiplayer:relay url"

    // Called by NetworkManager to register itself
    static void SetNetworkManager(NetworkManager* mgr);
private:
    static NetworkManager* s_networkManager;
};
```

```cpp
// extensions/src/multiplayer.cpp
void Multiplayer::Initialize()
{
    // Read relay server URL from INI config (required for Emscripten WebSocket transport)
    relayUrl = options["multiplayer:relay url"];
    if (relayUrl.empty()) {
        SDL_Log("Multiplayer: no relay url configured, multiplayer will not connect");
    }
}

void Multiplayer::HandleWorldEnable(LegoWorld* p_world, MxBool p_enable)
{
    if (!s_networkManager) return;

    // Check if this is the ISLE world (worldId == e_act1 for Act1/Isle)
    if (p_enable) {
        // ISLE re-enabled: re-add all remote player ROIs to ViewManager
        s_networkManager->OnWorldEnabled(p_world);
    }
    else {
        // ISLE disabled: ROIs already stripped by RemoveAll, just update state
        s_networkManager->OnWorldDisabled(p_world);
    }
}
```

This is much cleaner than polling - the hook fires at exactly the right moment, after entity ROIs are restored (enable) or after all ROIs are stripped (disable). Only two lines added to core LEGO1 code.

### RemotePlayer Class Design

```cpp
class RemotePlayer {
public:
    RemotePlayer(uint32_t peerId, uint8_t actorId);
    ~RemotePlayer();

    void Spawn(LegoWorld* isleWorld);     // Create ROI + animation setup
    void Despawn();                        // Release ROI + cleanup
    void UpdateFromNetwork(const PlayerStateMsg& msg);  // Store target state
    void Tick(float deltaTime);            // Interpolate + animate each frame

private:
    // Identity
    uint32_t m_peerId;
    uint8_t  m_actorId;
    char     m_uniqueName[32];            // "pepper_mp_<peerId>"

    // Visual
    LegoROI* m_roi;                       // From CreateCharacterClone()
    bool     m_spawned;
    bool     m_visible;

    // Network state (latest received)
    Mx3DPointFloat m_targetPosition;
    Mx3DPointFloat m_targetDirection;
    Mx3DPointFloat m_targetUp;
    float    m_targetSpeed;
    int8_t   m_targetVehicleType;         // -1 = on foot, 0-7 = vehicle type encoding
    int8_t   m_targetWorldId;            // LegoOmni::World enum; e_act1 = in ISLE
    uint32_t m_lastUpdateTime;            // For timeout detection

    // Interpolation state
    MxMatrix m_currentTransform;          // Smoothly approaches target
    float    m_interpAlpha;               // 0..1, reset on each network update

    // Animation state (local only, not networked)
    LegoAnim*  m_walkAnim;                // Pointer to ISLE world's walk cycle
    LegoROI**  m_walkRoiMap;              // Maps walk anim nodes → body part ROIs
    MxU32      m_walkRoiMapSize;
    LegoAnim*  m_rideAnim;               // Pointer to ride animation for small vehicles (or NULL)
    LegoROI**  m_rideRoiMap;             // Maps ride anim nodes → body + vehicle variant ROIs
    MxU32      m_rideRoiMapSize;
    LegoROI*   m_vehicleROI;             // Cloned vehicle ROI for large vehicles (or NULL)
    int8_t     m_currentVehicleType;     // Currently active vehicle (-1 = on foot)
    float      m_animTime;               // Local accumulator, advances with speed
    bool       m_wasMoving;              // Track start/stop transitions
};
```

### Walking Animation - Detailed Design

The local player is always in 1st person and has no visible walking animation on themselves. But other players' characters need visible limb movement to look natural. The existing NPC animation system is fully reusable.

**How NPC walking animation works (existing code):**

1. `LegoLocomotionAnimPresenter` instances in the ISLE world hold `LegoAnim*` trees - these are hierarchical keyframe data describing how each body part (arms, legs, etc.) moves over a walk cycle duration
2. NPCs discover them via `world->Find("LegoAnimPresenter", presenterName)` where `presenterName` comes from `g_cycles[characterType][moodIndex]` (e.g. `"CNs001xx"` for generic slow walk). The `LegoAnimationManager::FUN_10063b90()` function (`legoanimationmanager.cpp:2382`) loads multiple speed tiers per character: slow walk at speed 0.7, fast walk at speed 4.0, etc.
3. `CreateROIAndBuildMap()` builds a `roiMap[]` array mapping animation tree node indices to the actor's body part ROIs, matched by **name** (body, head, arm-lft, arm-rt, leg-lft, leg-rt, etc.)
4. Each frame, `AnimateWithTransform()` calls the **static** `LegoROI::ApplyAnimationTransformation(node, parentTransform, time, roiMap)` which recursively computes `childROI.m_local2world = localAnimTransform * parentTransform` for each body part
5. NPC animation time advances as: `m_actorTime += deltaTime * m_worldSpeed` (`legopathactor.cpp:359`)
6. Cyclic time is: `timeInCycle = m_actorTime % duration` (`legoanimactor.cpp:61`)

**Why this works directly for remote player ghost ROIs:**

- `ApplyAnimationTransformation` is **static** (`legoroi.cpp:435`) - it needs only `(treeNode, matrix, time, roiMap)`, no entity/actor
- Cloned ROIs from `CreateCharacterClone()` have child ROIs with the **same body part names** (both use `g_actorLODs[]`: body, infohat, head, arm-lft, arm-rt, claw-lft, claw-rt, leg-lft, leg-rt)
- The walking `LegoAnim*` data is already loaded in the ISLE world's presenters
- ROI map index assignment is deterministic by tree traversal order

**Animation state machine per RemotePlayer (local only, not networked):**

```
                          ┌──────────────┐
         speed > 0        │   STANDING   │   speed == 0 (initial)
     ┌──────────────────▶│              │◀─────────────────────┐
     │                    │ animTime = 0 │                      │
     │                    │ Apply frame 0│                      │
     │                    └──┬───────┬───┘                      │
     │                       │       │ vehicleType >= 0         │
     │            speed > 0  │       ▼                          │
     │                       │  ┌──────────────────────┐        │
     │                       │  │  IN SMALL VEHICLE    │        │
     │                       │  │  (bike/skate/moto)   │        │
     │                       │  │                      │── vt<0─┘
     │                       │  │ Use ride anim + map  │
     │                       │  │ animTime += dt*speed │
     │                       │  │ Character visible    │
     │                       │  │ on vehicle           │
     │                       │  └──────────────────────┘
     │                       │
     │                       │  ┌──────────────────────┐
     │                       │  │  IN LARGE VEHICLE    │
     │                       │  │  (heli/jet/db/etc.)  │── vt<0─┘
     │                       │  │                      │
     │                       │  │ Character ROI hidden  │
     │                       │  │ Vehicle ROI shown     │
     │                       │  │ Position vehicle at   │
     │                       │  │ interpolated transform │
     │                       │  └──────────────────────┘
     │                       ▼
     │                    ┌──────────────┐
     │                    │   WALKING    │
     │                    │              │──── speed == 0 ──────┘
     │                    │ Each frame:  │
     └────────────────────│  animTime += │
                          │   dt * speed │
                          │  Apply walk  │
                          │  at time %   │
                          │  duration    │
                          └──────────────┘
```

- **STANDING**: `m_animTime = 0`. Apply `ApplyAnimationTransformation` at time 0, which is the neutral standing pose (arms at sides, legs together). This is applied once when entering this state.
- **WALKING**: `m_animTime += deltaTime * receivedSpeed`. Compute `timeInCycle = m_animTime % walkDuration`. Apply `ApplyAnimationTransformation` at `timeInCycle` every frame. The animation naturally loops - legs swing, arms pump.
- **IN SMALL VEHICLE** (Bike, SkateBoard, Motocycle): Same `ApplyAnimationTransformation` pipeline as walking, but using `m_rideAnim` + `m_rideRoiMap` instead of walk anim. Character ROI remains visible (in riding pose). Vehicle variant ROI (e.g., "bikebd") is part of the roiMap and animates together with the character.
- **IN LARGE VEHICLE** (Helicopter, Jetski, DuneBuggy, TowTrack, Ambulance): Character ROI hidden. A cloned vehicle ROI (`m_vehicleROI`) is positioned at the interpolated transform. No animation playback - just position/rotation updates. (RaceCar excluded - leaves ISLE world.)
- **Transition WALKING→STANDING**: Reset `m_animTime = 0`, apply frame 0. There may be a small visual snap from mid-stride to standing, but this is acceptable - the position interpolation has already stopped the character's movement, so the snap is brief and natural.

**No network sync of animation state is needed** because:
- Walk cycles are purely cosmetic - exact limb positions don't matter for gameplay
- NPCs in the original game don't sync animation state with each other either
- Speed-based local advancement produces natural-looking results
- Each client independently advances its own timers

**Implementation (~70-80 lines in RemotePlayer):**
```cpp
// === One-time setup during Spawn() ===

// 1. Find walking animation in ISLE world (~5 lines)
// Walking animation presenter names come from g_cycles[11][17] table
// (legoanimationmanager.cpp:59). Use the generic cycle set (index 0):
//   g_cycles[0][0] = "CNs001xx" (slow walk, speed 0.7)
//   g_cycles[0][4] = "CNs005xx" (fast walk, speed 4.0)
// For character-specific animations, use the character's cycle set index.
LegoLocomotionAnimPresenter* walkPresenter =
    (LegoLocomotionAnimPresenter*) isleWorld->Find("LegoAnimPresenter", "CNs001xx");
m_walkAnim = walkPresenter->GetAnimation();

// 2. Build ROI map for this remote player's body parts (~30-40 lines)
//    Walk animation tree, match node names to child ROIs of m_roi
//    (same logic as BuildROIMap but operating on our cloned ROI hierarchy)
m_roiMap = BuildRemotePlayerROIMap(m_walkAnim, m_roi, &m_roiMapSize);

// === Per-frame in Tick() ===

// 3. Determine animation state from received speed
if (m_targetSpeed > 0.01f) {
    // WALKING: advance local animation time
    m_animTime += deltaTime * m_targetSpeed;
    float duration = m_walkAnim->GetDuration();
    float timeInCycle = m_animTime - duration * ((int)(m_animTime / duration));

    MxMatrix transform(m_roi->GetLocal2World());
    LegoTreeNode* root = m_walkAnim->GetRoot();
    for (int i = 0; i < root->GetNumChildren(); i++) {
        LegoROI::ApplyAnimationTransformation(
            root->GetChild(i), transform, timeInCycle, m_roiMap);
    }
    m_wasMoving = true;
}
else if (m_wasMoving) {
    // WALKING → STANDING transition: snap to frame 0 (standing pose)
    m_animTime = 0.0f;
    MxMatrix transform(m_roi->GetLocal2World());
    LegoTreeNode* root = m_walkAnim->GetRoot();
    for (int i = 0; i < root->GetNumChildren(); i++) {
        LegoROI::ApplyAnimationTransformation(
            root->GetChild(i), transform, 0.0f, m_roiMap);
    }
    m_wasMoving = false;
}
// else: already standing, no animation update needed
```

### Vehicle Support for Remote Players (MUST HAVE)

When a player enters a vehicle, other players MUST see the vehicle (and character riding pose for small vehicles). This is essential for MVP - without it, players randomly disappear and reappear which is confusing.

**Three categories of vehicles exist in LEGO Island:**

| Category | Vehicles | In ISLE World? | Ride Animation? | Approach |
|----------|----------|----------------|-----------------|----------|
| Small (open) | Bike, SkateBoard, Motocycle | YES - free driving | YES - NPC ride animations at `g_cycles[type][10]` | Show character riding vehicle using existing animation system |
| Large (enclosed) | Helicopter, DuneBuggy, TowTrack, Ambulance, Jetski | YES - free driving/missions | NO - first-person only in original game | Show vehicle model only (character hidden inside) |
| Race-only | RaceCar | NO - enters CarRace_World | NO | Not needed for multiplayer (player leaves ISLE) |

**Verified by code analysis:**
- **RaceCar** confirmed leaves ISLE: `HandleClick()` does NOT call `Enter()`, sets `e_carrace`, `SwitchArea()` opens `CarRace_World` via `InvokeAction(e_opendisk, *g_carraceScript)` (`legogamestate.cpp:953`)
- **Jetski** confirmed stays in ISLE: `HandleClick()` calls `Enter()` (line 122), registers with ControlManager; `SwitchArea(e_jetski)` just calls `LoadIsle()` (`legogamestate.cpp:906`). There IS also a separate Jet Race (`e_jetrace`) but free riding is in ISLE.
- **Ambulance** confirmed stays in ISLE: `HandleClick()` calls `Enter()` (line 389), mission runs within ISLE world (picking up patients); state handled by `Act1State::e_ambulance` within Isle's notification handler.
- All other vehicles (Bike, SkateBoard, Motocycle, DuneBuggy, Helicopter, TowTrack) call `Enter()` and stay in ISLE.

#### Character × Vehicle Animation Compatibility (Verified)

**All 5 playable characters (Pepper, Mama, Papa, Nick, Laura) CAN ride ANY small vehicle.**

The animation system dynamically binds the character via a variable table (`legoanimpresenter.cpp:1442`):
```cpp
variableTable->SetVariable(key, p_actor->GetROI()->GetName());
```
The root animation node is a variable resolved to the character's ROI name at runtime. The vehicle ROI name is hardcoded in the animation data tree. This means:
- **Character is swappable** - any character ROI can be paired with any ride animation
- **Vehicle is fixed per animation** - each animation references a specific vehicle variant ROI (e.g., "bikebd", "board", "motoni")

For multiplayer, we choose the ride animation based on the VEHICLE TYPE (not the character), and the character binds automatically.

**Available ride animation presenters per vehicle type:**

| Vehicle Type | Animation Presenter | Vehicle Variant ROI | Notes |
|-------------|-------------------|-------------------|-------|
| Bike | `CNs001Bd` (from `g_cycles[7][10]`) | `bikebd` | Any of CNs001Bd/Pg/Rd/Sy work; differ only in vehicle variant |
| SkateBoard | `CNs001sk` (from `g_cycles[1][10]`) | `board` | Pepper's cycle set, but character is swappable |
| Motorcycle | `CNs011Ni` (from `g_cycles[4][10]`) | `motoni` | Nick's cycle set, but character is swappable |

**How NPC vehicle riding works (existing code, verified):**

1. Each NPC character has a `m_vehicleId` in `g_characters[47]` (`legoanimationmanager.cpp:251`), mapping to `g_vehicles[]` (e.g., Pepper → "board"/skateboard, Nick → "motoni"/motorcycle)
2. The vehicle animation is at `g_cycles[type][10]` (e.g., `"CNs001sk"` for Pepper on skateboard). This is a `LegoLocomotionAnimPresenter` that contains **both** the character's riding pose AND the vehicle model in one unified animation tree
3. `FUN_10063b90()` (`legoanimationmanager.cpp:2382`) loads it via `CreateROIAndBuildMap(actor, 1.7f)` at speed 1.7
4. `CreateSceneROIs()` (`legoanimpresenter.cpp:301`) finds/creates the vehicle ROI in the scene (e.g., calls `FindROI("board")` or `CreateAutoROI()`)
5. `BuildROIMap()` maps animation tree nodes to **both** character body parts AND the vehicle ROI
6. `ApplyAnimationTransformation()` then animates the whole assembly - character in riding pose + vehicle moving together

**This means vehicle animation is handled by the SAME `ApplyAnimationTransformation()` pipeline as walking.** The only difference is which `LegoAnim*` and `roiMap[]` are used.

#### Vehicle ROI Names and Cloning Strategy

**Vehicle entity ROI names for ISLE-world vehicles (from SI data / source code references):**

| Vehicle | Entity Class | Entity ROI Name | LOD in ViewLODListManager? | Cloning Method |
|---------|-------------|----------------|---------------------------|----------------|
| Bike | `Bike` | `bike` | YES (`g_sharedModelsHigh`) | `CreateAutoROI("bike_mp_N", "bike", FALSE)` |
| Motocycle | `Motocycle` | `moto` | YES (`g_sharedModelsHigh`) | `CreateAutoROI("moto_mp_N", "moto", FALSE)` |
| DuneBuggy | `DuneBuggy` | `dunebugy` | YES (`g_alwaysLoadNames`) | `CreateAutoROI("dunebugy_mp_N", "dunebugy", FALSE)` |
| Jetski | `Jetski` | `jsuser` | YES (`g_alwaysLoadNames`) | `CreateAutoROI("jsuser_mp_N", "jsuser", FALSE)` |
| TowTrack | `TowTrack` | `towtk` | After SI load | `CreateAutoROI("towtk_mp_N", "towtk", FALSE)` |
| Ambulance | `Ambulance` | `ambul` | After SI load | `CreateAutoROI("ambul_mp_N", "ambul", FALSE)` |
| Helicopter | `Helicopter` | from SI data | After SI load | `CreateAutoROI` with discovered name |
| SkateBoard | `SkateBoard` | from SI data | After SI load | `CreateAutoROI` with discovered name |

Note: RaceCar (`rcuser`) excluded - transitions to CarRace_World, not ridden in ISLE.

Source: `g_sharedModelsHigh` and `g_alwaysLoadNames` in `legoroi.cpp:47-53`; `SetROIVisible("towtk")` in `isle.cpp:860`; `FindROI("ambul")` in `legoact2.cpp:287`.

**NPC vehicle variant ROI names (for ride animations):**

| NPC Vehicle ROI | LOD Name | Used by Animation |
|----------------|----------|-------------------|
| `bikebd` | `bikebd` | CNs001Bd (bike ride) |
| `bikepg` | `bikepg` | CNs001Pg (bike ride) |
| `bikerd` | `bikerd` | CNs001Rd (bike ride) |
| `bikesy` | `bikesy` | CNs001Sy (bike ride) |
| `motoni` | `motoni` | CNs011Ni (motorcycle ride) |
| `motola` | `motola` | CNs011La (motorcycle ride) |
| `board` | `board` | CNs001sk (skateboard ride) |

These are DIFFERENT objects from the player vehicle entities. NPC variants are created by `CreateAutoROI()` during animation setup (`legoanimpresenter.cpp:326`). For multiplayer, we use these same NPC variant ROIs for ride animations, and create clones via `CreateAutoROI()` if multiple remote players ride the same vehicle type.

**Cloning approach - `CreateAutoROI` (`legocharactermanager.cpp:987`):**
```cpp
// Creates a new ROI from a LOD list name in ViewLODListManager
// Works for single-mesh vehicle models
// Adds to 3D scene and m_characters map automatically
LegoROI* roi = CharacterManager()->CreateAutoROI("bikebd_mp_42", "bikebd", FALSE);
```

**Why NOT CreateAutoROI for character cloning:**
- Characters are hierarchical multi-part ROIs: root + 10 child ROIs (body, head, arm-lft, arm-rt, leg-lft, leg-rt, claw-lft, claw-rt, infohat) from `g_actorLODs[]`
- Each child has its own LOD list, texture, and color customization
- `CreateAutoROI` creates a single-level ROI from ONE LOD list - no hierarchy
- Characters still need `CreateCharacterClone()` which copies the `CreateActorROI()` hierarchy-building logic (~100 lines)

#### Vehicle Rendering Implementation

**What changes for remote player vehicle support:**

1. **Protocol**: `vehicleType` field in STATE message (1 byte, see encoding below)
2. **Detecting local vehicle state**: Compare `UserActor()` pointer to previous value. When it changes, the player entered/exited a vehicle. Use `IsA("Helicopter")`, `IsA("Bike")`, etc. to determine vehicle type.
3. **Small vehicle (bike/skateboard/motorcycle) - show character riding:**
   - Find the ride animation presenter for the vehicle type (e.g., `world->Find("LegoAnimPresenter", "CNs001Bd")` for bike)
   - Build roiMap binding the remote player's character ROI + a vehicle variant ROI
   - Vehicle variant ROI created via `CreateAutoROI("bikebd_mp_N", "bikebd", FALSE)` if clone needed
   - Animate using same `ApplyAnimationTransformation()` pipeline as walking
4. **Large vehicle (helicopter/jetski/etc.) - show vehicle only:**
   - Hide the remote player's character ROI (`SetVisibility(FALSE)`)
   - Create vehicle ROI clone via `CreateAutoROI("jsuser_mp_N", "jsuser", FALSE)` (example for Jetski; use appropriate vehicle name per type)
   - Position vehicle ROI at remote player's interpolated transform
   - No animation needed (vehicle just moves/rotates)
5. **On vehicle exit**: Hide vehicle ROI, show character ROI, switch back to walk animation

**Vehicle type encoding for protocol (only vehicles ridden in ISLE world):**
```
-1 = on foot (walking/standing)
 0 = Helicopter      (large, vehicle-only rendering)
 1 = Jetski          (large, vehicle-only rendering)
 2 = DuneBuggy       (large, vehicle-only rendering)
 3 = Bike            (small, ride animation via CNs001Bd + "bikebd")
 4 = SkateBoard      (small, ride animation via CNs001sk + "board")
 5 = Motocycle       (small, ride animation via CNs011Ni + "motoni")
 6 = TowTrack        (large, vehicle-only rendering)
 7 = Ambulance       (large, vehicle-only rendering)
```
Note: RaceCar is excluded because clicking it transitions to CarRace_World (separate world, player leaves ISLE).

**Effort assessment**: Medium. The animation pipeline is identical for small vehicles - we're just swapping which `LegoAnim*` + `roiMap[]` is active. Large vehicles are simpler (just position a model). The main work is:
- Building the vehicle roiMap for small vehicles (lazily on first vehicle state) - ~30 lines
- Detecting vehicle enter/exit on local player via `UserActor()` comparison - ~10 lines
- Vehicle ROI cloning via `CreateAutoROI()` per vehicle type - ~20 lines
- Swapping animation state in `RemotePlayer::Tick()` - ~15 lines
- Large vehicle ROI creation/positioning - ~20 lines

---

## Wire Protocol

```
Header (all messages):
  uint8_t  type;        // JOIN=1, LEAVE=2, STATE=3
  uint32_t peerId;      // assigned by relay server
  uint32_t sequence;    // monotonic counter for ordering/staleness

STATE message (~49 bytes, sent at 10-15 Hz):
  uint8_t  actorId;     // LegoActor enum (pepper=1..laura=5; brickster=6 is NPC-only, never sent)
  int8_t   worldId;     // LegoOmni::World enum (-1=undefined, 0=e_act1/ISLE, 1=e_imain, etc.)
  int8_t   vehicleType; // -1 = on foot, 0-7 = vehicle type (see Vehicle Type Encoding)
  float    position[3]; // from m_local2world[3] (world position, 12 bytes)
  float    direction[3];// from m_local2world[2] (facing direction, 12 bytes)
  float    up[3];       // from m_local2world[1] (up vector, 12 bytes)
  float    speed;       // m_worldSpeed (0 = standing, >0 = walking/running)

JOIN message (reliable, on connect):
  uint8_t  actorId;     // pepper=1..laura=5 (validated on receive; reject if out of range)
  char     name[20];    // player display name

LEAVE message (reliable, on disconnect):
  (header only)
```

Remote players are only visible when **both** the local and remote player are in the ISLE world (`worldId == e_act1`). When either enters an instanced area (buildings, races, cutscenes), the remote player's ghost is hidden. The `worldId` field uses the `LegoOmni::World` enum directly, which is useful for logging/debugging (e.g., "player entered e_hosp" vs just "player left ISLE").

Position+direction+up (9 floats, 36 bytes) rather than full 4x4 matrix because:
- Maps directly to `LegoEntity::SetWorldTransform(location, direction, up)`
- 12 bytes smaller than full 4x3 matrix
- The right vector can be reconstructed as `cross(up, direction)`

At 15Hz with 4 peers: 48 bytes * 15 * 4 = ~2.9 KB/s total - negligible.

---

## Relay Server: Cloudflare Worker + Durable Object

The relay server is a Cloudflare Worker using a Durable Object per room. This gives globally distributed edge deployment, built-in WebSocket support, no server management, and a generous free tier.

**Architecture:**
- Each "room" is a **Durable Object** instance that holds the set of connected WebSocket peers
- The Worker's `fetch` handler routes `wss://<host>/room/<roomId>` to the correct Durable Object
- The Durable Object accepts WebSocket upgrades, tracks connected peers, and broadcasts incoming binary messages to all other peers in the room
- Room lifetime is automatic: Durable Object hibernates when all peers disconnect

**Sketch (`extensions/src/multiplayer/server/src/index.ts`, ~80 lines):**
```typescript
export class GameRoom implements DurableObject {
  private peers: Map<string, WebSocket> = new Map();

  async fetch(request: Request): Promise<Response> {
    const [client, server] = Object.values(new WebSocketPair());
    const peerId = crypto.randomUUID();
    this.ctx.acceptWebSocket(server);
    server.serializeAttachment({ peerId });
    this.peers.set(peerId, server);
    // Notify others of join
    this.broadcast(server, joinMsg(peerId));
    return new Response(null, { status: 101, webSocket: client });
  }

  async webSocketMessage(ws: WebSocket, data: ArrayBuffer | string) {
    // Relay binary STATE messages to all other peers
    this.broadcast(ws, data);
  }

  async webSocketClose(ws: WebSocket) {
    const { peerId } = ws.deserializeAttachment();
    this.peers.delete(peerId);
    this.broadcast(ws, leaveMsg(peerId));
  }

  private broadcast(sender: WebSocket, data: any) {
    for (const [, peer] of this.peers) {
      if (peer !== sender && peer.readyState === WebSocket.OPEN) {
        peer.send(data);
      }
    }
  }
}

export default {
  async fetch(request: Request, env: Env) {
    const url = new URL(request.url);
    const match = url.pathname.match(/^\/room\/([a-z0-9-]+)$/);
    if (!match) return new Response("Not found", { status: 404 });
    const roomId = env.GAME_ROOM.idFromName(match[1]);
    const room = env.GAME_ROOM.get(roomId);
    return room.fetch(request);
  }
};
```

**`wrangler.toml`:**
```toml
name = "isle-multiplayer"
main = "src/index.ts"

[[durable_objects.bindings]]
name = "GAME_ROOM"
class_name = "GameRoom"

[[migrations]]
tag = "v1"
new_classes = ["GameRoom"]
```

**Client connection URL**: Configured via INI as `multiplayer:relay url` (e.g., `wss://isle-multiplayer.<account>.workers.dev`). Room ID is appended at connect time: `<relay_url>/room/<roomId>`.

**Benefits:**
- Zero ops: no server to provision, scale, or maintain
- Global edge: WebSocket terminates at nearest Cloudflare PoP
- Free tier: 100k requests/day, 1M Durable Object requests/month (ample for MVP)
- Hibernation API: Durable Object sleeps when no peers are connected (zero cost when idle)
- Self-hostable: anyone can deploy their own relay by running `wrangler deploy`

---

## INI Configuration

The multiplayer extension follows the same INI config pattern as SiLoader and TextureLoader. The extension is enabled/disabled in the `[extensions]` section, and its options live in a dedicated `[multiplayer]` section.

**INI file (`isle.ini`):**
```ini
[extensions]
multiplayer=true

[multiplayer]
relay url=wss://isle-multiplayer.example.workers.dev
```

**How it works** (no changes to `isleapp.cpp` config loading needed):

The existing extension loading loop in `IsleApp::LoadConfig()` (`isleapp.cpp:1251-1267`) already handles this automatically:
1. Checks `iniparser_getboolean(dict, "extensions:multiplayer", 0)` - is multiplayer enabled?
2. Reads all keys from the `[multiplayer]` section via `iniparser_getseckeys()`
3. Builds `std::map<std::string, std::string>` with entries like `{"multiplayer:relay url", "wss://..."}`
4. Calls `Extensions::Enable("extensions:multiplayer", options)`
5. `Multiplayer::options` is set, `Multiplayer::Initialize()` parses the relay URL

**INI keys:**

| INI Section | Key | Type | Required | Description |
|------------|-----|------|----------|-------------|
| `extensions` | `multiplayer` | boolean | Yes | Enable/disable multiplayer extension |
| `multiplayer` | `relay url` | string | Yes (for MVP) | WebSocket relay server base URL. Room ID appended as `/room/<roomId>` |

**CONFIG app changes**: Add multiplayer toggle + relay URL input field to the Qt configuration app (`CONFIG/qt/config.cpp`), following the same pattern as the texture loader path and SI loader files fields.

---

## Transport Abstraction

The networking layer uses an abstract transport interface so that future transport implementations (WebRTC, native UDP, etc.) can be swapped in without changing any game logic. **For MVP, only the Emscripten WebSocket transport is implemented.**

```cpp
// extensions/include/extensions/multiplayer/networktransport.h
class NetworkTransport {
public:
    virtual ~NetworkTransport() = default;

    // Lifecycle
    virtual void Connect(const char* roomId) = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;

    // Send binary data to all peers (via relay or direct)
    virtual void Send(const uint8_t* data, size_t length) = 0;

    // Drain received messages. Returns number of messages dequeued.
    // Callback signature: void(const uint8_t* data, size_t length)
    virtual size_t Receive(std::function<void(const uint8_t*, size_t)> callback) = 0;
};
```

**MVP implementation: `WebSocketTransport` (Emscripten-only)**

```cpp
// extensions/src/multiplayer/websockettransport.h
class WebSocketTransport : public NetworkTransport {
public:
    WebSocketTransport(const std::string& relayBaseUrl);

    void Connect(const char* roomId) override;
    void Disconnect() override;
    bool IsConnected() const override;
    void Send(const uint8_t* data, size_t length) override;
    size_t Receive(std::function<void(const uint8_t*, size_t)> callback) override;

private:
    std::string m_relayBaseUrl;  // From INI: "multiplayer:relay url"
    // JS-side WebSocket handle via EM_JS interop
    // Incoming messages queued in JS ArrayBuffer queue, drained in Receive()
};
```

**Transport is created in `Multiplayer::Initialize()` based on platform:**
```cpp
void Multiplayer::Initialize()
{
    relayUrl = options["multiplayer:relay url"];

#ifdef __EMSCRIPTEN__
    if (!relayUrl.empty()) {
        s_transport = std::make_unique<WebSocketTransport>(relayUrl);
    }
#else
    // Future: native transports (UDP, WebRTC via native lib, etc.)
    SDL_Log("Multiplayer: no transport available for this platform yet");
#endif
}
```

`NetworkManager` receives the transport via `Multiplayer::GetTransport()` and is completely transport-agnostic - it only calls `Send()`, `Receive()`, `Connect()`, `Disconnect()`.

**Future transport implementations** (not MVP):

| Transport | Platform | Use Case | Relay Needed? |
|-----------|----------|----------|---------------|
| `WebSocketTransport` | Emscripten | **MVP** - browser builds | Yes (Cloudflare Worker) |
| `WebRTCTransport` | Emscripten | P2P upgrade - lower latency | Only for signaling |
| `UDPTransport` | Desktop/Mobile | LAN multiplayer | No (direct/broadcast) |
| `ENetTransport` | Desktop/Mobile | Internet multiplayer | Optional relay |

---

## New File Structure

```
extensions/
  include/extensions/
    multiplayer.h             # Extension class: INI config, HandleWorldEnable hook, transport factory
    multiplayer/
      networkmanager.h        # MxCore subclass, manages session lifecycle (transport-agnostic)
      networktransport.h      # Abstract transport interface (Connect/Disconnect/Send/Receive)
      websockettransport.h    # Emscripten WebSocket implementation (MVP transport)
      remoteplayer.h          # Remote player ROI + interpolation + animation state
      protocol.h              # Wire protocol structs & serialization
  src/
    multiplayer.cpp           # Extension Enable/Initialize (INI parsing, transport creation) + HandleWorldEnable
    multiplayer/
      networkmanager.cpp      # Core tick logic, local state broadcast, remote player management
      remoteplayer.cpp        # ROI creation/destruction, interpolation, animation
      protocol.cpp            # Packet encode/decode
      websockettransport.cpp  # WebSocket transport (Emscripten via EM_JS, connects to relay URL from INI)
      server/                 # Cloudflare Worker relay server (self-contained, deployable)
        src/
          index.ts            # Cloudflare Worker entry point + Durable Object
        wrangler.toml         # Cloudflare Workers config
        package.json
```

**CMakeLists.txt changes**: Add new sources under the existing `ISLE_EXTENSIONS` block. No new cmake option needed initially - multiplayer is a runtime feature within the existing extensions system.

**Existing files modified**:
- `extensions/include/extensions/extensions.h` - Add `"extensions:multiplayer"` to `availableExtensions[]`
- `extensions/src/extensions.cpp` - Add `#include "extensions/multiplayer.h"` and enable handler (same pattern as SiLoader: set options, enabled flag, call Initialize)
- `LEGO1/lego/legoomni/src/entity/legoworld.cpp` - Add 2 `Extension<Multiplayer>::Call(HandleWorldEnable, ...)` lines in `Enable()` (enable path after line 720, disable path after line 817)
- `CONFIG/qt/config.cpp` - Add multiplayer toggle + relay URL input field (follows texture loader / SI loader pattern)

Note: `isleapp.cpp` does NOT need modification for INI loading - the existing extension loading loop (`isleapp.cpp:1251-1267`) automatically picks up any extension registered in `availableExtensions[]`.

**Additions to core LEGO1 code** (minimal, following existing extension patterns):
- `LegoCharacterManager::CreateCharacterClone()` (~100 lines) - new method for independent ROI clones
- `LegoWorld::Enable()` - 2 lines: `Extension<Multiplayer>::Call(HandleWorldEnable, this, TRUE/FALSE)` after entity ROI re-add (enable path) and after `RemoveAll` (disable path), same pattern as `Extension<SiLoader>::Call(HandleWorld, p_world)` in `LegoOmni::AddWorld()`

All other multiplayer code lives in `extensions/`.

---

## Implementation Order

| Phase | Task | Effort | Dependencies | Details |
|-------|------|--------|--------------|---------|
| 1 | Transport interface + WebSocket impl | Medium | None | `NetworkTransport` abstract interface (`Connect`, `Disconnect`, `Send`, `Receive`). `WebSocketTransport` (Emscripten-only MVP impl) uses `EM_JS` to create browser WebSocket, connects to relay URL from INI config (`multiplayer:relay url`), sends/receives binary `ArrayBuffer` messages. Incoming data buffered in JS queue, drained in C++ each frame. Future transports (UDP, WebRTC) implement the same interface. |
| 2 | Protocol structs + serialization | Small | None | `PlayerStateMsg`, `PlayerJoinMsg`, `PlayerLeaveMsg` packed structs. `Serialize()`/`Deserialize()` to/from byte buffers. Endianness handling (always little-endian on wire). |
| 3 | NetworkManager skeleton | Small | Phase 1 | `MxCore` subclass. Register with `MxTickleManager`. `Tickle()` calls `BroadcastLocalState()` at 15Hz interval + `ProcessIncomingPackets()` + `UpdateRemotePlayers()` every frame. |
| 4 | Local state broadcast | Small | Phase 3 | In `BroadcastLocalState()`: null-check `UserActor()` and `CurrentWorld()` first (both can be NULL during world transitions - `legogamestate.cpp:219`). If valid, read `UserActor()->GetROI()->GetLocal2World()` for position/direction/up, `GetWorldSpeed()` for speed, set `worldId` from `GetCurrentWorld()->GetWorldId()`, detect vehicle via `UserActor()->IsA("Helicopter")` etc., serialize and send. |
| 5 | `CreateCharacterClone()` | Medium | None | New method on `LegoCharacterManager`. ~100 lines copied from `CreateActorROI()` body construction loop. Takes `(uniqueName, characterType)`, creates independent multi-part ROI, stores in `m_characters` map under unique name, does NOT write to `g_actorInfo[]`. |
| 6 | RemotePlayer: ROI + interpolation | Medium | 2, 5 | `RemotePlayer` class. `Spawn()` calls `CreateCharacterClone()`, adds to 3D scene. `UpdateFromNetwork()` stores target transform. `Tick()` interpolates current transform toward target using lerp, applies via `SetLocal2WorldWithWorldDataUpdate()`. |
| 7 | RemotePlayer: walking animation | Small | 6 | In `Spawn()`: find walk presenter via `g_cycles[type][0]`, get `LegoAnim*`, build roiMap. In `Tick()`: if speed>0 advance `m_animTime`, compute `timeInCycle`, call `ApplyAnimationTransformation()`. If speed==0 and was moving, snap to frame 0. ~70-80 lines. |
| 7b | RemotePlayer: vehicle rendering (MUST HAVE) | Medium | 7 | **Small vehicles (type 3-5):** When `vehicleType` changes, swap active `LegoAnim*` + `roiMap[]` to ride animation. Ride anim from `g_cycles` (bike→`CNs001Bd`, skate→`CNs001sk`, moto→`CNs011Ni`). Character dynamically bound via variable table. Vehicle variant ROI (e.g., "bikebd") created via `CreateAutoROI()`. **Large vehicles (type 0-2,6-7):** Hide character ROI, create vehicle ROI clone via `CreateAutoROI()` (e.g., `CreateAutoROI("jsuser_mp_N", "jsuser", FALSE)` for Jetski), position at interpolated transform. On vehicle exit: show character, hide/release vehicle ROI, restore walk animation. |
| 8 | ISLE-world visibility | Small | 6 | Add `Extension<Multiplayer>::Call(HandleWorldEnable, this, p_enable)` hook in `LegoWorld::Enable()` (2 lines in core code, following SiLoader pattern). `HandleWorldEnable` calls `NetworkManager::OnWorldEnabled(world)` which re-adds all remote player ROIs via `Get3DManager()->Add(*roi)` and restores visibility. `OnWorldDisabled(world)` updates internal state. On remote `worldId` change: toggle visibility for that specific remote player. |
| 9 | Cloudflare Worker relay | Small | None | ~80 lines TypeScript in `extensions/src/multiplayer/server/`. Durable Object per room. Accept WebSocket, broadcast binary messages to other peers, emit JOIN/LEAVE on connect/disconnect. Deploy with `cd extensions/src/multiplayer/server && wrangler deploy`. |
| 10 | End-to-end test | - | All above | Two Emscripten instances in separate browser tabs. Connect to same room. Walk around. Verify: characters appear, move smoothly, animate, disappear on disconnect. |
| 11 | Extensions integration + UI | Small | 10 | Register `"extensions:multiplayer"` in `availableExtensions[]` and `extensions.cpp`. INI config provides relay URL. Room code input via HTML outside game canvas. JS calls `Module.ccall()` to trigger connect/disconnect. Player count display. |
| Future | WebRTC P2P upgrade | Large | 10 | Add `WebRTCTransport` implementation. Use Cloudflare Worker for signaling only (SDP/ICE exchange). Data flows P2P after connection established. |
| Future | Native UDP transport | Medium | 1 | Add `UDPTransport` for desktop builds. LAN broadcast for discovery. |

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| `UserActor()` NULL during world transitions | **High** | Between `SetUserActor(NULL)` (`legogamestate.cpp:219`) and `SetUserActor(newActor)` (line 239), `GetUserActor()` returns NULL. `BroadcastLocalState()` MUST null-check both `UserActor()` and `CurrentWorld()` and skip if either is NULL. Same guard needed for vehicle detection. |
| `Get3DManager()->Add()` is NOT recursive | **Resolved** | `Add()` only pushes the parent ROI onto `ViewManager::rois`. However, `ViewManager::Update()` calls `ManageVisibilityAndDetailRecursively()` which traverses `GetComp()` children every frame, handling LOD and geometry attachment for all body parts automatically. This matches how the game works: `GetActorROI()` (`legocharactermanager.cpp:272`) and `LegoWorld::Enable(TRUE)` (`legoworld.cpp:708-720`) both add only the parent. Only one `Add(*roi)` call needed per remote player. |
| `GetWorldSpeed()` returns animation speed, not vehicle speed | **Medium** | When player is in a vehicle, `GetWorldSpeed()` is the actor's animation playback multiplier, not the vehicle's actual movement speed. For large vehicles (character hidden), read speed from the vehicle entity itself: `UserActor()->GetWorldSpeed()` works because `UserActor()` IS the vehicle during riding (`islepathactor.cpp:94`). |
| Emscripten WebSocket threading | Medium | All network I/O on main thread via `Tickle()`; browser WebSocket is natively async; incoming messages queued in JS, drained in C++ each frame. Existing interop uses `MAIN_THREAD_EM_ASM` pattern (not `EM_JS`); WebSocket impl should follow the same pattern. |
| ROI lifecycle management | Medium | Every `CreateCharacterClone()` paired with `ReleaseActor()`; tracked in `RemotePlayer`; `Despawn()` called in destructor as safety net. `ReleaseActor()` calls `RemoveROI()` → `Get3DManager()->Remove()` which DOES recursively clean children via `GetComp()`. |
| Entering/leaving ISLE world | **Medium** | `ViewManager::RemoveAll(NULL)` called on ISLE disable strips ALL ROIs from scene (not just ours). Handled cleanly via `Extension<Multiplayer>::Call(HandleWorldEnable, this, p_enable)` hook in `LegoWorld::Enable()`, following the same pattern as `Extension<SiLoader>::Call(HandleWorld, p_world)` in `LegoOmni::AddWorld()`. The hook fires at exactly the right moment - after entity ROIs are restored (enable) or after all ROIs are stripped (disable). NetworkManager re-adds remote player ROIs via `Get3DManager()->Add(*roi)` in the enable callback. |
| Brickster actorId validation | Medium | Brickster (actorId=6) is NPC-only. Character selection (`legovariables.cpp:167-186`) only handles IDs 1-5. Vehicle code clamps to `c_laura` (5). Protocol must validate `actorId` is in range 1-5 on receive; reject packets with invalid actorId. |
| Character map name collisions | Low | `LegoCharacterManager`'s map uses case-insensitive comparison (`SDL_strcasecmp`). Multiplayer clone names (e.g., "pepper_mp_42") must be unique and cannot accidentally match existing names like "pepper". The `_mp_<id>` suffix ensures this. |
| Walking animation ROI map construction | Low | Body part names are deterministic from `g_actorLODs[]`; `BuildRemotePlayerROIMap()` matches by name, same logic as existing `BuildROIMap()` |
| Walk→stand animation snap | Low | Resetting to frame 0 may cause a brief visual snap from mid-stride; acceptable because position has already stopped; could improve later by letting walk cycle complete to nearest frame-0 crossing |
| Animation presenters survive world transitions | Low | `LegoWorld::Enable(FALSE)` only toggles presenter `Enable` flags and stores them in `m_disabledObjects`. `Enable(TRUE)` re-enables them. Presenters and their `LegoAnim*` data remain valid in memory. Remote player animation pointers obtained during `Spawn()` remain valid across world transitions. |
| Cloudflare Worker availability | Very Low | Cloudflare's edge network is highly available; free tier is generous for MVP |
| Relay latency vs P2P | Low | Cloudflare PoPs are globally distributed; relay adds ~5-20ms; negligible for 15Hz position updates |
| Transform interpolation jitter | Low | Linear interpolation at 60fps between 15Hz network updates; add dead reckoning (extrapolate along direction * speed) if needed |
| Sequence number redundancy | None | WebSocket (TCP) guarantees ordering, but sequence numbers provide app-level deduplication on reconnect and useful diagnostic logging. Keep. |

---

## Verification Plan

1. **Spawn test**: Call `CreateCharacterClone()` + `Add(*roi)` manually, verify a second character renders in the ISLE world at a hardcoded position
2. **Animation test**: With a spawned clone, call `ApplyAnimationTransformation()` in a loop advancing time, verify legs/arms move
3. **Transform test**: Record local player transforms over 10 seconds, replay them on a remote ROI with interpolation, verify smooth movement
4. **Network test**: Two Emscripten instances in separate browser tabs, connect to same Cloudflare Worker room, walk around, verify characters appear and move
5. **Animation in motion**: Verify remote player's limbs animate while moving and stop when standing still
6. **ISLE transition test**: Local player enters a building, verify remote players disappear; exit building, verify they reappear
7. **Disconnect test**: Close one tab, verify remote player disappears in other tab within 5 seconds
8. **Performance**: Measure FPS impact with 2-4 remote players (target: <5% FPS loss)
9. **Build regression**: Ensure project compiles identically with extensions disabled

---

## Source Code Verification Status

All assumptions in this plan have been verified against the source code:

| Assumption | Status | Evidence |
|-----------|--------|----------|
| `GetActorROI()` returns shared ROI (refcount bump, same object) | Verified | `legocharactermanager.cpp:250-296` - `character->AddRef()` then `return character->m_roi` |
| `CreateActorROI()` body construction loop is ~100 lines and self-contained | Verified | `legocharactermanager.cpp:459-603` - builds root ROI, 10 child ROIs from `g_actorLODs`, applies textures/colors |
| `OrientableROI::SetLocal2WorldWithWorldDataUpdate(const Matrix4&)` exists | Verified | `orientableroi.h:35` - virtual, vtable+0x20 |
| `LegoROI::ApplyAnimationTransformation()` is static | Verified | `legoroi.h:43` - `static void ApplyAnimationTransformation(LegoTreeNode*, Matrix4&, LegoTime, LegoROI**)` |
| `LegoLocomotionAnimPresenter::GetAnimation()` returns `LegoAnim*` | Verified | `legoanimpresenter.h:109` - `LegoAnim* GetAnimation() { return m_anim; }` (inherited from `LegoAnimPresenter`) |
| Body part names in `g_actorLODs` match animation node names | Verified | `legoactors.cpp:10-93` defines names (body, infohat, head, arm-lft, arm-rt, etc.); `legoanimpresenter.cpp:480` matches via `FindChildROI(name)` with case-insensitive comparison |
| `MxTickleManager::RegisterClient(MxCore*, MxTime)` | Verified | `mxticklemanager.h:47` |
| `LegoOmni::GetUserActor()` returns `LegoPathActor*` | Verified | `legomain.h:166` |
| `LegoOmni::GetCurrentWorld()` returns `LegoWorld*` | Verified | `legomain.h:163` |
| `LegoWorld::GetROIList()` returns `list<LegoROI*>&` | Verified | `legoworld.h:119` |
| `LegoWorld::Find(const char*, const char*)` returns `MxCore*` | Verified | `legoworld.h:110` |
| `Lego3DManager::Add(ViewROI&)` returns `BOOL` | Verified | `lego3dmanager.h` - inline, delegates to `m_pLego3DView->Add(rROI)`. Also `Remove(ViewROI&)`, `Moved(ViewROI&)` |
| Extensions system uses `Extension<T>::Call()` template pattern | Verified | `extensions/include/extensions/extensions.h` - returns `std::optional`, no-op when disabled or `EXTENSIONS` undefined |
| Extensions registered in `availableExtensions[]` array, enabled via `Enable(key, options)` | Verified | `extensions.h` and `extensions.cpp` |
| CMake: extensions conditionally compiled under `ISLE_EXTENSIONS` | Verified | `CMakeLists.txt:527-535` - `target_compile_definitions(lego1 PUBLIC EXTENSIONS)` |
| Walking animation presenter names come from `g_cycles[11][17]` | **Corrected** | `legoanimationmanager.cpp:59` - names like `"CNs001xx"` (generic), `"CNs001Pe"` (Pepper); **NOT** `"bdwk00"` as originally stated. Plan updated. |
| NPC walk animations loaded at multiple speed tiers | Verified | `legoanimationmanager.cpp:2382-2430` - mood 0 at speed 0.7, mood+4 at speed 4.0, mood+7 for third tier |
| Animation time: `m_actorTime += deltaTime * m_worldSpeed` | Verified | `legopathactor.cpp:359` |
| Cyclic time: `timeInCycle = m_actorTime - duration * ((MxS32)(m_actorTime / duration))` | Verified | `legoanimactor.cpp:61` |
| ROIs survive world transitions via SetVisibility toggle | **Corrected** | `LegoWorld::Enable(FALSE)` calls `ViewManager::RemoveAll(NULL)` which removes ALL ROIs from scene graph (not just hides them). `Enable(TRUE)` only re-adds entity ROIs from `m_entityList`. Remote player ROIs must be explicitly re-added via `Get3DManager()->Add(*roi)`. Solution: `Extension<Multiplayer>::Call(HandleWorldEnable)` hook in `LegoWorld::Enable()`, same pattern as SiLoader hooks. Plan updated. |
| ISLE world is not destroyed during normal gameplay | Verified | `DeleteWorld`/`RemoveWorld` only called during `Close()` (app shutdown) or for specific worlds (Act2/3, garage). ISLE world uses `Enable(FALSE/TRUE)` pattern. |
| `ViewManager::RemoveAll` does not delete ROI objects | Verified | `viewmanager.cpp:127-152` - only removes from scene graph (`RemoveROIDetailFromScene`), sets LOD level to unset. Does not call `delete`. |
| Vehicle animation is in `g_cycles[type][10]` | Verified | `legoanimationmanager.cpp:2388-2394` - `cycles[10]` checked when `m_vehicleId >= 0`, loaded via `CreateROIAndBuildMap(p_actor, 1.7f)` |
| Vehicle animation tree includes vehicle model | Verified | `legoanimpresenter.cpp:301-331` - `CreateSceneROIs()` iterates animation actors, finds/creates vehicle ROI via `FindROI()` or `CreateAutoROI()`. `BuildROIMap()` then maps both character body parts AND vehicle ROI to animation nodes. |
| Vehicle ROIs are shared world objects | Verified | `legoanimationmanager.cpp:455,2267` - `FindROI(g_vehicles[vehicleId].m_name)` searches scene for pre-existing ROIs like "board", "bikebd", etc. Only one instance per vehicle name exists. |
| `UserActor()` changes to vehicle on enter | Verified | `islepathactor.cpp:94` - `SetUserActor(this)` makes vehicle the UserActor; line 114 restores `m_previousActor` on exit |
| `g_vehicles[7]` vehicle list | Verified | `legoanimationmanager.cpp:48-56` - bikebd, bikepg, bikerd, bikesy, motoni, motola, board |
| Character-vehicle mapping via `m_vehicleId` | Verified | `legoanimationmanager.cpp:251-299` - pepper→board(6), nick→motoni(4), laura→motola(5), etc. |
| Character binding is dynamic in ride animations | Verified | `legoanimpresenter.cpp:1442` - `variableTable->SetVariable(key, p_actor->GetROI()->GetName())` binds any character ROI to animation tree via variable table. Character is swappable; vehicle ROI name is hardcoded in animation data. |
| Any character can ride any small vehicle | Verified | Animation system resolves character dynamically; bike ride anim (CNs001Bd), skate ride anim (CNs001sk), moto ride anim (CNs011Ni) all accept any character ROI. Verified via `CreateROIAndBuildMap` + variable table mechanism. |
| Player vehicle entity ROI names | Verified | `g_sharedModelsHigh`: bike, moto (`legoroi.cpp:47`); `g_alwaysLoadNames`: rcuser, jsuser, dunebugy (`legoroi.cpp:53`); towtk (`isle.cpp:860`); ambul (`legoact2.cpp:287`). Helicopter and SkateBoard entity ROI names come from SI data. |
| `CreateAutoROI` creates single-mesh ROIs from LOD lists | Verified | `legocharactermanager.cpp:987-1044` - looks up LOD list by name, creates single `LegoROI`, adds to 3D scene. Suitable for vehicle cloning, NOT for multi-part character cloning. |
| NPC vehicle variant ROIs created by `CreateAutoROI` | Verified | `legoanimpresenter.cpp:326` - `CreateSceneROIs()` calls `CreateAutoROI(actorName, lodName, FALSE)` to create vehicle variant ROIs for ride animations. |
| No ride animations exist for large vehicles | Verified | `g_cycles[11][17]` index [10] entries reference only bike/skateboard/motorcycle variants. No animation presenters for helicopter, jetski, racecar, dunebuggy, towtrack, or ambulance ride poses. Players see first-person dashboard only. |
| `IslePathActor::Enter()` hides vehicle ROI | Verified | `islepathactor.cpp:77` - `m_roi->SetVisibility(FALSE)` when player enters vehicle. Vehicle entity ROI is available for reuse/cloning while local player drives it. |
| RaceCar leaves ISLE world | Verified | `racecar.cpp:41-51` - HandleClick does NOT call Enter(), sets e_carrace; `legogamestate.cpp:953` - SwitchArea(e_carrace) opens CarRace_World via InvokeAction(e_opendisk). |
| Jetski stays in ISLE world | Verified | `jetski.cpp:122` - HandleClick calls Enter(); `legogamestate.cpp:906` - SwitchArea(e_jetski) calls LoadIsle() (stays in ISLE). Free riding is in ISLE waters; e_jetrace is a separate race event. |
| Ambulance stays in ISLE world | Verified | `ambulance.cpp:389` - HandleClick calls Enter(); mission runs within ISLE world; state handled by Act1State::e_ambulance within Isle notification handler. |
| All other vehicles stay in ISLE | Verified | Bike, SkateBoard, Motocycle, DuneBuggy, Helicopter, TowTrack all call Enter() in HandleClick and stay in ISLE world. |
| Actor ID enum range is 0-6 (none=0, pepper=1..brickster=6) | **Corrected** | `legoactor.h:15-23` - enum: c_none=0, c_pepper=1, c_mama=2, c_papa=3, c_nick=4, c_laura=5, c_brickster=6. Plan originally stated pepper=0..brickster=5. |
| `LegoEntity::GetWorldSpeed()` returns `MxFloat` | Verified | `legoentity.h:91` - `MxFloat GetWorldSpeed() { return m_worldSpeed; }` |
| `LegoEntity::SetWorldTransform(location, direction, up)` takes 3 Vector3 params | Verified | `legoentity.h:60-64` - `virtual void SetWorldTransform(const Vector3& p_location, const Vector3& p_direction, const Vector3& p_up)` |
| `Extension<SiLoader>::Call(HandleWorld, p_world)` reference pattern | Verified | `legomain.cpp:417` in `LegoOmni::AddWorld()` |
| Ride animation presenters exist in ISLE world from streaming data | Verified | `legoanimationmanager.cpp:2389-2394` looks up via `world->Find("LegoAnimPresenter", vehicleWC)` - presenters are loaded from SI data into `m_animPresenters`. `g_vehicles[].m_unk0x04` controls dynamic NPC assignment, not presenter existence. |
| `LegoAnim::GetDuration()` and `LegoTree::GetRoot()` | Verified | `legoanim.h:342` - `LegoTime GetDuration() { return m_duration; }`; `legotree.h:79` - `LegoTreeNode* GetRoot() { return m_root; }` (LegoAnim inherits from LegoTree) |
| `LegoTreeNode::GetNumChildren()` and `GetChild()` | Verified | `legotree.h:45` - `LegoU32 GetNumChildren()`; `legotree.h:51` - `LegoTreeNode* GetChild(LegoU32 p_i)` |
| INI extension loading loop auto-discovers new extensions | Verified | `isleapp.cpp:1251-1267` - iterates `availableExtensions[]`, reads section keys via `iniparser_getseckeys()`, builds options map, calls `Extensions::Enable()`. Adding `"extensions:multiplayer"` to `availableExtensions[]` is sufficient. |
| `Extensions::Enable()` sets options + calls Initialize | Verified | `extensions.cpp:8-27` - pattern: `T::options = std::move(p_options); T::enabled = true; T::Initialize();` |
| SiLoader reads INI keys as `"si loader:files"` format | Verified | `siloader.cpp:29` - `options["si loader:files"]`. Section name is extracted from extension key after `:` (`isleapp.cpp:1255`). |
| `UserActor()` can be NULL during world transitions | Verified | `legogamestate.cpp:219` - `SetUserActor(NULL)` before creating new actor. Window exists until line 239 `SetUserActor(newActor)`. `isle.cpp:556,588` - existing code null-checks `UserActor()` before use. |
| Brickster is NPC-only, not playable | Verified | `legovariables.cpp:167-186` - WhoAmIVariable only handles pepper/mama/papa/nick/laura (IDs 1-5, no brickster case). `ambulance.cpp:331-332` - vehicle code clamps actorId to `c_laura` (5). `legoactor.cpp:137` - SetROI loop is `for (i = 1; i <= sizeOfArray - 2; i++)` = 1..5. |
| `Get3DManager()->Add()` only adds parent; children render automatically | Verified | `viewmanager.h:52` - `Add()` pushes parent to `rois` list only. But `ViewManager::Update()` (`viewmanager.cpp:287-308`) calls `ManageVisibilityAndDetailRecursively()` which recursively traverses `GetComp()` children for LOD/geometry. Confirmed by `legocharactermanager.cpp:272` and `legoworld.cpp:708-720` which both add only the parent ROI. One `Add(*roi)` call per remote player is sufficient. |
| `GetWorldSpeed()` is animation playback multiplier | Verified | `legoentity.h:91` - simple getter `return m_worldSpeed`. Set explicitly by code, not automatically by vehicle context. `legopathactor.cpp:359` - used as `m_actorTime += deltaTime * m_worldSpeed` for animation. |
| `LegoCharacterManager` uses case-insensitive name comparison | Verified | `legocharactermanager.h:19-21` - `LegoCharacterComparator` uses `SDL_strcasecmp`. Names like "pepper_mp_1" are safe from colliding with "pepper". |
| Animation presenters survive `LegoWorld::Enable(FALSE/TRUE)` | Verified | `legoworld.cpp:776-791` - disable path toggles `Enable(FALSE)` and stores in `m_disabledObjects`. `legoworld.cpp:723-734` - enable path re-enables from `m_disabledObjects`. Presenters not destroyed. `LegoAnim*` data remains valid. |
| Emscripten interop uses `MAIN_THREAD_EM_ASM` pattern | Verified | `ISLE/emscripten/events.cpp` - `Emscripten_SendEvent()` uses `MAIN_THREAD_EM_ASM`. Also `config.cpp`, `filesystem.cpp`, `haptic.cpp`, `messagebox.cpp`. No `EM_JS` usage found. |
| MxTickleManager uses wall-clock timing (SDL_GetTicks) | Verified | `mxtimer.cpp` - uses `SDL_GetTicks()` for time. `mxticklemanager.cpp:36-63` - compares `(interval + lastTime) < currentTime` with millisecond resolution. 66ms (15Hz) intervals work at 30+ FPS game loop. |
