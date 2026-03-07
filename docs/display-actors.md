# Display Actor Override for Multiplayer Extension

## Context

Currently, the multiplayer extension clones a character ROI based on the player's in-game actor ID (1-5: Pepper/Mama/Papa/Nick/Laura). This means all players appear as one of the 5 playable characters. We want to allow players to choose any of the 66 character models from `g_actorInfoInit` (e.g., "rhoda", "infoman") via an INI setting, decoupling the visual display from the gameplay actor ID.

The actor ID continues to be communicated for future use, but the visual display is driven by a separate `displayActorIndex` — an index into `g_actorInfoInit[66]`. This works because all 66 characters share the same skeleton (`g_actorLODs`), so animations are compatible.

## Files to Modify

| File | Change |
|------|--------|
| `extensions/include/extensions/multiplayer/protocol.h` | Add `displayActorIndex` to `PlayerStateMsg`, add constants |
| `extensions/include/extensions/multiplayer/remoteplayer.h` | Add display actor fields, setter for actorId |
| `extensions/src/multiplayer/remoteplayer.cpp` | Use display actor name for cloning |
| `extensions/include/extensions/multiplayer/networkmanager.h` | Add `m_localDisplayActorIndex`, `SetDisplayActorIndex()` |
| `extensions/src/multiplayer/networkmanager.cpp` | Broadcast/handle display actor index, update respawn logic |
| `extensions/include/extensions/multiplayer/thirdpersoncamera.h` | Add display clone fields and methods |
| `extensions/src/multiplayer/thirdpersoncamera.cpp` | Create/manage display clone ROI, sync transform |
| `extensions/src/multiplayer.cpp` | Read INI setting, resolve to g_actorInfoInit index |

## Implementation Steps

### Step 1: Protocol — Add `displayActorIndex` to `PlayerStateMsg`

**File:** `extensions/include/extensions/multiplayer/protocol.h`

Add at the end of `PlayerStateMsg` (after `idleAnimId`):
```cpp
uint8_t displayActorIndex; // Index into g_actorInfoInit (0-65)
```

Add constants/helper near `IsValidActorId`:
```cpp
static const uint8_t DISPLAY_ACTOR_NONE = 0xFF;

inline bool IsValidDisplayActorIndex(uint8_t p_index)
{
    return p_index < 66;
}
```

The relay server (`extensions/src/multiplayer/server/`) does NOT parse MSG_STATE content — it just forwards raw bytes. No relay changes needed.

### Step 2: RemotePlayer — Use display actor for cloning

**File:** `extensions/include/extensions/multiplayer/remoteplayer.h`

- Change constructor: `RemotePlayer(uint32_t p_peerId, uint8_t p_actorId, uint8_t p_displayActorIndex)`
- Add member: `uint8_t m_displayActorIndex`
- Add getter: `uint8_t GetDisplayActorIndex() const { return m_displayActorIndex; }`
- Add setter: `void SetActorId(uint8_t p_actorId) { m_actorId = p_actorId; }`
- Add private helper declaration: `const char* GetDisplayActorName() const`

**File:** `extensions/src/multiplayer/remoteplayer.cpp`

Constructor: store `m_displayActorIndex`, use display actor name for unique name:
```cpp
const char* displayName = GetDisplayActorName();
SDL_snprintf(m_uniqueName, sizeof(m_uniqueName), "%s_mp_%u", displayName, p_peerId);
```

Add helper using existing APIs (`CharacterManager()->GetActorName()` at `legocharactermanager.h:72`, `LegoActor::GetActorName()` at `legoactor.cpp:125`):
```cpp
const char* RemotePlayer::GetDisplayActorName() const
{
    if (IsValidDisplayActorIndex(m_displayActorIndex)) {
        return CharacterManager()->GetActorName(m_displayActorIndex);
    }
    return LegoActor::GetActorName(m_actorId);
}
```

In `Spawn()`, replace `LegoActor::GetActorName(m_actorId)` with `GetDisplayActorName()`:
```cpp
const char* actorName = GetDisplayActorName();
```

### Step 3: NetworkManager — Broadcast and handle display actor index

**File:** `extensions/include/extensions/multiplayer/networkmanager.h`

- Add public method: `void SetDisplayActorIndex(uint8_t p_index)`
- Add private member: `uint8_t m_localDisplayActorIndex`
- Update `CreateAndSpawnPlayer` signature: add `uint8_t p_displayActorIndex` parameter

**File:** `extensions/src/multiplayer/networkmanager.cpp`

Initialize `m_localDisplayActorIndex(DISPLAY_ACTOR_NONE)` in constructor.

`SetDisplayActorIndex`: store value and forward to third-person camera:
```cpp
void NetworkManager::SetDisplayActorIndex(uint8_t p_index)
{
    m_localDisplayActorIndex = p_index;
    m_thirdPersonCamera.SetDisplayActorIndex(p_index);
}
```

`BroadcastLocalState`: always send a valid display index (resolve NONE to actorId-1):
```cpp
uint8_t displayIndex = m_localDisplayActorIndex;
if (displayIndex == DISPLAY_ACTOR_NONE) {
    displayIndex = actorId - 1; // actorId already validated above
}
msg.displayActorIndex = displayIndex;
```

`HandleState` — replace the actorId-change respawn logic (lines 387-392) with display-actor-change logic:
```cpp
// Respawn only if display actor changed (not on actorId change)
if (it->second->GetDisplayActorIndex() != p_msg.displayActorIndex) {
    it->second->Despawn();
    m_remotePlayers.erase(it);
    CreateAndSpawnPlayer(peerId, p_msg.actorId, p_msg.displayActorIndex);
    it = m_remotePlayers.find(peerId);
}
else if (IsValidActorId(p_msg.actorId)) {
    it->second->SetActorId(p_msg.actorId); // Update for future use, no visual change
}
```

Also update the fallback player creation (line 381) to pass displayActorIndex:
```cpp
CreateAndSpawnPlayer(peerId, p_msg.actorId, p_msg.displayActorIndex);
```

`CreateAndSpawnPlayer`: pass displayActorIndex to RemotePlayer constructor:
```cpp
RemotePlayer* NetworkManager::CreateAndSpawnPlayer(uint32_t p_peerId, uint8_t p_actorId, uint8_t p_displayActorIndex)
{
    auto player = std::make_unique<RemotePlayer>(p_peerId, p_actorId, p_displayActorIndex);
    // ... rest unchanged
}
```

### Step 4: ThirdPersonCamera — Display clone for local player

**File:** `extensions/include/extensions/multiplayer/thirdpersoncamera.h`

Add public:
```cpp
void SetDisplayActorIndex(uint8_t p_index);
```

Add private:
```cpp
uint8_t m_displayActorIndex;
LegoROI* m_displayROI;           // Owned clone; nullptr = use native ROI
char m_displayUniqueName[32];
void CreateDisplayClone();
void DestroyDisplayClone();
bool HasDisplayOverride() const { return m_displayROI != nullptr; }
```

**File:** `extensions/src/multiplayer/thirdpersoncamera.cpp`

Add includes: `#include "extensions/multiplayer/charactercloner.h"`, `#include "legocharactermanager.h"`

Constructor: initialize `m_displayActorIndex(DISPLAY_ACTOR_NONE)`, `m_displayROI(nullptr)`, zero `m_displayUniqueName`.

`SetDisplayActorIndex`: just store the index:
```cpp
void ThirdPersonCamera::SetDisplayActorIndex(uint8_t p_index)
{
    m_displayActorIndex = p_index;
}
```

`CreateDisplayClone`: create clone via CharacterCloner (reuse existing `CharacterCloner::Clone` at `charactercloner.h:14`):
```cpp
void ThirdPersonCamera::CreateDisplayClone()
{
    if (!IsValidDisplayActorIndex(m_displayActorIndex)) {
        return;
    }
    LegoCharacterManager* charMgr = CharacterManager();
    const char* actorName = charMgr->GetActorName(m_displayActorIndex);
    if (!actorName) {
        return;
    }
    SDL_snprintf(m_displayUniqueName, sizeof(m_displayUniqueName), "tp_display");
    m_displayROI = CharacterCloner::Clone(charMgr, m_displayUniqueName, actorName);
}
```

`DestroyDisplayClone`: clean up owned clone:
```cpp
void ThirdPersonCamera::DestroyDisplayClone()
{
    if (m_displayROI) {
        VideoManager()->Get3DManager()->Remove(*m_displayROI);
        CharacterManager()->ReleaseActor(m_displayUniqueName);
        m_displayROI = nullptr;
    }
}
```

`OnActorEnter` (walking character path, ~line 94): when display override active, use clone instead of native ROI:
```cpp
if (IsValidDisplayActorIndex(m_displayActorIndex)) {
    newROI->SetVisibility(FALSE); // hide native ROI
    if (!m_displayROI) {
        CreateDisplayClone();
    }
    if (!m_displayROI) {
        return; // clone failed
    }
    m_playerROI = m_displayROI;
} else {
    m_playerROI = newROI;
}
m_currentVehicleType = VEHICLE_NONE;
m_active = true;
m_playerROI->SetVisibility(TRUE);
VideoManager()->Get3DManager()->Remove(*m_playerROI);
VideoManager()->Get3DManager()->Add(*m_playerROI);
// ... build anim caches, setup camera (unchanged)
```

`OnActorExit` (walking exit, ~line 139): hide clone but don't destroy it (persists across actor changes):
```cpp
if (m_active && static_cast<LegoPathActor*>(p_actor) == UserActor()) {
    if (m_playerROI) {
        m_playerROI->SetVisibility(FALSE);
        VideoManager()->Get3DManager()->Remove(*m_playerROI);
    }
    // ... existing cleanup (ClearRideAnimation, ClearAnimCaches, etc.)
    m_playerROI = nullptr;
    m_active = false;
}
```

`ReinitForCharacter` (~line 540, walking character init): use clone if override active:
```cpp
if (IsValidDisplayActorIndex(m_displayActorIndex)) {
    if (!m_displayROI) {
        CreateDisplayClone();
    }
    if (!m_displayROI) {
        m_active = false;
        return;
    }
    roi->SetVisibility(FALSE); // hide native
    m_playerROI = m_displayROI;
} else {
    m_playerROI = roi;
}
// ... rest of init (SetVisibility, Add to 3D, build caches, etc.)
```

`OnWorldDisabled`: destroy the display clone (animation presenters are invalidated):
```cpp
// Add before existing cleanup:
DestroyDisplayClone();
```

`Disable`: destroy display clone on full teardown:
```cpp
// Add to Disable():
DestroyDisplayClone();
```

`Tick` (walking mode, ~line 207): sync display clone position from actual player ROI before animation:
```cpp
LegoPathActor* userActor = UserActor();
if (!userActor) {
    return;
}

// Sync display clone position from native ROI
if (m_displayROI && m_displayROI == m_playerROI) {
    LegoROI* nativeROI = userActor->GetROI();
    if (nativeROI) {
        MxMatrix mat(nativeROI->GetLocal2World());
        m_displayROI->WrappedSetLocal2WorldWithWorldDataUpdate(mat);
        VideoManager()->Get3DManager()->Moved(*m_displayROI);
    }
}
// ... rest of animation code uses m_playerROI (unchanged)
```

### Step 5: INI Setting — Read and resolve actor name

**File:** `extensions/src/multiplayer.cpp`

Add include: `#include "legoactors.h"` (provides `extern LegoActorInfo g_actorInfoInit[66]`)

Add static resolver before `Initialize()`:
```cpp
static uint8_t ResolveDisplayActorIndex(const char* p_name)
{
    for (int i = 0; i < static_cast<int>(sizeOfArray(g_actorInfoInit)); i++) {
        if (!SDL_strcasecmp(g_actorInfoInit[i].m_name, p_name)) {
            return static_cast<uint8_t>(i);
        }
    }
    return Multiplayer::DISPLAY_ACTOR_NONE;
}
```

In `Initialize()`, after creating NetworkManager, read and apply setting:
```cpp
auto actorIt = options.find("multiplayer:actor");
if (actorIt != options.end() && !actorIt->second.empty()) {
    uint8_t displayIndex = ResolveDisplayActorIndex(actorIt->second.c_str());
    if (displayIndex != Multiplayer::DISPLAY_ACTOR_NONE) {
        s_networkManager->SetDisplayActorIndex(displayIndex);
    }
}
```

## Key Reused Functions

| Function | File | Purpose |
|----------|------|---------|
| `CharacterCloner::Clone(charMgr, uniqueName, characterType)` | `extensions/src/multiplayer/charactercloner.cpp` | Clone any actor by name |
| `CharacterManager()->GetActorName(index)` | `LEGO1/.../legocharactermanager.cpp:231` | Index → actor name |
| `CharacterManager()->GetNumActors()` | `LEGO1/.../legocharactermanager.cpp:243` | Returns 66 |
| `LegoActor::GetActorName(actorId)` | `LEGO1/.../legoactor.cpp:125` | ActorId → name (fallback) |
| `g_actorInfoInit[66]` | `LEGO1/.../legoactors.h:75` (extern) | Name lookup for INI resolution |
| `CharacterManager()->ReleaseActor(name)` | `LEGO1/.../legocharactermanager.h:83` | Clean up cloned ROI |

## Edge Cases

- **Invalid INI value**: `ResolveDisplayActorIndex` returns `DISPLAY_ACTOR_NONE`, player uses default actorId-based display
- **Actor change mid-game (with INI override)**: actorId changes but `displayActorIndex` stays the same (fixed by INI); remote players update stored actorId without respawning; local display clone persists unchanged
- **No INI override (backward-compatible behavior)**: `m_localDisplayActorIndex` stays `DISPLAY_ACTOR_NONE`; `BroadcastLocalState` resolves to `actorId - 1` dynamically each frame. When the player changes in-game character (e.g., Pepper→Nick), actorId goes 1→4, so `displayActorIndex` goes 0→3. Remote side detects the changed `displayActorIndex` and respawns with the new character model — preserving current behavior exactly. ThirdPersonCamera uses native ROI directly (no clone), so it also updates naturally with the new character
- **Vehicle entry**: Display clone is hidden for large vehicles (existing visibility logic in `SetVisible`/`OnActorEnter`). For small vehicles, ride animation uses `m_playerROI` (the clone) — works because all actors share the same skeleton
- **World transitions**: Display clone is destroyed on `OnWorldDisabled` (animation caches become stale). Recreated on next `ReinitForCharacter`/`OnActorEnter` via lazy `CreateDisplayClone`

## Verification

1. **Build**: Compile the project with `EXTENSIONS` enabled
2. **No override**: Run without `multiplayer:actor` in INI — verify existing behavior unchanged:
   - Remote players show correct characters
   - Third-person camera works with native ROI
   - When a player changes in-game character (Pepper→Nick), the remote player ROI changes accordingly (displayActorIndex changes from 0→3, triggering respawn)
3. **With override**: Set `multiplayer:actor=rhoda` (or any g_actorInfoInit name) — verify:
   - Local third-person camera shows the chosen actor model
   - Remote players see the chosen actor model
   - Changing in-game character (Pepper→Nick) doesn't change the visual display
   - Vehicle entry/exit works correctly with display clone
   - World transitions don't crash (clone recreated properly)
4. **Two players with different overrides**: Verify both display correctly to each other
