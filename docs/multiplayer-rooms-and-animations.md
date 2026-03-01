# Phase 1: URL Rooms, Animation Triggers, and Svelte Overlay

## Context

The multiplayer backend works: Cloudflare Durable Objects relay, binary protocol (15Hz player state, world events), remote player rendering, host election. But there's no user-facing interface -- room ID is hardcoded to `"default"` in `ISLE/emscripten/config.cpp`, everyone auto-joins one room. The isle.pizza Svelte 5 frontend hosts the WASM game but has zero multiplayer screens.

The relay can be extended. Max 5 players per room (5 playable characters). No auth system.

## What We're Building

URL-based rooms + animation trigger system + Svelte overlay.

- **Room code in URL**: `isle.pizza/r/BRICK42`
- **Svelte overlay** with buttons that trigger animations on the local player (presented as "emotes" in the UI, but internally just animation triggers)
- **Room preview** on isle.pizza (player count before joining)
- **No text chat** -- emotes are the communication
- **Connection lifecycle handled by URL** at startup (no `mp_connect`/`mp_disconnect` from Svelte)

### Key Design Principle: Animations, Not Emotes

The UI presents these as "emotes" (Wave, Dance, Cheer, etc.), but the internal system is a **generic animation trigger mechanism**. The protocol, C++ exports, and network messages all deal in animation IDs -- not emote-specific concepts. This means:

- The protocol message carries an `animationId`, not an `emoteId`
- The C++ export is `mp_trigger_animation(int animationId)`, not `mp_trigger_emote`
- The mapping from UI label ("Wave") to animation ID lives entirely in the Svelte overlay
- New animations can be added without changing the protocol or C++ code

## User Flow

```
isle.pizza                → solo play
isle.pizza/r/BRICK42      → auto-joins room BRICK42
```

To play together:

1. Click "Create Room" → generates code → navigates to `/r/CODE`
2. Share the URL with friends
3. Friends open URL → auto-join the same room

## In-Game Overlay

```
┌──────────────── Game Canvas ─────────────────┐
│                                              │
│                                              │
│                          ┌────────────────┐  │
│                          │ Room: BRICK42  │  │
│                          │ Players: 3/5   │  │
│                          │ [Copy Link]    │  │
│                          │                │  │
│                          │ ── Emotes ──   │  │
│                          │ [Wave] [Dance] │  │
│                          │ [Cheer] [Bow]  │  │
│                          └────────────────┘  │
│                                              │
└──────────────────────────────────────────────┘
```

Clicking a button calls a C++ function that triggers an animation on the local player character. The animation state is synced to other players through the multiplayer protocol as a one-shot animation message.

## Svelte → WASM Bridge

One-way, minimal. The C++ side exports only what the Svelte overlay needs:

```cpp
extern "C" {
  // Trigger an animation on the local player by ID.
  // The mapping from UI emote labels to animation IDs lives in Svelte.
  EMSCRIPTEN_KEEPALIVE void mp_trigger_animation(int animationId);

  // Room info -- for the overlay to display
  EMSCRIPTEN_KEEPALIVE int mp_get_player_count();
}
```

No `mp_connect` / `mp_disconnect` -- the connection is established at WASM startup based on the room ID passed in via config. The Svelte frontend sets the room ID before WASM init:

```
URL → Svelte reads /r/BRICK42 → sets Module config → WASM starts → auto-connects to BRICK42
```

## Protocol: MSG_ANIMATION

A new one-shot message sent when any player triggers an animation:

```
MSG_ANIMATION = 9

struct AnimationMsg {
  MessageHeader header;    // 9 bytes (type + peerId + sequence)
  uint8_t animationId;     // 1 byte
};
```

Total: 10 bytes per animation trigger.

The relay broadcasts it to all peers (it already broadcasts unknown message types). Remote clients receive the message and play the corresponding animation on that peer's character.

### Why a One-Shot Message (Not Player State)

Animations are discrete events, not continuous state. Piggybacking on the 15Hz `PlayerStateMsg` would mean:

- Wasting bandwidth sending an animation field every 66ms even when idle
- Needing "started at" timestamps or frame counters to avoid replaying
- Difficulty distinguishing "no animation" from "animation just ended"

A one-shot message is cleaner: fire once, play once, done.

### Animation ID Space

The `animationId` field is a `uint8_t`, giving 256 possible animations. The initial set depends on what animations the original game characters already have. Potential candidates:

| ID | UI Label | Animation Source | Notes |
|----|----------|-----------------|-------|
| 0  | Wave     | Arm wave        | May need new animation or repurpose existing |
| 1  | Dance    | Character dance  | Original game has some dance animations |
| 2  | Cheer    | Jump + arms up  | Could reuse victory animations |
| 3  | Bow      | Forward lean    | May need new animation |
| 4  | Point    | Point forward   | Reuse directing animation |

The exact set requires investigation of available animation assets. The protocol supports adding more at any time -- just add a new ID mapping in the Svelte overlay and a handler on the C++ side.

## Room ID Configuration

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

The Svelte frontend reads the room code from the URL path (`/r/BRICK42`) and passes it to the WASM module before initialization. The default remains `""` (empty = solo play, no connection).

The Emscripten config override should read the room from a JS `Module` property:

```cpp
// In Emscripten_SetupDefaultConfigOverrides():
EM_ASM({
  var room = Module['multiplayerRoom'] || '';
  if (room) {
    stringToUTF8(room, $0, 64);
  } else {
    stringToUTF8('', $0, 64);
  }
}, roomBuf);
iniparser_set(p_dictionary, "multiplayer:room", roomBuf);
```

The relay URL can follow the same pattern, or remain hardcoded to the production relay.

### Connection Logic

- **Empty room** (`""` or missing): multiplayer extension skips initialization entirely (current behavior when `room.empty()` returns true)
- **Non-empty room**: connect to relay at `/room/{roomId}` as currently implemented

No `mp_connect`/`mp_disconnect` exports needed. The connection is established once at startup and torn down when the page unloads.

## Relay: Room Preview Endpoint

Add an HTTP response to the Durable Object for non-WebSocket requests, so the Svelte UI can show player count before joining:

```typescript
// In GameRoom.fetch(), before WebSocket upgrade:
if (request.headers.get("Upgrade") !== "websocket") {
  return new Response(JSON.stringify({
    players: this.connections.size,
    maxPlayers: 5
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

When `mp_trigger_animation(animationId)` is called:

1. Look up the animation for the current player character
2. Play the animation locally (using existing animation system)
3. Send `MSG_ANIMATION` with the `animationId` to all peers via the relay

When a remote `MSG_ANIMATION` is received:

1. Look up the remote player by `peerId` from the message header
2. Look up the animation for that character
3. Play the animation on the remote player's character model

The animation lookup and playback depend on the existing animation assets and system (`LegoROI::ApplyAnimationTransformation()` for remote players, or triggering through the entity system for the local player). The exact mechanism needs investigation of available character animations.

## Files Changed

| File | Change |
|------|--------|
| `extensions/include/extensions/multiplayer/protocol.h` | Add `MSG_ANIMATION` (type 9) and `AnimationMsg` struct |
| `extensions/src/multiplayer/networkmanager.cpp` | Handle incoming `MSG_ANIMATION`, add `SendAnimation(uint8_t animationId)` method |
| `extensions/include/extensions/multiplayer/networkmanager.h` | Declare `SendAnimation()` |
| `extensions/src/multiplayer.cpp` | Add `mp_trigger_animation()` and `mp_get_player_count()` C exports |
| `ISLE/emscripten/config.cpp` | Read `multiplayerRoom` from JS `Module` property instead of hardcoding `"default"` |
| `CMakeLists.txt` | Add `mp_trigger_animation`, `mp_get_player_count` to `EXPORTED_FUNCTIONS` |
| `extensions/src/multiplayer/server/gameroom.ts` | Add HTTP response for non-WebSocket requests (room preview) |
| isle.pizza `src/App.svelte` | Read `/r/:code` from URL, pass room to WASM `Module` config, mount overlay |
| isle.pizza: new overlay component | Emote buttons that call `Module.ccall('mp_trigger_animation', ...)`, room info, copy link |

## Implementation Priority

1. **URL rooms** -- read room ID from URL in Svelte, pass to WASM via `Module` property, connect on startup. Solo play when no room in URL.
2. **Animation trigger protocol** -- `MSG_ANIMATION` message type, `AnimationMsg` struct, send/receive in `NetworkManager`.
3. **C++ animation exports** -- `mp_trigger_animation()` that plays locally and broadcasts, `mp_get_player_count()` for overlay display.
4. **Svelte overlay** -- emote buttons calling `mp_trigger_animation`, room info badge, copy link button.
5. **Room preview** -- relay HTTP endpoint returning player count, Svelte pre-join display.

## Open Questions

1. **Available animations**: What character animations exist in the original game assets? The emote set depends entirely on this. Need to investigate `LegoCharacterManager`, animation trees, and existing animation data files.
2. **Animation playback for remote players**: Remote players use `LegoROI::ApplyAnimationTransformation()` with static animation data. Can triggered animations be played through this system, or do they need a different approach? The current remote player rendering applies walking animations at the network tick rate -- triggered animations may need to run for a fixed duration and then return to the idle/walking state.
3. **Animation interruption**: What happens if a player triggers an animation while walking? Should movement stop, or should the animation play on top of movement? The local player's path actor state may need to be considered.
4. **Room code generation**: What format for generated room codes? Short alphanumeric strings (e.g., `BRICK42`, `SURF99`) are memorable and URL-safe. A simple random generator in Svelte is sufficient.
5. **Room capacity enforcement**: The relay should reject WebSocket upgrades when a room has 5 players. Currently the relay doesn't enforce this -- needs a connection count check in `GameRoom.fetch()`.

---

## Future Layers (Reference)

These build on URL rooms. Each layers on the previous:

### Layer 1: Public Lobby Browser

Browse active rooms. Requires cross-DO aggregation (Cloudflare KV or D1) for room listing.

```
┌────────────────────────────────────┐
│  PUBLIC ROOMS          [Create]    │
│  ┌──────────────────────────────┐  │
│  │ BRICK42   ●●●○○  3/5  [Join]│  │
│  └──────────────────────────────┘  │
│  ┌──────────────────────────────┐  │
│  │ SURF99    ●○○○○  1/5  [Join]│  │
│  └──────────────────────────────┘  │
└────────────────────────────────────┘
```

**Relay extensions needed**: KV writes on join/leave (with TTL), Worker endpoint to list rooms, room visibility flag (public/private).

### Layer 2: Quick Match / Auto-Join

One button, auto-join an open room. Requires same infra as lobby browser. Best with a large active player base.

### Layer 3: Persistent Social Lobby

Global chat room at isle.pizza. See who's online, organize games. Separate Durable Object for lobby chat (independent of game rooms).

### Layer 4: Spatial Island Lobby

The game world IS the lobby. Everyone loads into a shared overworld, walks up to activities. Most immersive, most complex. Long-term vision.

### Composition

```
Layer 0 (Foundation):  URL Rooms + Animation Triggers  ← building this now
Layer 1 (Discovery):   Lobby Browser / Quick Match
Layer 2 (Social):      Persistent Lobby + Chat
Layer 3 (Immersive):   Spatial Island Lobby
```
