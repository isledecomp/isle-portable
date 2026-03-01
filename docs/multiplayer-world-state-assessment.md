# Assessment: Sharing Overworld State in Multiplayer

## Context

The multiplayer extension syncs player character positions/animations (~52 bytes at 15 Hz per peer) and, as of the Tier 1 implementation, shared plant and building state. The remaining question is: what additional world state is worth syncing, and what are the trade-offs?

## The Short Answer

Tier 1 (plants + buildings) is **implemented**. The remaining tiers vary significantly in value. Tier 4 (vehicle world presence) is the highest-value next step but is complicated by autonomous vehicle coasting after exit. Tier 2 (NPC customization) has **low practical value** because NPCs are randomly spawned and pathed per-session -- players never see the same NPCs in the same locations. Tier 3 (vehicle colors + environment) is low-effort but low-value.

---

## What State Exists to Share

| Category | Count | Bytes (serialized) | Mutation Frequency | Visual Impact |
|----------|-------|--------------------|--------------------|---------------|
| Plants | 81 | 972 | Click-driven (low) | HIGH - variant, color, height visible |
| Buildings | 16 | 161 | Click-driven (low) | MEDIUM - mood, animation |
| Characters (NPCs) | 66 | 1,056 | Click-driven (low) | MEDIUM - hat, colors |
| Vehicle colors | 43 | ~860 | Build-screen only | LOW |
| Environment | 2 | ~60 | Rare | LOW |
| **Total** | | **~3,100 bytes** | | |

The entire syncable world state fits in a single WebSocket frame. Bandwidth is a non-issue.

## The Core Problem: No Authority Model

The current system is fully symmetric -- all peers are equal, the relay server (`extensions/src/multiplayer/server/relay.ts`) just stamps peer IDs and broadcasts. For shared world state, exactly one peer must be the "source of truth."

### Recommended: Server-Assigned Host

The relay server already assigns peer IDs incrementally. Adding host tracking is ~40-60 lines of TypeScript:

- First peer to connect becomes host; server sends `MSG_HOST_ASSIGN`
- On host disconnect, lowest-ID remaining peer becomes host; server broadcasts new `MSG_HOST_ASSIGN`
- New peers receive `MSG_HOST_ASSIGN` on connect so they know who the host is

This is clearly better than client-side election (no race conditions, no timeout-based detection). The server is the one entity that knows the full connection set.

### Host Migration Risk

When the host disconnects, the new host has its own local copy of the world (which was being kept in sync). It immediately broadcasts a full snapshot to converge everyone. The data-loss window is tiny -- at most one unbroadcast click. For plants/buildings this is imperceptible.

## The Second Core Problem: Intercepting Mutations

This is where the real complexity lives. State mutations happen via direct function calls with no event system:

```
Player clicks plant -> LegoEntity::Notify() -> SwitchVariant()
    -> PlantManager()->SwitchVariant(this)  // directly mutates g_plantInfo[81]
```

The `Switch*` methods in `legoplantmanager.cpp`, `legobuildingmanager.cpp`, and `legocharactermanager.cpp` modify global arrays in-place. There is no observer pattern, no event bus, nothing to hook into.

### Recommended: Single Extension Hook in LegoEntity::Notify()

Add one `Extensions::Call()` invocation in `LegoEntity::Notify()` (in `legoentity.cpp`), following the established pattern already used in `LegoWorld::Enable()`. This is the natural chokepoint -- all click-driven mutations flow through here.

The extension hook would:
- **If host**: allow mutation locally, then broadcast a `MSG_WORLD_EVENT` (12 bytes: entity type, index, change type, actor ID)
- **If non-host**: block the local mutation, send a `MSG_WORLD_EVENT_REQUEST` (same 12-byte format as `MSG_WORLD_EVENT`) to the host via the relay, wait for the host's broadcast to apply it

Non-host click latency would be one relay round trip (20-100ms depending on region) -- imperceptible for clicking on plants.

**Important edge case: host outside the Isle world.** The host may not be in the Isle world when a non-host's `MSG_WORLD_EVENT_REQUEST` arrives (e.g., the host is in a building interior). The host's `m_entity` pointers are also NULL in this case, so the host cannot call `Switch*` methods either. The host must apply the mutation to its data arrays directly (same increment/cycle logic as the `Switch*` methods, without the ROI visual update), then broadcast the change-type event. This works because the data arrays (`g_plantInfo[]`, `g_buildingInfo[]`) persist across world unloads.

**Why change-type events, not value-based events:** The `Switch*` methods are increment/cycle operations (e.g., `SwitchColor` does `color = (color + 1) % 5`), and they combine the data mutation with the visual update (ROI/LOD swap) in a single call. If `MSG_WORLD_EVENT` carried a resulting value like "color = RED", receivers couldn't easily use the existing `Switch*` methods -- they'd need separate visual-update-only helpers that don't exist today.

Instead, `MSG_WORLD_EVENT` carries a **change type** (e.g., "cycle color on plant 42"). Since WebSocket/TCP guarantees in-order delivery and all peers start from the same snapshot, applying the same sequence of change-type events keeps everyone in sync:
- **In-world peers**: call the same `Switch*` method, which increments the data AND updates the ROI
- **Out-of-world peers**: apply the same increment logic to the data arrays directly (no visual update needed; `LoadWorldInfo()` rebuilds visuals from the data when re-entering the Isle world)
- **Host outside the Isle world**: same as out-of-world peers -- apply data increment, then broadcast the event

## Synchronization: Hybrid Approach

**On join**: New peer requests a snapshot from the host. Host serializes using the *existing* `PlantManager::Write()` / `BuildingManager::Write()` / `CharacterManager::Write()` methods via the existing `LegoMemory` class (a `LegoStorage` subclass at `legostorage.h:194` that reads/writes to a `uint8_t*` buffer -- no new adapter class needed). New peer deserializes using the existing `Read()` methods. This reuses all existing serialization code.

**During play**: Individual mutations broadcast as 12-byte `MSG_WORLD_EVENT` messages. Receiving peers that are currently in the Isle world call the same `Switch*` methods locally to get both data mutation and visual update in one step. Peers that are NOT in the Isle world (in Act 2, a building, etc.) instead write the changed field directly to the `g_plantInfo[]` / `g_buildingInfo[]` arrays -- the visual update happens automatically when they re-enter the Isle world and `LoadWorldInfo()` rebuilds entities from the updated data.

**Late joiner edge case**: Queue any `MSG_WORLD_EVENT` messages that arrive between snapshot request and snapshot response, apply them after the snapshot.

## Protocol Changes

```
New message types needed:
  MSG_HOST_ASSIGN          (13 bytes)  - server -> all, announces host peer ID
  MSG_REQUEST_SNAPSHOT      (9 bytes)  - client -> host, request full world state
  MSG_WORLD_SNAPSHOT       (~3.1 KB)   - host -> client, full world state blob
  MSG_WORLD_EVENT          (12 bytes)  - host -> all, single state mutation (change type, not value)
  MSG_WORLD_EVENT_REQUEST  (12 bytes)  - non-host -> host, request a mutation (same format)
```

Steady-state bandwidth addition: essentially zero (events are click-driven, ~0.1 Hz). Snapshot is a one-time 3 KB message on join.

## Tiered Implementation

### Tier 1: Plants + Buildings ~~IMPLEMENTED~~
- **Status**: Complete. Host election, snapshot sync, event-based deltas, mutation interception hook, and visual state application are all working.
- This established the authority model (server-assigned host), the hybrid sync pattern (snapshot on join + change-type events during play), and the extension hook in `LegoEntity::Notify()`. All subsequent tiers build on this foundation.

### Tier 2: NPC Character Customization ~~LOW VALUE -- DEPRIORITIZE~~
- **Value**: **LOW** (downgraded from MEDIUM)
- **Effort**: ~150-200 additional lines
- **Risk**: Character customization involves LOD cloning and texture swapping (`legocharactermanager.cpp:807-858`). Remote triggering could have subtle visual bugs.
- **Problem**: NPCs are **randomly spawned and autonomously pathed per-session**, making customization sync nearly meaningless in practice:
  - NPCs spawn on randomized `LegoPathBoundary` paths. The walking mode is selected via a global counter (`g_pathWalkingModeSelector++`) combined with `SDL_rand()` calls for edge selection.
  - NPC speeds are randomized (`0.9f + 1.5f * SDL_randf()` for walking).
  - NPC **positions are never saved** -- only customization data (hats, colors, mood) persists across world loads. On every world load, NPCs are recreated at different boundaries with different paths.
  - This means players will never see the same NPCs in the same places. If Player A customizes "pepper" with a red hat, Player B would need to find "pepper" somewhere in their own world to notice -- but "pepper" is on a completely different path walking in a different direction.
  - The only way NPC customization sync would have visible impact is if NPC positions/paths were also synced, which would require deterministic spawning, synchronized pathfinding, and shared random seeds -- a massive undertaking far beyond this tier's scope.

### Tier 3: Vehicle Colors + Environment
- **Value**: LOW -- vehicle colors are rarely changed during gameplay, environment changes are cosmetic
- **Effort**: ~100-150 additional lines

### Tier 4: Vehicle World Presence (placement, enter/exit, visibility, coasting)
- **Value**: HIGH -- without this, each player sees all 7 vehicles sitting idle in the world even when another player is driving one. When Player A enters the helicopter, Player B should see it disappear (it's "in use"), and when Player A exits, it should reappear at the exit location for everyone.
- **Effort**: ~500-800 additional lines (**HIGH** complexity)
- **Scope**:
  - Sync vehicle enter/exit events so vehicles disappear/reappear for all peers
  - Sync vehicle positions during post-exit coasting (see below)
  - Sync vehicle final resting position
  - Sync custom textures for built vehicles (helicopter windshield, jetski front, etc.)

**How vehicles work:**

There are 7 vehicles in the Isle world: motorcycle, bike, skateboard, helicopter, jetski, dunebuggy, racecar. Their world state is stored in `Act1State` as `LegoNamedPlane` structs (76 bytes each: name + position + direction + up vector). Built vehicles (helicopter, jetski, dunebuggy, racecar) also have custom textures (`LegoNamedTexture*`).

**Vehicle enter/exit lifecycle:**
1. **Idle**: Vehicle is visible in the world via `m_roi->SetVisibility(TRUE)`
2. **Enter** (`IslePathActor::Enter()`): `m_roi->SetVisibility(FALSE)` -- vehicle disappears, player takes control
3. **Active**: Vehicle is invisible; dashboard UI is shown; vehicle is the `UserActor`
4. **Exit** (`IslePathActor::Exit()`): `m_roi->SetVisibility(TRUE)` -- vehicle reappears at exit position **and continues moving autonomously**

**Autonomous vehicle coasting (critical finding):**

Vehicles do **NOT** stop when a player exits them. `IslePathActor::Exit()` sets `m_userNavFlag = FALSE` and `m_actorState = c_initial`, but **never resets `m_worldSpeed`**. The vehicle retains whatever velocity it had at the moment of exit and continues following its path boundary:

1. `LegoAnimActor::Animate()` is called every frame. It checks `m_actorState == c_initial && !m_userNavFlag && m_worldSpeed <= 0` to decide whether to idle. Since `m_worldSpeed > 0`, this condition is FALSE, so it calls `LegoPathActor::Animate()`.
2. `LegoPathActor::Animate()` calls `CalculateTransform()` in a while-loop until caught up to the current time.
3. `CalculateTransform()` sees `m_worldSpeed > 0` with `m_userNavFlag = FALSE` and continues advancing the vehicle along the spline path, evaluating `m_spline.Evaluate(m_traveledDistance / m_BADuration, ...)` with the retained speed.
4. The vehicle coasts along path boundaries, potentially transitioning between boundaries, until speed depletes or it reaches a dead end.

None of the 7 vehicle `Exit()` methods reset `m_worldSpeed` -- they all delegate to `IslePathActor::Exit()`.

**Sync approach (revised):**
- `MSG_WORLD_EVENT` with `EVENT_VEHICLE_ENTER` / `EVENT_VEHICLE_EXIT` types
- On enter: host broadcasts which vehicle was entered; all peers call `SetVisibility(FALSE)` on that vehicle's ROI
- On exit: host broadcasts the vehicle exit event; all peers call `SetVisibility(TRUE)`. **But the vehicle is now coasting**, so one of two approaches is needed:
  - **Option A: Periodic position sync while coasting.** The host periodically broadcasts the coasting vehicle's position/orientation (similar to player position sync but at a lower frequency, e.g., 5 Hz) until the vehicle stops (`m_worldSpeed <= 0`). Then a final position is broadcast. This is simpler but adds bandwidth during coasting.
  - **Option B: Deterministic path replay.** Sync the path state (boundary ID, spline distance, speed) and let each client simulate independently. Much harder -- requires all clients to have identical path data and deterministic spline evaluation, and boundary transitions would need to match exactly. Fragile and not recommended.
- On snapshot: include which vehicles are currently "in use" (a bitmask, 1 byte) plus the saved `LegoNamedPlane` for each parked vehicle, plus coasting state for any currently-coasting vehicles

**Simplified Tier 4a (enter/exit visibility only):** As a stepping stone, only sync enter/exit visibility (hide when entered, show when exited) without syncing the coasting position. The vehicle would "teleport" to its final resting position when it stops. This removes the worst artifact (seeing idle vehicles that are being driven) with much less complexity (~300 lines). Position sync during coasting could be added later.

**Key complexity**: Built vehicles have custom textures that are serialized as bitmap data (variable size, potentially several KB each). For Tier 4, syncing textures on join would require including them in the snapshot. An alternative is to skip texture sync initially and use default textures for remote players' built vehicles -- visually imperfect but functional.

**Out-of-world handling**: Vehicle state is managed by `Act1State`, which persists across world transitions. `RemoveActors()` calls `UpdatePlane()` to save positions when leaving the Isle; `PlaceActors()` restores them on re-entry. The same direct-write approach from Tier 1 applies: update the `LegoNamedPlane` data in `Act1State` when the receiving peer is outside the Isle world.

**Data size:**
| Component | Size |
|-----------|------|
| 7 vehicle planes | ~532 bytes (7 x 76) |
| Vehicle-in-use bitmask | 1 byte |
| Custom textures (optional) | 0-50+ KB (bitmap data) |
| 43 vehicle color strings | ~500-1,000 bytes |

## Estimated Effort Summary

| Component | New Lines | Difficulty | Status |
|-----------|-----------|------------|--------|
| ~~Tier 1: Plants + Buildings~~ | ~~750-1,250~~ | ~~Hard~~ | **DONE** |
| Tier 2: NPC Customization | 150-200 | Medium | Deprioritized (low value) |
| Tier 3: Vehicle Colors + Environment | 100-150 | Easy | |
| Tier 4a: Vehicle enter/exit visibility only | ~300 | Medium | |
| Tier 4b: + coasting position sync | 200-500 | **Hard** (path system integration) | |
| **Total (Tier 4a)** | **~300** | | |
| **Total (Tier 4a+4b)** | **~500-800** | | |

**Recommended priority order:**
1. **Tier 4a** (vehicle visibility) -- highest visual impact for moderate effort
2. **Tier 4b** (coasting sync) -- incremental on top of 4a, hardest remaining piece
3. **Tier 3** (vehicle colors + environment) -- small incremental work
4. ~~**Tier 2**~~ (NPC customization) -- deprioritized, near-zero perceptible benefit without NPC position sync

## Open Questions and Risks

1. **Sound on remote mutations**: `ClickSound()` uses 3D positional audio via `Lego3DSound`. Sounds are anchored to the entity's world position and naturally attenuate with distance (min 15, max 100 units; effectively silent beyond ~100 units). This means **sounds should be played on remote peers** -- a player clicking a plant far away from you will be quiet or silent. The 3D sound system handles this correctly without any special networking logic. The `ClickAnimation()` visual effect should also play for the same reason.

2. **Conflict resolution**: Two players click the same plant simultaneously. With host authority, the host processes them sequentially -- the second click advances from the state the first click left it in. Both players see the same result. No special handling needed.

3. **State sync for players not in the Isle world** (IMPORTANT): When a player leaves the Isle world (enters Act 2, a building interior, etc.), `PlantManager()->Reset()` and `BuildingManager()->Reset()` set all `m_entity` pointers to NULL. Several `Switch*` methods access `m_entity->GetROI()` without NULL checks and will **crash** if called when the entity doesn't exist:
   - **CRASHES outside Isle**: `SwitchColor()`, `SwitchVariant()`, `DecrementCounter()` (both plant and building) -- access `m_entity->GetROI()` which is NULL
   - **NO-OPS outside Isle**: `SwitchSound()`, `SwitchMove()`, `SwitchMood()` -- these use `GetInfo(p_entity)` which searches `g_plantInfo[].m_entity` for a matching pointer. When entities are NULL, the search finds nothing and returns NULL, so the method does nothing. They don't crash, but they also **don't apply the data change**.

   **Solution**: When receiving state updates from other players while not in the Isle world, **ALL methods require direct array writes** -- not just the crash-prone ones. The data fields (variant, color, mood, sound, move, counter) persist across world unloads. When the player re-enters the Isle world, `LoadWorldInfo()` → `CreatePlant()` / `CreateBuilding()` reads these fields to create entities with the correct appearance. The receiving code must check `info->m_entity != NULL` and branch accordingly: if non-NULL, call the `LegoEntity::Switch*` method (which handles data + visual + sound + animation); if NULL, write the field directly to the data array by index.

4. **Testing**: Requires testing late-joiner sync, host migration, simultaneous clicks, snapshot + queued events ordering, and state application when players are in different worlds. The existing `relay.ts` dev server (via `wrangler dev`) helps, but automated multi-client testing would need a new test harness.

5. **World reload**: When a player leaves and re-enters the Isle world, `PlantManager::LoadWorldInfo()` re-creates plants from `g_plantInfo[]`. As noted in point 3, if the data arrays have been kept in sync via direct writes, the re-created entities will have the correct appearance. This is confirmed to work because `CreatePlant()` reads `g_plantInfo[index].m_variant` and `g_plantInfo[index].m_color` to select the correct LOD list.

## Verified Findings & Additional Gotchas (code review)

The following were discovered by cross-checking every claim against the actual source code:

6. **`LegoMemory` already exists** (`legostorage.h:194`): The plan proposed creating a "MemoryStorage adapter" but `LegoMemory` is already a `LegoStorage` subclass that reads/writes to a `uint8_t*` buffer with position tracking. This eliminates ~50-80 lines of estimated work.

7. **`BuildingManager::SwitchVariant()` is disabled by default**: `g_buildingManagerConfig` is initialized to 1 (`legobuildingmanager.cpp:222`), and `SwitchVariant()` returns TRUE immediately when `g_buildingManagerConfig <= 1`. Building variant switching only works if this config is explicitly set to > 1 (via a registry/config setting from the original game). This means building variant sync may be a no-op in practice.

8. **`BuildingManager::SwitchVariant()` creates a new entity**: Unlike plant mutations (which swap LOD lists), `SwitchVariant()` at line 474 calls `CreateBuilding(HAUS1_INDEX, CurrentWorld())`, which requires the Isle world to be loaded. This mutation **cannot be applied via direct array writes** for out-of-world peers -- you can write `m_variant` to the data array, but the visual swap requires `CreateBuilding()`. For out-of-world peers, the data write is sufficient; `LoadWorldInfo()` will create the correct variant on re-entry.

9. **`ClickAnimation()` blocks rapid clicks**: `ClickAnimation()` sets `m_interaction |= c_disabled` (`legoentity.cpp:340`), preventing further clicks until the animation finishes. Both `ClickSound()` and `ClickAnimation()` check `!IsInteraction(c_disabled)` and skip if set. If a remote world event arrives while an entity is already animating, the data mutation (via the manager's `Switch*` method) still applies correctly, but the sound and animation for that remote event will be silently skipped. This is consistent with single-player behavior but means rapid multiplayer clicks may have fewer visual/audio effects than expected.

10. **Every mutation decrements the interaction counter**: `ClickAnimation()` triggers `ScheduleAnimation()`, which calls `AdjustCounter(-1)` for plants and `AdjustCounter(-2)` for buildings. This means every click-driven mutation decrements the entity's counter as a side effect through the animation system. When the counter reaches 0, the entity sinks and disappears. This is already correctly captured when calling `LegoEntity::SwitchVariant()` etc. (which call `ClickAnimation()` internally), but it means **the receiving side must call the full `LegoEntity::Switch*` method (not just the manager method)** to get the counter decrement and animation.

11. **Plant `Write()` / `Read()` asymmetry**: `Write()` saves `m_initialCounter` but `Read()` reads into `m_counter` (then copies to `m_initialCounter` and calls `AdjustHeight()`). The snapshot sender must be aware that what gets written is `initialCounter`, not the current `counter`. This matters if a plant has been partially decremented -- the snapshot should send the current state, not the initial state. The snapshot serialization should either use the existing `Write()`/`Read()` pair as-is (which saves/restores `initialCounter` and adjusts heights accordingly), or use a custom serialization that captures `m_counter` directly.

12. **Snapshot `Read()` doesn't update visual positions**: `PlantManager::Read()` calls `AdjustHeight()` which modifies `g_plantInfo[].m_position[1]` (Y coordinate) but does NOT call `SetLocation()` on the entity. If a snapshot is received while in the Isle world, the entity's visual position won't update until the player leaves and re-enters. An additional `SetLocation()` call per entity is needed after applying a snapshot in-world. Same issue applies to buildings via `AdjustHeight()` in `BuildingManager::Read()`.

13. **PlayerStateMsg is 52 bytes, not 48**: Header(9) + actorId(1) + worldId(1) + vehicleType(1) + position(12) + direction(12) + up(12) + speed(4) = 52 bytes. The bandwidth estimate changes negligibly (from 720 to 780 bytes/s per peer at 15 Hz).

## Impact Assessment of Verified Findings

None of the verified findings above are architectural blockers. Summary:

- **#6 (LegoMemory exists)**: Positive — reduces work.
- **#7 (SwitchVariant disabled)**: Neutral — less to sync; already handled if config changes.
- **#8 (CreateBuilding for SwitchVariant)**: Already handled — out-of-world peers use direct array writes; `LoadWorldInfo()` creates the correct variant on re-entry.
- **#9 (ClickAnimation blocks rapid clicks)**: Matches single-player behavior — data still applies, only sound/animation skipped. Prevents audio spam.
- **#10 (Counter decrement side effect)**: Most important finding. Receiving peers **must call `LegoEntity::Switch*()` methods** (not `PlantManager::Switch*()` directly) to get the full effect chain (data + visual + sound + animation + counter decrement). For out-of-world peers, direct array writes must also decrement `m_counter` and call `AdjustHeight()`.
- **#11 (Write/Read asymmetry)**: Manageable — use existing `Write()`/`Read()` pair as-is for snapshots. The sequence "snapshot (initial state) + queued change-type events" produces the correct final state.
- **#12 (Snapshot Read doesn't update visuals)**: Minor — add a `SetLocation()` loop (~10 lines) after applying a snapshot in-world.
- **#13 (52 bytes not 48)**: Cosmetic correction, no design impact.

## Opinion

With Tier 1 implemented, the authority model, hybrid sync pattern, and mutation interception architecture are all proven. The remaining tiers build on this foundation.

**Tier 4 (vehicles) is the clear next priority** -- seeing idle vehicles that another player is driving is the most visible remaining multiplayer artifact. The enter/exit visibility sync (Tier 4a) is straightforward and high-impact. The coasting position sync (Tier 4b) is the harder part: vehicles retain their speed after exit and continue following path boundaries via `CalculateTransform()` with spline evaluation. This requires either periodic position broadcasts (simple but adds bandwidth) or deterministic path replay (fragile). A pragmatic approach is to implement 4a first, then add 4b with periodic position sync.

**Tier 2 (NPC customization) should be deprioritized or dropped.** NPCs are randomly spawned on randomized path boundaries with randomized walking modes and speeds (`g_pathWalkingModeSelector`, `SDL_rand()`, `SDL_randf()`). NPC positions are never saved -- only customization data persists. This means players never see the same NPCs in the same places, so syncing appearance changes produces near-zero perceptible benefit. Making NPC sync meaningful would require deterministic spawning and synchronized pathfinding -- a fundamentally different (and much larger) undertaking.

**Tier 3 (vehicle colors + environment) is low-effort and low-value** -- fine as incremental work whenever convenient.

## Key Files

- `extensions/src/multiplayer/server/relay.ts` - host tracking additions
- `extensions/include/extensions/multiplayer/protocol.h` - new message types
- `extensions/src/multiplayer/networkmanager.cpp` - world sync logic
- `extensions/include/extensions/multiplayer/networkmanager.h` - new fields/methods
- `LEGO1/lego/legoomni/src/entity/legoentity.cpp` - extension hook in Notify()
- `LEGO1/lego/legoomni/src/common/legoplantmanager.cpp` - existing Write/Read to reuse
- `LEGO1/lego/legoomni/src/common/legobuildingmanager.cpp` - existing Write/Read to reuse
- `LEGO1/lego/legoomni/include/legoplants.h` - LegoPlantInfo struct definition
- `LEGO1/lego/legoomni/include/legobuildingmanager.h` - LegoBuildingInfo struct definition
- `LEGO1/lego/legoomni/include/isle.h` - Act1State with vehicle LegoNamedPlane fields
- `LEGO1/lego/legoomni/src/worlds/isle.cpp` - Act1State::Serialize(), RemoveActors(), PlaceActors()
- `LEGO1/lego/legoomni/src/actors/islepathactor.cpp` - Enter()/Exit() with SetVisibility toggle
- `LEGO1/lego/legoomni/src/paths/legopathactor.cpp` - CalculateTransform() autonomous coasting logic
- `LEGO1/lego/legoomni/src/paths/legoanimactor.cpp` - Animate() frame loop (m_worldSpeed check)
- `LEGO1/lego/legoomni/include/legonamedplane.h` - LegoNamedPlane struct (position/direction/up)
