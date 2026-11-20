# World-overlay rendering audit

## Cause and fix

World geometry drawn through a private viewport must use the same eye camera,
asymmetric projection, and pixel viewport as the main world pass. Rendering the
eye camera alone is insufficient: `TCameraObject::Render()` installs the desktop
projection. Mixing that projection with the eye camera causes geometry to drift
relative to the scene as the headset turns. A desktop-sized pixel viewport also
misplaces/scales the draw in an eye-sized render target.

The remaining private world viewports were:

- `AGolfMinigameState::OnRenderOverlay`: the Gopher Golf aiming pointer **and all
  36 power-ring segments**, for every player.
- `ABarnYardPoolMiniGameState::OnRenderOverlay`: the pool cue rendered again over
  the table.

Both now use `AWorldOverlayRenderScope`. It drains pending parent draws before
switching context, matches the eye viewport while VR is rendering, renders the
active world camera, reapplies the eye projection (including updating its cached
matrix), and balances the transform stack. On exit it drains overlay draws before
restoring context, restores the private viewport's parameters, and rebinds the
main viewport without clearing colour or depth. The private depth-clear policy
is preserved, as is the ordinary camera projection outside an active VR eye.

The scope is for overlays called from the **main world viewport**, not GUI model
previews or render-to-texture passes. New depth-separated world geometry should
use this scope rather than duplicating viewport/camera setup.

## Audit coverage

The source audit covered viewport/context switches, camera rendering, explicit
projection changes, and all `OnRenderOverlay` overrides in `OpenBarnyard/Source`:

- The main and ordinary overlay passes in `ARenderer_Win.cpp` already reapply the
  eye projection after rendering the eye camera. Existing world shadows, hint
  markers (including sleep/save), animal markers, and replay markers remain on
  those paths rather than using a private desktop projection.
- Car Chase's overlay override only flushes shaders; it does not create a view.
- `AGUI2ModelRenderer` and `AGUI2ModelRendererScreen` render GUI model previews
  using their own cameras. These intentionally stay in the UI layer, as do HUD
  direction arrows, screen-edge pointers, and reticles. They must not be given
  a stereo world projection indiscriminately.
- Sun/moon/flare rendering already has a dedicated per-eye VR path; its desktop
  orthographic branch is not used during eye rendering.
- World VIS and terrain compute culling projections from the active context's
  parameters, rather than installing an independent desktop camera projection.

This is a source audit, not a claim that every game effect has been visually
verified in a headset.

## Validation

`python3 DevTools/Tests/test_world_overlay_scope.py` compiles the actual scope
implementation against a recording backend. It checks left/right-eye projection
cache updates, eye-sized viewport selection, desktop fallback, re-entry into VR,
flush context/projection ordering, balanced transforms, preserved clear policy,
and restoration of viewport, camera, projection and parent GPU viewport.

A full Windows DX11 build and headset regression test are still required:

1. In Gopher Golf, hold a ball, aim, then wind up; turn/tilt/translate the head.
   The pointer and every visible ring segment should remain at the gopher.
2. Repeat in both eyes and at non-default eye resolution/VR scale. Check that
   following world markers are not clipped to the desktop viewport.
3. In Barnyard Pool, rotate and move the head while aiming. The cue redraw must
   align with the world cue/table, without eye-dependent stretching or drift.
4. Recheck shadows, waypoint/hint and sleep/save markers, and desktop fallback.
   UI model previews and ordinary HUD elements should remain unchanged.
