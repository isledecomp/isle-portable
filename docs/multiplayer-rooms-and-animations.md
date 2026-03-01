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

### Key Design Principle: Animations, Not Emotes

The UI presents these as "emotes", but the internal system is a **generic animation trigger mechanism**. The protocol, C++ exports, and network messages all deal in animation names -- not emote-specific concepts. This means:

- The protocol message carries an animation name string, not an emote-specific concept
- The C++ export is `mp_trigger_animation(const char* animName)`, not `mp_trigger_emote`
- The mapping from UI label to animation name lives entirely in the Svelte overlay
- New animations can be added without changing the protocol

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

Clicking an emote button broadcasts an animation to all other players through the multiplayer protocol as a one-shot message. The animation is **not played on the local player** -- it is only visible to remote players. This avoids interfering with the local player's path actor state and movement controls.

## Svelte → WASM Bridge

One-way, minimal. The C++ side exports only what the Svelte overlay needs:

```cpp
extern "C" {
  // Broadcast an animation to all other players by its CNs name
  // (e.g., "CNs003xx"). The animation is NOT played on the local player --
  // it is only visible to remote players. The mapping from UI emote label
  // to animation name lives in the Svelte overlay.
  EMSCRIPTEN_KEEPALIVE void mp_trigger_animation(const char* animName);

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

The emote system should use `ApplyAnimationTransformation()` since it already works for remote players.

### CNs### Locomotion/Gesture Animations (g_cycles)

The `g_cycles[11][17]` array in `legoanimationmanager.cpp` maps character types to animation names. Each character has character-specific variants (e.g., `CNs001Pe` for Pepper), but **generic `xx` versions exist that work on any character** since all characters share the same skeleton structure.

Remote players already use two of these:
- `CNs001xx` -- walk cycle
- `CNs008xx` -- idle/breathing

The remaining generic animations are available for emotes. Their exact visual content needs to be identified by playing them in-game:

| Slot | Name | Status |
|------|------|--------|
| 0 | `CNs001xx` | Walk cycle (already used) |
| 1 | `CNs002xx` | **Available for emotes** -- visual TBD |
| 2 | `CNs003xx` | **Available for emotes** -- visual TBD |
| 3 | `CNs004xx` | **Available for emotes** -- visual TBD |
| 4 | `CNs005xx` | **Available for emotes** -- visual TBD |
| 5 | `CNs007xx` | **Available for emotes** -- visual TBD |
| 6 | `CNs006xx` | **Available for emotes** -- visual TBD |
| 7 | `CNs008xx` | Idle/breathing (already used) |
| 8 | `CNs009xx` | **Available for emotes** -- visual TBD |
| 9 | `CNs010xx` | **Available for emotes** -- visual TBD |
| 10 | `CNs011xx` | **Available for emotes** -- visual TBD |
| 11 | `CNs012xx` | **Available for emotes** -- visual TBD |

These are the NPC idle/gesture animations that play when NPCs are near the player, selected by the mood system (moods 0-3). They include fidgets, gestures, looking around, etc. All are loaded as `LegoAnimPresenter` objects in the world and can be looked up via `world->Find("LegoAnimPresenter", "CNs003xx")`.

**Playback for remote players**: Same pattern already used for walk/idle -- call `BuildROIMap()` to map animation bones to the character's ROI, then `ApplyAnimationTransformation()` each frame with an advancing time value. The animation runs for its `GetDuration()` then returns to idle.

### Click/Customize Animation (Body Flip)

When clicking on an NPC, `ClickAnimation()` (`legoentity.cpp:309`) plays a body flip animation:

1. Gets animation ID: `m_move + g_characterAnimationId` (IDs 10-13, 4 variants)
2. Loads from a separate SI file (`CUSTOMIZE_ANIM_FILE`)
3. Uses `SUBST:actor_01:characterName` to apply to any character
4. Played via `StartEntityAction()` (presenter pipeline)

This animation works on any character via the SUBST mechanism, but uses the presenter system which isn't available for remote players. To use this as an emote, the animation data would need to be extracted and played via `ApplyAnimationTransformation()` instead -- or the presenter system would need to be extended.

### Disassemble/Reassemble (BNsDis/BNsAss)

Action IDs in Isle: `BNsDis01`-`03` (231-233), `BNsAss01`-`03` (228-230). Character falls apart into pieces then reassembles. Triggered by NPC collision in `LegoExtraActor::HitActor()`. Also uses the presenter system -- same portability challenge as the click animation.

### Procedural Flip

Not an animation file -- pure code in `LegoExtraActor::StepState()`. Applies `RotateX`/`RotateZ` at 0.7 rad/frame for 2 seconds based on a collision axis. Triggered by NPC collision at certain angles.

**Easiest to port**: Since it's just transform math, it can be replicated for remote players by applying the same rotation transforms. No animation data or presenter system needed.

### Emote Implementation Strategy

**Phase 1 emotes** (use `ApplyAnimationTransformation()` -- already proven for remote players):

1. Pick the best-looking CNs###xx animations after visual review (up to 10 candidates)
2. Optionally add the procedural flip as a special emote (code-driven, no animation data)

**Future emotes** (would require extending the presenter system for remote players):

3. Click animation body flip (from CUSTOMIZE_ANIM_FILE)
4. Disassemble/reassemble (BNsDis/BNsAss)

## Protocol: MSG_ANIMATION

A new one-shot message sent when any player triggers an animation:

```
MSG_ANIMATION = 9

struct AnimationMsg {
  MessageHeader header;      // 9 bytes (type + peerId + sequence)
  uint8_t nameLength;        // 1 byte
  char animName[nameLength]; // variable length (e.g., "CNs003xx" = 8 bytes)
};
```

The animation is identified by its `CNs` name string. This matches how animations are looked up internally (`world->Find("LegoAnimPresenter", name)`) and avoids needing an ID mapping layer. Name strings are short (8-10 bytes typically), so the overhead vs. a fixed integer ID is minimal.

For the procedural flip, a reserved name like `"_flip"` can be used (the `_` prefix distinguishes it from CNs names).

The relay broadcasts it to all peers (it already broadcasts unknown message types). Remote clients receive the message and play the corresponding animation on that peer's character.

### Why a One-Shot Message (Not Player State)

Animations are discrete events, not continuous state. Piggybacking on the 15Hz `PlayerStateMsg` would mean:

- Wasting bandwidth sending an animation field every 66ms even when idle
- Needing "started at" timestamps or frame counters to avoid replaying
- Difficulty distinguishing "no animation" from "animation just ended"

A one-shot message is cleaner: fire once, play once, done.

### Animation Interruption

If a player moves while an emote/animation is playing, the animation is **immediately cleared/interrupted**. Movement always takes priority. This keeps the gameplay responsive and prevents players from getting stuck in animation states.

On the local player: any path actor movement input cancels the active triggered animation and returns to normal movement animation.

On remote players: when a `PlayerStateMsg` arrives indicating movement (non-zero speed), any active triggered animation on that remote player is cancelled.

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

## C++ Implementation: Animation Trigger

When `mp_trigger_animation(animName)` is called:

1. Send `MSG_ANIMATION` with the animation name to all peers via the relay
2. Nothing happens locally -- the animation is only visible to other players

When a remote `MSG_ANIMATION` is received:

1. Look up the remote player by `peerId` from the message header
2. If CNs name: look up the animation presenter via `world->Find("LegoAnimPresenter", animName)`, build ROI map if not cached, play the animation for its duration then return to idle
3. If `"_flip"`: apply procedural rotation on the remote player's transform for 2 seconds (same math as `LegoExtraActor::StepState()` `c_hitAnimation`)

The remote player's `UpdateAnimation()` already handles walk vs. idle selection. A triggered animation adds a third state: when active, it takes priority over both walk and idle. When it completes (duration elapsed) or is interrupted (movement detected), it returns to the normal walk/idle logic.

## Files Changed

| File | Change |
|------|--------|
| `extensions/include/extensions/multiplayer/protocol.h` | Add `MSG_ANIMATION` (type 9) and `AnimationMsg` struct |
| `extensions/src/multiplayer/networkmanager.cpp` | Handle incoming `MSG_ANIMATION`, add `SendAnimation(const char* animName)` method |
| `extensions/include/extensions/multiplayer/networkmanager.h` | Declare `SendAnimation()` |
| `extensions/src/multiplayer/remoteplayer.cpp` | Add triggered animation state, animation playback with duration, interruption on movement, procedural flip support |
| `extensions/include/extensions/multiplayer/remoteplayer.h` | Add triggered animation members (current anim, ROI map, elapsed time, flip state) |
| `extensions/src/multiplayer.cpp` | Add `mp_trigger_animation()` and `mp_get_player_count()` C exports |
| `ISLE/emscripten/config.cpp` | Remove hardcoded `"default"` room override |
| `CMakeLists.txt` | Add `mp_trigger_animation`, `mp_get_player_count` to `EXPORTED_FUNCTIONS` |
| `extensions/src/multiplayer/server/gameroom.ts` | Add HTTP room preview, configurable max players with ceiling enforcement, capacity check on WebSocket upgrade |
| `extensions/src/multiplayer/server/relay.ts` | Add `MAX_PLAYERS_CEILING` env config |
| isle.pizza `src/core/opfs.js` | Write `multiplayer:room` and `extensions:multiplayer` to INI |
| isle.pizza `src/App.svelte` | Read `#r/:name` from URL hash, write room config to INI before launch, mount overlay |
| isle.pizza: room creation UI | Max player selector, room name generation, navigate to `#r/NAME` |
| isle.pizza: new overlay component | Collapsible panel: emote buttons calling `mp_trigger_animation`, room info, copy link |
| isle.pizza: word lists module | Lego Island-themed adjective/color/noun word lists for room name generation |

## Implementation Priority

1. **Room name generation + URL routing** -- word lists, generate names in Svelte, read `#r/:name` from URL hash.
2. **INI config integration** -- write `multiplayer:room` and `extensions:multiplayer` to `isle.ini` via OPFS. Disable multiplayer for solo play. Remove hardcoded room from `config.cpp`.
3. **Configurable room size** -- max player selector in room creation UI, relay ceiling enforcement, capacity check on WebSocket upgrade.
4. **Visual review of CNs animations** -- play each CNs002xx-CNs012xx in-game to identify what they look like. Select the best candidates for emotes and assign UI labels.
5. **Animation trigger protocol** -- `MSG_ANIMATION` message type, send/receive in `NetworkManager`.
6. **C++ animation trigger** -- `mp_trigger_animation()` broadcasts to peers (no local playback). Triggered animation state in `RemotePlayer` with duration-based playback and movement interruption. Procedural flip support.
7. **C++ exports** -- `mp_trigger_animation()` and `mp_get_player_count()` as WASM exports.
8. **Svelte overlay** -- collapsible panel with emote buttons calling `mp_trigger_animation`, room info badge, copy link button.
9. **Room preview** -- relay HTTP endpoint returning player count and max, Svelte pre-join display.

## Open Questions

1. **Visual identification of CNs animations**: The 10 generic CNs animations (CNs002xx-CNs012xx, excluding walk and idle) need to be played in-game to determine what they look like visually. Only after this can we assign emote labels and select the best subset for the UI.
2. **Autonomous animation playback for remote players**: The current `RemotePlayer::UpdateAnimation()` uses a simple moving/idle state machine. Adding a third "triggered animation" state is straightforward in concept (play for duration, then return to idle), but needs to handle: animation caching (avoid rebuilding ROI maps each trigger), smooth transitions back to idle, and the procedural flip as a special case. Needs prototyping.
3. **Room capacity UX**: When a player tries to join a full room, the relay rejects the WebSocket upgrade. The game calls `exit()`, which triggers the existing page reload in isle.pizza (`Module.onExit`). The frontend detects the error condition and shows a modal/popup (using existing isle.pizza styles) informing the player the room is full before returning to the main page.
