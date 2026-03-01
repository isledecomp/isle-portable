# Phase 1: URL Rooms, Animation Triggers, and Svelte Overlay

## Context

The multiplayer backend works: Cloudflare Durable Objects relay, binary protocol (15Hz player state, world events), remote player rendering, host election. But there's no user-facing interface -- room ID is hardcoded to `"default"` in `ISLE/emscripten/config.cpp`, everyone auto-joins one room. The isle.pizza Svelte 5 frontend hosts the WASM game but has zero multiplayer screens.

The relay can be extended. No auth system.

## What We're Building

URL-based rooms + animation trigger system + Svelte overlay.

- **Room code in URL**: `isle.pizza/#r/brave-red-brick`
- **Configurable room size**: creator picks max players (up to a relay-enforced ceiling)
- **Svelte overlay** with buttons that trigger animations on the local player (presented as "emotes" in the UI, but internally just animation triggers)
- **Collapsible in-game overlay** -- panel can be minimized to stay out of the way
- **Room preview** on isle.pizza (player count before joining)
- **No text chat** -- emotes are the communication
- **Connection lifecycle handled by INI config** at startup (no `mp_connect`/`mp_disconnect` from Svelte)

### Animation Categories

The multiplayer UI supports three types of player animations:

1. **Walking animation** -- plays while the player is moving. Persistent setting (stays until changed).
2. **Idle animation** -- plays while stationary, after ~2.5s of not moving. Persistent setting (stays until changed).
3. **One-off emotes** -- plays one full cycle while stationary. Movement interrupts. After completing, returns to the 2.5s timeout → idle animation flow. Triggers an emoji popup in the local player's UI (similar to the debug menu activation feedback).

All three are selected by the player through the overlay UI. Internally, the protocol and C++ exports deal in animation name strings (e.g., `"CNs003xx"`) -- the Svelte overlay maps UI labels to these names.

## User Flow

```
isle.pizza                            → solo play (multiplayer disabled in INI)
isle.pizza/#r/brave-red-brick         → auto-joins room brave-red-brick
```

To play together:

1. Click "Create Room" → configure max players → generates room name → navigates to `#r/NAME`
2. Share the URL with friends
3. Friends open URL → see room preview (player count) → auto-join the same room

When launching from the main page without a room hash, the multiplayer extension is disabled in the INI config before the game starts. This ensures solo play has zero multiplayer overhead.

## Room Name Generation

Room names are three-word phrases generated from curated Lego Island-themed word lists. Each component is drawn randomly:

```
[adjective] - [color/material] - [noun]

Examples:
  brave-red-brick
  sneaky-chrome-pizza
  turbo-sandy-surfer
```

**Word lists** (defined in the Svelte frontend):

- **Adjectives**: island/adventure tone (brave, sneaky, turbo, radical, mega, super, hyper, epic, wild, rogue, swift, mighty, ...)
- **Colors/Materials**: Lego-relevant descriptors (red, blue, chrome, sandy, mossy, golden, rusty, neon, crystal, painted, ...)
- **Nouns**: Lego Island objects, locations, and character name fragments drawn from the game's 66 characters (brick, pizza, pepper, mama, papa, nick, laura, brickster, studs, rhoda, snap, infoman, clickitt, rom, ding, legando, shrimp, hogg, funberg, surfer, racer, cop, skater, island, jetski, tower, chopper, minifig, ...)

Character name inspiration from `ActorDisplayNames` in isle.pizza: Pepper Roni, Mama/Papa Brickolini, Nick/Laura Brick, Infomaniac, Brickster, Studs Linkin, Rhoda Hogg, Valerie Stubbins, Snap Lockitt, Maggie Post, Buck Pounds, Ed Mail, Nubby Stevens, Nancy Nubbins, Dr. Clickitt, Captain D. Rom, Bill Ding, Brazilian Carmen, Gideon Worse, Red Greenbase, Polly Gone, Bradford Brickford, Shiney Doris, Glen/Dorothy Funberg, Brian Shrimp, Luke Tepid, Shorty Tails, Bumpy Kindergreen, Jack O'Trades.

Each list needs ~30-50 words to produce enough unique combinations (30^3 = 27,000 possible names). Words must be URL-safe (lowercase, no special characters). The generation happens client-side in Svelte -- no server round-trip needed.

## Configurable Room Size

### Creator-Side

When creating a room, the UI offers a max player count selector (e.g., dropdown or stepper). The chosen limit is sent to the relay when the room is created and stored as room metadata.

### Relay-Side

The relay enforces a **ceiling limit** -- a hard upper bound configured in the Cloudflare Worker environment (e.g., `MAX_PLAYERS_CEILING = 64`). The room's configured max players cannot exceed this ceiling. If a room is created with a limit above the ceiling, the relay clamps it down.

When a new WebSocket connection arrives:

1. Check `connections.size` against the room's configured max players
2. If full, reject the WebSocket upgrade with an appropriate error
3. The room preview endpoint returns both `players` and `maxPlayers` so the UI can show capacity

### Protocol

The max player count is set when the room is first created (first WebSocket connection, or via a creation endpoint). It persists for the lifetime of the Durable Object. Default if unspecified: 5 (for the 5 playable characters).

## In-Game Overlay

The overlay is **collapsible** -- a small tab/handle remains visible when collapsed so players can re-expand it.

```
┌──────────────── Game Canvas ─────────────────┐
│                                              │
│                                              │
│                          ┌────────────────┐  │
│                          │ Room: brave-.. │  │
│                          │ Players: 3/5   │  │
│                          │ [Copy Link]    │  │
│                          │                │  │
│                          │ ── Emotes ──   │  │
│                          │ [emote buttons]│  │
│                          │ [.............]│  │
│                          │       [__]     │  │  ← collapse handle
│                          └────────────────┘  │
│                                              │
└──────────────────────────────────────────────┘

Collapsed:
┌──────────────── Game Canvas ─────────────────┐
│                                          [≡] │  ← expand handle
│                                              │
└──────────────────────────────────────────────┘
```

Changing a walk/idle animation or triggering an emote broadcasts to all other players through the multiplayer protocol. Animations are **not played on the local player** -- they are only visible to remote players. This avoids interfering with the local player's path actor state and movement controls. One-off emotes show an emoji popup locally as feedback.

## Svelte → WASM Bridge

One-way, minimal. The C++ side exports only what the Svelte overlay needs:

```cpp
extern "C" {
  // Set the walking animation for this player (persistent).
  // Index into the walk animation table. Included in every PlayerStateMsg.
  // Not played locally -- only visible to remote players.
  EMSCRIPTEN_KEEPALIVE void mp_set_walk_animation(int index);

  // Set the idle animation for this player (persistent).
  // Index into the idle animation table. Included in every PlayerStateMsg.
  // Not played locally -- only visible to remote players.
  EMSCRIPTEN_KEEPALIVE void mp_set_idle_animation(int index);

  // Trigger a one-off emote (plays one cycle on remote players).
  // Index into the emote table. Not played locally.
  EMSCRIPTEN_KEEPALIVE void mp_trigger_emote(int index);

  // Room info -- for the overlay to display
  EMSCRIPTEN_KEEPALIVE int mp_get_player_count();
}
```

No `mp_connect` / `mp_disconnect` -- the connection is established at WASM startup based on the room name in the INI config.

## Room Configuration via INI

### Current State

Room name is set in `ISLE/emscripten/config.cpp` as a default INI override:

```cpp
iniparser_set(p_dictionary, "multiplayer:room", "default");
```

The extension reads it in `MultiplayerExt::Initialize()`:

```cpp
room = options["multiplayer:room"];
```

### Change

The isle.pizza frontend writes multiplayer settings directly into `isle.ini` via OPFS, using the same `saveConfig()` pattern already used for display, audio, and control settings in `opfs.js`.

When navigating to a room URL (`#r/brave-red-brick`):

1. Svelte reads the room name from the URL hash
2. Writes `multiplayer:room=brave-red-brick` and `extensions:multiplayer=YES` into `isle.ini` via OPFS
3. Game starts and reads the INI as normal

When launching without a room hash (solo play):

1. Svelte writes `extensions:multiplayer=NO` into `isle.ini` via OPFS
2. The multiplayer extension is not initialized at all -- zero overhead for solo play

The hardcoded `"default"` room override in `ISLE/emscripten/config.cpp` is removed. The INI file becomes the sole authority for room configuration.

### Connection Logic

- **`extensions:multiplayer=NO`** (or missing): multiplayer extension is not loaded
- **`extensions:multiplayer=YES`** with a room name: connect to relay at `/room/{roomName}` as currently implemented
- **Empty room name with multiplayer enabled**: extension loads but skips connection (current behavior when `room.empty()`)

## Available Animation Assets

### Animation System Overview

The game has two distinct animation playback mechanisms:

| Mechanism | How It Works | Remote Player Support |
|-----------|-------------|----------------------|
| `ApplyAnimationTransformation()` + ROI map | Directly applies bone transforms from a `LegoAnim` tree at a given time. Used for walk/idle/ride cycles. | **Working** -- already used for remote players |
| `StartEntityAction()` + presenter system | Loads animations from SI files via the action/presenter pipeline. Used for click animations, disassemble/reassemble. | **Not available** -- requires the full presenter infrastructure which remote players don't have |

All multiplayer animations use `ApplyAnimationTransformation()` since it already works for remote players.

### CNs### Animations (g_cycles)

The `g_cycles[11][17]` array in `legoanimationmanager.cpp` maps character types to animation names. Each character has character-specific variants (e.g., `CNs001Pe` for Pepper), but **generic `xx` versions exist that work on any character** since all characters share the same skeleton structure.

All are loaded as `LegoAnimPresenter` objects in the world and can be looked up via `world->Find("LegoAnimPresenter", "CNs003xx")`.

**Playback for remote players**: Same pattern already used for walk/idle -- call `BuildROIMap()` to map animation bones to the character's ROI, then `ApplyAnimationTransformation()` each frame with an advancing time value.

#### Walking Animations (looping, while moving)

| Name | UI Label | Description |
|------|----------|-------------|
| `CNs001xx` | Normal | Walking upright/straight (default) |
| `CNs002xx` | Joyful | Happy, energetic walk |
| `CNs003xx` | Gloomy | Sad, leaning forward, depressed |
| `CNs005xx` | Sneaky | Walking hunched forward |
| `CNs006xx` | Scared | Frightened walk |
| `CNs007xx` | Hyper | Super excited, head pops off |

`CNs004xx` appears identical to `CNs001xx` and is excluded.

#### Idle Animations (looping, after ~2.5s stationary)

| Name | UI Label | Description |
|------|----------|-------------|
| `CNs008xx` | Sway | Body swaying back and forth (default) |
| `CNs009xx` | Groove | Happy, body swaying side to side |
| `CNs010xx` | Excited | Swinging arms energetically |

#### One-Off Emotes (single cycle, must be stationary)

| Name | UI Label | Description |
|------|----------|-------------|
| `CNs011xx` | Wave | Waving hello with one hand |
| `CNs012xx` | Hat Tip | Taking off hat and waving with it |

### Other Animation Assets (Future)

These use the presenter system which isn't available for remote players yet:

- **Click/Customize body flip** (`CUSTOMIZE_ANIM_FILE`, IDs 10-13): Full body flip animation triggered by clicking NPCs. Uses `SUBST:actor_01:characterName` to work on any character. Would need extraction to `ApplyAnimationTransformation()` or presenter system extension.
- **Disassemble/Reassemble** (`BNsDis01`-`03` / `BNsAss01`-`03`, action IDs 228-233): Character falls apart into pieces then reassembles. Same presenter portability challenge.
- **Procedural flip**: Not animation data -- pure code in `LegoExtraActor::StepState()` (`RotateX`/`RotateZ` at 0.7 rad/frame for 2 seconds). Easiest to port since it's just transform math.

## Protocol: Animation Messages

### Walk/Idle: Carried in PlayerStateMsg

Walk and idle animation selections are **persistent state** -- they must survive player joins, reconnects, and packet loss. They are included in the existing 15Hz `PlayerStateMsg`:

```
struct PlayerStateMsg {
  MessageHeader header;
  float position[3];
  float direction[3];
  float up[3];
  float speed;
  int8_t vehicleType;
  int32_t worldId;
  uint8_t walkAnimId;     // index into walk animation table (0 = default)
  uint8_t idleAnimId;     // index into idle animation table (0 = default)
};
```

Using a `uint8_t` index into a fixed animation table (rather than a string name) keeps the per-tick overhead minimal. The table mapping is shared between Svelte (UI labels) and C++ (CNs name lookup):

**Walk animation table:**

| Index | CNs Name | UI Label |
|-------|----------|----------|
| 0 | `CNs001xx` | Normal (default) |
| 1 | `CNs002xx` | Joyful |
| 2 | `CNs003xx` | Gloomy |
| 3 | `CNs005xx` | Sneaky |
| 4 | `CNs006xx` | Scared |
| 5 | `CNs007xx` | Hyper |

**Idle animation table:**

| Index | CNs Name | UI Label |
|-------|----------|----------|
| 0 | `CNs008xx` | Sway (default) |
| 1 | `CNs009xx` | Groove |
| 2 | `CNs010xx` | Excited |

This way every `PlayerStateMsg` carries the current walk/idle setting. New players joining a room automatically receive the correct animations from the first state update -- no special join-time sync needed.

### Emotes: One-Shot Message

Emotes are discrete events, not persistent state. A separate message type:

```
MSG_EMOTE = 9

struct EmoteMsg {
  MessageHeader header;      // 9 bytes (type + peerId + sequence)
  uint8_t emoteId;           // 1 byte -- index into emote table
};
```

Total: 10 bytes per emote trigger.

**Emote table:**

| Index | CNs Name | UI Label |
|-------|----------|----------|
| 0 | `CNs011xx` | Wave |
| 1 | `CNs012xx` | Hat Tip |

The relay broadcasts it to all peers. The remote player must be stationary -- if a `PlayerStateMsg` arrives indicating movement (non-zero speed) during the emote, it is **immediately interrupted**. After the emote completes (or is interrupted), the remote player returns to the ~2.5s idle timeout → idle animation flow.

### Why This Split

- **Walk/idle in PlayerStateMsg**: persistent state that must be known by all peers at all times. Piggybacking on the existing 15Hz message means zero additional messages and automatic sync on join. 2 extra bytes per tick is negligible.
- **Emotes as one-shot messages**: discrete events with "fire once, play once" semantics. Don't belong in continuous state.

## Relay: Room Preview Endpoint

Add an HTTP response to the Durable Object for non-WebSocket requests, so the Svelte UI can show player count before joining:

```typescript
// In GameRoom.fetch(), before WebSocket upgrade:
if (request.headers.get("Upgrade") !== "websocket") {
  return new Response(JSON.stringify({
    players: this.connections.size,
    maxPlayers: this.maxPlayers  // room-specific configured limit
  }), {
    headers: {
      "Content-Type": "application/json",
      "Cross-Origin-Resource-Policy": "same-site"
    }
  });
}
```

The `same-site` header is needed because isle.pizza serves `Cross-Origin-Embedder-Policy: require-corp` (required for SharedArrayBuffer / pthreads). WebSocket connections are unaffected by COEP; only HTTP fetch needs this header.

## Hosting: relay.isle.pizza

Works with the COOP/COEP setup:

- **WebSocket** (`wss://relay.isle.pizza/room/...`): unaffected by COEP
- **HTTP fetch** (`GET https://relay.isle.pizza/room/.../info`): needs `Cross-Origin-Resource-Policy: same-site` (relay and isle.pizza share the same registrable domain)

## C++ Implementation: Animation System

### Exports (Svelte → WASM)

All three exports affect remote players only -- nothing is played on the local player.

- `mp_set_walk_animation(index)`: stores the walk animation index, included in subsequent `PlayerStateMsg` broadcasts
- `mp_set_idle_animation(index)`: stores the idle animation index, included in subsequent `PlayerStateMsg` broadcasts
- `mp_trigger_emote(index)`: sends `MSG_EMOTE` to all peers

### Remote Player State Machine

The remote player's `UpdateAnimation()` currently has two states (moving → walk, stationary → idle). This extends to:

```
moving ──────────────────────────────► walk animation (from walkAnimId in PlayerStateMsg)
    │                                      ▲
    │                                      │ movement detected
    ▼                                      │
stationary ──(2.5s timeout)──► idle animation (from idleAnimId in PlayerStateMsg)
    │                              ▲
    │  MSG_EMOTE received          │ emote completed / movement detected
    ▼                              │
emote playing ─────────────────────┘
```

- **Walk/idle changes**: when `walkAnimId` or `idleAnimId` in an incoming `PlayerStateMsg` differs from the current value, look up the new animation via `world->Find("LegoAnimPresenter", tableLookup(index))`, build ROI map (cache it), swap in as the active walk/idle animation.
- **Emote** (`MSG_EMOTE`): look up animation presenter from emote table, build ROI map if not cached, play for one `GetDuration()` cycle. If movement is detected during playback, interrupt immediately and return to walk. After completion, return to stationary state (2.5s timeout → idle).

## Files Changed

| File | Change |
|------|--------|
| `extensions/include/extensions/multiplayer/protocol.h` | Add `walkAnimId`/`idleAnimId` to `PlayerStateMsg`, add `MSG_EMOTE` (9) and `EmoteMsg` struct |
| `extensions/src/multiplayer/networkmanager.cpp` | Include walk/idle IDs in state broadcasts, handle incoming `MSG_EMOTE`, add `SendEmote()` |
| `extensions/include/extensions/multiplayer/networkmanager.h` | Declare `SendEmote()`, walk/idle animation index storage |
| `extensions/src/multiplayer/remoteplayer.cpp` | Extended state machine: configurable walk/idle from PlayerStateMsg, emote playback with duration + interruption, animation ROI map caching |
| `extensions/include/extensions/multiplayer/remoteplayer.h` | Add animation state members (current walk/idle/emote anim, ROI maps, elapsed time, animation tables) |
| `extensions/src/multiplayer.cpp` | Add `mp_set_walk_animation()`, `mp_set_idle_animation()`, `mp_trigger_emote()`, `mp_get_player_count()` C exports |
| `ISLE/emscripten/config.cpp` | Remove hardcoded `"default"` room override |
| `CMakeLists.txt` | Add `_mp_set_walk_animation`, `_mp_set_idle_animation`, `_mp_trigger_emote`, `_mp_get_player_count` to `EXPORTED_FUNCTIONS` |
| `extensions/src/multiplayer/server/gameroom.ts` | Add HTTP room preview, configurable max players with ceiling enforcement, capacity check on WebSocket upgrade |
| `extensions/src/multiplayer/server/relay.ts` | Add `MAX_PLAYERS_CEILING` env config |
| isle.pizza `src/core/opfs.js` | Write `multiplayer:room` and `extensions:multiplayer` to INI |
| isle.pizza `src/App.svelte` | Read `#r/:name` from URL hash, write room config to INI before launch, mount overlay |
| isle.pizza: room creation UI | Max player selector, room name generation, navigate to `#r/NAME` |
| isle.pizza: new overlay component | Collapsible panel: walk/idle selectors, emote buttons with emoji popup, room info, copy link |
| isle.pizza: word lists module | Lego Island-themed adjective/color/noun word lists for room name generation |

## Implementation Priority

1. **Room name generation + URL routing** -- word lists, generate names in Svelte, read `#r/:name` from URL hash.
2. **INI config integration** -- write `multiplayer:room` and `extensions:multiplayer` to `isle.ini` via OPFS. Disable multiplayer for solo play. Remove hardcoded room from `config.cpp`.
3. **Configurable room size** -- max player selector in room creation UI, relay ceiling enforcement, capacity check on WebSocket upgrade.
4. **Animation protocol** -- add `walkAnimId`/`idleAnimId` fields to `PlayerStateMsg`, add `MSG_EMOTE` message type.
5. **Remote player animation state machine** -- extend `RemotePlayer::UpdateAnimation()` to read walk/idle IDs from state updates, emote playback with duration + interruption, animation ROI map caching.
6. **C++ exports** -- `mp_set_walk_animation()`, `mp_set_idle_animation()`, `mp_trigger_emote()`, `mp_get_player_count()` as WASM exports.
7. **Svelte overlay** -- collapsible panel with walk/idle animation selectors (persistent), emote buttons with emoji popup feedback, room info badge, copy link button.
8. **Room preview** -- relay HTTP endpoint returning player count and max, Svelte pre-join display.

## Open Questions

1. **Animation ROI map caching**: Remote players need ROI maps for each animation they play. Building maps is expensive. Need a caching strategy -- pre-build maps for all known animations on spawn, or lazily build and cache on first use?
2. **Room capacity UX**: When a player tries to join a full room, the relay rejects the WebSocket upgrade. The game calls `exit()`, which triggers the existing page reload in isle.pizza (`Module.onExit`). The frontend detects the error condition and shows a modal/popup (using existing isle.pizza styles) informing the player the room is full before returning to the main page.
