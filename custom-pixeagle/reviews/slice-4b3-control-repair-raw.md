# Slice 4b.3 control repair — raw AI visual review

Reviewer: independent AI agent taking an operator-workflow perspective.
This is not feedback from a human operator, a blinded usability study, or
physical-camera acceptance. The reviewer previously inspected the control code,
so implementation knowledge may influence interpretation.

## Evidence inspected

1440×900 screenshots under
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b3-2026-10-01/control-ui/`:

- `03.png`: neutral joystick, readings collapsed.
- `04-readings.png`: readings expanded; Camera readings tooltip visible.

Both show the synthetic camera fixture and no aircraft. The tracker label says
CSRT (mock fixture). No hardware commands were issued for this review. These
images do not demonstrate the camera-owned Classic/Smart profile, aircraft
following, or real telemetry.

## Raw observations

The panel reads as a camera controller immediately: a short PixEagle camera
heading, one dominant circular pad, zoom beside it, and a direct Stop action.
It occupies the upper-right edge without dimming the video or covering its
center. This is a clearer operational arrangement than a centered settings
form. The neutral thumb visibly sits at the center of the rings.

Roll is now visible in both states. I do not have to discover it behind the
ellipsis. The two curved arrows make sense as opposite rotations, but their
small glyphs alone do not establish which camera axis they rotate. Existing
Roll left/right accessible names and tooltips help desktop use. A touch user
still needs the hands-on check; these screenshots do not prove touch sizing or
direction comprehension. I would not add a permanent explanatory paragraph.

The ellipsis expands a small, aligned list below the controls. Yaw 12.5°, pitch
−8.0°, and roll 2.0° have explicit degree units. Camera body angles is an
important qualification: these are not presented as aircraft heading, earth
angles, or a validated vertical-mount transform. No zoom number appears, which
is appropriate when the provider does not report measured zoom. The numbers
are fixture values, not measurements from the connected camera.

Readings fit without clipping in this desktop capture. They are secondary in
size and placement; the joystick remains dominant. The white tooltip partially
covers the pad while hovering over the ellipsis, but it is transient and names
the action clearly. This does not justify another visual redesign.

## Concerns retained for the operator checkpoint

1. **Interaction expectation, medium:** the circular pad suggests two-axis
   motion, whereas the current provider intentionally uses one dominant axis.
   Proportional speed is implemented, but static images cannot demonstrate it.
   Check slow diagonal drags and small adjustments with the operator before
   describing the controller as naturally intuitive.

2. **Targeting transition, medium:** the separate Classic/Smart target widget
   remains visible while camera controls are open. The images do not explain
   that target selection is paused during camera control. Ask whether closing
   the controller to select a target is discoverable. Keep any resulting hint
   contextual; do not fill the video with persistent instructions.

3. **Freshness, unverified:** expanded readings look live, but there is no
   stale-state screenshot here. Source inspection shows expired readings should
   become dashes with Readings unavailable. The screenshot supports the live
   formatting only; tests and a disconnect check must establish stale behavior.

4. **Stop, unverified:** Stop is disabled at neutral in both captures. That is
   plausible with no owned motion. These images cannot establish that it remains
   reachable during a delayed command, failed Stop, or stale telemetry.

5. **Profile and follower visibility, not demonstrated:** the mock shows local
   CSRT and no follower row. It does not provide acceptance evidence for the
   planned camera-owned startup selection or Bench: following disabled state.
   Verify those on the actual handoff profile rather than interpreting the
   fixture screenshot as proof they are absent or working.

6. **Small displays and real imagery, unverified:** this large desktop mock
   cannot establish readability under sunlight, finger accuracy, narrow-window
   behavior, long-error layout, or how much a real moving target is obscured.

## Disposition

The neutral and readings layouts are suitable for the next operator checkpoint.
Always-visible roll and compact optional readings address the requested layout
changes without increasing central video obstruction. No screenshot finding
requires a new cosmetic redesign. Retain the interaction and evidence gaps
above; physical control behavior and human acceptance remain separate gates.

## Follow-up: camera-v2 read-only desktop captures

Additional evidence under the same checkpoint's `desktop-v2/` directory:
`06-camera-idle.png`, `07-options.png`, and `08-panel.png`. The operator camera
feed is visible. The capture session is reported as read-only: no target
selection or motor movement was issued. This remains an AI screenshot review,
not physical control acceptance.

The idle widget now clearly distinguishes Camera Classic from Camera Smart;
Camera Classic is visibly selected. The follower row shows GM PID Pursuit,
Disabled, and Bench: following disabled. This resolves the earlier mock's
profile/follower visibility gap at the presentation level. It does not prove
tracker acquisition, camera AI detections, follower compatibility, or flight
behavior.

The options dialog keeps Tracking engine and the disabled follower selection
together, with Settings, Dashboard, and Camera controls reachable without a
long text explanation. Its layout is readable and has no visible clipping.
The image does not show the engine dropdown's alternatives, so switching back
to local trackers still requires the interaction test.

The controller remains legible against real camera imagery. Its upper-right
position leaves the central image clear, and roll controls remain directly
visible. The normal QGC Disconnected banner coexists with live camera video;
the handoff should continue to explain that this means no aircraft connection.
No new visual blocker appears in these captures. Retain the previous checks
for dominant-axis feel, targeting transitions, release/Stop, stale readings,
and touch usability. Physical acceptance remains pending.
