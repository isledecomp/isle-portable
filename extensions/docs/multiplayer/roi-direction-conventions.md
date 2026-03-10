# ROI Direction Conventions & Third-Person Camera

## Background: The Two Z-Axis Conventions

The game engine represents an actor's facing direction via the z-axis of its ROI
(Real-time Object Instance) local-to-world transform. Two opposite conventions
exist throughout the codebase:

| Convention     | ROI z-axis points...     | Used by                                                |
|----------------|--------------------------|--------------------------------------------------------|
| **forward-z**  | Toward visual forward    | `PlaceActor` (with `m_cameraFlag=TRUE`), cam anim end |
| **backward-z** | Away from visual forward | After `Enter()`'s `TurnAround()`, vehicle ROIs         |

Toggling between conventions is done by `IslePathActor::TurnAround` (or the
local `FlipMatrixDirection` helper): negate the z-axis and recompute the right
vector.

## Design Choice: Forward-Z

The third-person orbit camera uses **forward-z**, matching the convention that
`PlaceActor` naturally produces. This eliminates the need to flip the ROI
direction after every `PlaceActor` call.

`ComputeOrbitVectors` treats local Z+ as the character's visual forward and
places the camera at local −Z (behind the character), looking toward +Z.

## Engine Behavior

The engine's actor lifecycle:

```
Enter()
  → ResetWorldTransform(TRUE)   sets m_cameraFlag = TRUE
  → TurnAround()                flips to backward-z
  → TransformPointOfView()      sets 1st-person camera

PlaceActor()                     resets to forward-z  ← what we use
```

`PlaceActor` always produces forward-z (when `m_cameraFlag=TRUE`), which is
exactly what the orbit camera expects. No direction correction is needed after
`PlaceActor` runs.

## World Transition Timing

The one remaining complexity is timing during world transitions. The event order
is:

1. `OnWorldEnabled` fires (from `LegoWorld::Enable`, BEFORE `SpawnPlayer`)
2. `ReinitForCharacter` sets up the display ROI and marks `m_active = true`
3. `Enter()` fires `OnActorEnter` — ROI is at **stale position** from previous session
4. `PlaceActor` sets ROI to correct spawn position
5. First `Tick` — `ApplyOrbitCamera` sets the camera at the correct position

Between steps 3 and 5, the ROI position is stale. If we set up the orbit camera
in step 3, the stale view would freeze on screen during the ~500ms world load.

The `m_pendingWorldTransition` flag handles this: set in `OnWorldEnabled`,
it causes `OnActorEnter` and `ReinitForCharacter` to skip camera setup.
Cleared in the first `Tick` after `PlaceActor`, where `ApplyOrbitCamera`
naturally handles the camera. The orbit state (yaw, pitch, distance) is also
reset to defaults in `OnWorldEnabled`.

## Display Clone Direction

The native actor ROI is invisible in 3rd-person mode. A display clone renders
the character model instead. Character meshes face −z, so the clone needs
backward-z to look correct. When syncing the clone's transform from the native
ROI (which is in forward-z), `Tick` negates the z-axis and recomputes the right
vector — the same operation as `TurnAround`.

This also affects the right vector (X-axis): forward-z and backward-z produce
opposite right vectors. The orbit yaw input is negated to compensate, keeping
drag-right = camera-moves-right.

## Cam Anim Interaction

While a cam anim locks the player (`GetActorState() == c_disabled`), two
things protect the orbit camera:

1. **Tick guard**: `ApplyOrbitCamera` is skipped so it doesn't fight the cam
   anim's `TransformPointOfView`. Without this, the cam anim end handler would
   read our elevated orbit camera position and place the actor in the air.

2. **`OnCamAnimEnd`**: When the cam anim releases the player (first space bar
   interruption or natural end), this callback calls `SetupCamera` to restore
   the orbit camera.

After the first interruption, the actor state resets to `c_initial` and the
orbit camera resumes immediately — even if `m_animRunning` is still true for
background animations playing in the world.

## Network Direction

The network protocol sends forward-z direction (visual forward). Remote players
negate the received direction to backward-z for their ROI, since character meshes
face −z.
